// System link on host Winsock. The game reaches the network only through the XNet winsock thunks, replaced
// here. The XNet object [0x5E7784] stays null, so the XNet calls not replaced (Live QoS and the like) fail with
// WSANOTINITIALISED on their own, and the XOnline calls with XONLINE_E_NOT_INITIALIZED.
// Addresses: an XNADDR's ina is the peer's real IPv4, so secure addresses map to IPs one to one.
// With --instance N the game binds 127.0.0.N and a broadcast goes to the other loopback instances.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <random>

#include "app/app.h"
#include "xdk/hle.h"

using kernel::HostPtr;
using kernel::R32;
using kernel::W32;

namespace {

// XNADDR: ina, inaOnline, wPortOnline, abEnet[6], abOnline[20]
constexpr uint32_t kXnAddrSize = 0x24;
constexpr uint32_t kXnAddrEnet = 0x0A;
// WSAOVERLAPPED: Internal, InternalHigh, Offset, OffsetHigh, hEvent
constexpr uint32_t kOvlStatus = 0x00;
constexpr uint32_t kOvlBytes = 0x04;
constexpr uint32_t kOvlEvent = 0x10;
constexpr uint32_t kOvlPending = 0x103;
// XNDNS: iStatus, cina, aina[8]
constexpr uint32_t kXnDnsSize = 0x28;
// fd_set: fd_count, fd_array[64]
constexpr uint32_t kFdSetMax = 64;

constexpr uint32_t kSocketError = 0xFFFFFFFF;
constexpr uint32_t kLinkUp = 0x01 | 0x02 | 0x08;          // active, 100 Mbps, full duplex
constexpr uint32_t kXnAddrEthernet = 0x02 | 0x04;         // ethernet, static address
constexpr DWORD kUdpConnReset = 0x9800000C;  // SIO_UDP_CONNRESET: off, or a send to a closed port fails the next recv
constexpr int kLoopbackInstances = 8;

struct Socket {
    SOCKET s = INVALID_SOCKET;
    bool udp = false;
    bool nonblocking = false;  // the guest's FIONBIO; the host socket is always nonblocking
};

struct Span {
    uint32_t ptr, len;
};

// the byte count and flags pointers are only written when the call completes at once, as in Winsock:
// Quazal passes its flags on the stack
struct PendingRecv {
    uint32_t sock, ovl, from, fromlen;
    std::vector<Span> spans;  // copied: Quazal's WSABUF array lives on its stack
};

std::mutex g_lock;
std::unordered_map<uint32_t, Socket> g_sockets;
uint32_t g_next_socket = 0x100;
std::vector<PendingRecv> g_pending;

std::once_flag g_init;
int g_instance = 0;
uint32_t g_local = 0;  // network order
std::array<uint8_t, 8> g_kid{};
std::unordered_map<uint32_t, std::array<uint8_t, kXnAddrSize>> g_peers;

thread_local uint32_t t_error = 0;

std::string Ip(uint32_t a) {
    char s[32];
    std::snprintf(s, sizeof(s), "%u.%u.%u.%u", a & 0xFF, (a >> 8) & 0xFF, (a >> 16) & 0xFF, a >> 24);
    return s;
}
std::string Addr(const sockaddr_in& a) { return Ip(a.sin_addr.s_addr) + ":" + std::to_string(ntohs(a.sin_port)); }

uint32_t PrimaryAddress() {
    uint32_t addr = htonl(INADDR_LOOPBACK);
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(53);
    to.sin_addr.s_addr = htonl(0x08080808);  // no packet is sent: connect only picks the route
    sockaddr_in me{};
    int len = sizeof(me);
    if (s != INVALID_SOCKET && ::connect(s, reinterpret_cast<sockaddr*>(&to), sizeof(to)) == 0 &&
        ::getsockname(s, reinterpret_cast<sockaddr*>(&me), &len) == 0 && me.sin_addr.s_addr)
        addr = me.sin_addr.s_addr;
    if (s != INVALID_SOCKET) ::closesocket(s);
    return addr;
}

void Pump();

void Init() {
    std::call_once(g_init, [] {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        g_instance = app::Instance();
        g_local = g_instance ? htonl(0x7F000000u | uint32_t(g_instance)) : PrimaryAddress();
        LOGI("xnet: %s address %s", g_instance ? "loopback instance" : "LAN", Ip(g_local).c_str());
        std::thread(Pump).detach();
    });
}

uint32_t Fail(int err) {
    t_error = uint32_t(err);
    return kSocketError;
}
int HostError() {
    int e = WSAGetLastError();
    return e == WSAECONNRESET ? WSAEWOULDBLOCK : e;  // a port unreachable reply from an earlier send
}

Socket Find(uint32_t sock) {
    std::lock_guard<std::mutex> lock(g_lock);
    auto it = g_sockets.find(sock);
    return it == g_sockets.end() ? Socket{} : it->second;
}

sockaddr_in GuestAddr(uint32_t va) {
    sockaddr_in a{};
    std::memcpy(&a, HostPtr(va), sizeof(a));
    return a;
}

void Signal(uint32_t event) {
    if (!event) return;
    if (auto o = kernel::FromHandle(event)) kernel::SetEvent(*o);
}

bool Wait(SOCKET s, bool write, int ms) {
    fd_set f;
    FD_ZERO(&f);
    FD_SET(s, &f);
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    return ::select(0, write ? nullptr : &f, write ? &f : nullptr, nullptr, &tv) > 0;
}

std::vector<Span> Spans(uint32_t bufs, uint32_t count) {
    std::vector<Span> v;
    for (uint32_t i = 0; i < count; i++) v.push_back({R32(bufs + 8 * i + 4), R32(bufs + 8 * i)});
    return v;
}

// one datagram into guest buffers: bytes, or -1 with err
int Recv(uint32_t sock, SOCKET s, const std::vector<Span>& spans, uint32_t from, uint32_t fromlen, int& err) {
    thread_local std::vector<uint8_t> buf(65536);
    sockaddr_in src{};
    int sl = sizeof(src);
    int n = ::recvfrom(s, reinterpret_cast<char*>(buf.data()), int(buf.size()), 0, reinterpret_cast<sockaddr*>(&src), &sl);
    if (n < 0) {
        err = HostError();
        if (err != WSAEWOULDBLOCK) LOG_FIRST(20, "xnet: recv %X failed (%d)", sock, err);
        return -1;
    }
    int done = 0;
    for (const Span& sp : spans) {
        int c = std::min<int>(n - done, int(sp.len));
        if (c <= 0) break;
        std::memcpy(HostPtr(sp.ptr), buf.data() + done, size_t(c));
        done += c;
    }
    if (done < n) LOG_FIRST(10, "xnet: %d byte datagram cut to %d", n, done);
    if (from) std::memcpy(HostPtr(from), &src, sizeof(src));
    if (fromlen) W32(fromlen, sizeof(src));
    return done;
}

int SendOnce(SOCKET s, const uint8_t* data, int len, const sockaddr_in* to) {
    if (!to) return ::send(s, reinterpret_cast<const char*>(data), len, 0);
    sockaddr_in d = *to;
    if (g_instance && d.sin_addr.s_addr == INADDR_BROADCAST) {
        for (int i = 1; i <= kLoopbackInstances; i++) {
            if (i == g_instance) continue;
            d.sin_addr.s_addr = htonl(0x7F000000u | uint32_t(i));
            ::sendto(s, reinterpret_cast<const char*>(data), len, 0, reinterpret_cast<sockaddr*>(&d), sizeof(d));
        }
        return len;
    }
    return ::sendto(s, reinterpret_cast<const char*>(data), len, 0, reinterpret_cast<const sockaddr*>(&d), sizeof(d));
}

int Send(uint32_t sock, SOCKET s, const uint8_t* data, int len, const sockaddr_in* to) {
    int n = SendOnce(s, data, len, to);
    if (n < 0) LOG_FIRST(20, "xnet: send %X failed (%d)", sock, WSAGetLastError());
    return n;
}

std::vector<uint8_t> Gather(uint32_t bufs, uint32_t count) {
    std::vector<uint8_t> v;
    for (const Span& sp : Spans(bufs, count)) {
        auto p = static_cast<const uint8_t*>(HostPtr(sp.ptr));
        v.insert(v.end(), p, p + sp.len);
    }
    return v;
}

void Complete(uint32_t ovl, uint32_t status, uint32_t bytes) {
    W32(ovl + kOvlBytes, bytes);
    W32(ovl + kOvlStatus, status);
    Signal(R32(ovl + kOvlEvent));
}

// with g_lock held: false while no datagram is waiting
bool TryComplete(PendingRecv& p) {
    auto it = g_sockets.find(p.sock);
    if (it == g_sockets.end()) {
        Complete(p.ovl, WSAENOTSOCK, 0);
        return true;
    }
    int err = 0;
    int n = Recv(p.sock, it->second.s, p.spans, p.from, p.fromlen, err);
    if (n < 0 && err == WSAEWOULDBLOCK) return false;
    Complete(p.ovl, n < 0 ? uint32_t(err) : 0, n < 0 ? 0 : uint32_t(n));
    return true;
}

void Pump() {
    for (;;) {
        fd_set set;
        FD_ZERO(&set);
        {
            std::lock_guard<std::mutex> lock(g_lock);
            for (auto& p : g_pending) {
                auto it = g_sockets.find(p.sock);
                if (it != g_sockets.end() && set.fd_count < FD_SETSIZE) FD_SET(it->second.s, &set);
            }
        }
        if (!set.fd_count) {
            Sleep(1);
            continue;
        }
        timeval tv{0, 2000};
        if (::select(0, &set, nullptr, nullptr, &tv) <= 0) {
            Sleep(1);
            continue;
        }
        std::lock_guard<std::mutex> lock(g_lock);
        for (size_t i = 0; i < g_pending.size();) {
            auto it = g_sockets.find(g_pending[i].sock);
            bool ready = it == g_sockets.end() || FD_ISSET(it->second.s, &set);
            if (ready && TryComplete(g_pending[i]))
                g_pending.erase(g_pending.begin() + ptrdiff_t(i));
            else
                i++;
        }
    }
}

void OwnXnAddr(uint8_t* x) {
    std::memset(x, 0, kXnAddrSize);
    std::memcpy(x, &g_local, 4);
    const uint8_t mac[6] = {0x00, 0x50, 0xF2, uint8_t(g_local >> 8), uint8_t(g_local >> 16), uint8_t(g_local >> 24)};
    std::memcpy(x + kXnAddrEnet, mac, 6);
}

void Random(uint32_t va, uint32_t n) {
    static std::mutex m;
    static std::mt19937 rng{std::random_device{}()};
    std::lock_guard<std::mutex> lock(m);
    auto p = static_cast<uint8_t*>(HostPtr(va));
    for (uint32_t i = 0; i < n; i++) p[i] = uint8_t(rng());
}

}  // namespace

HLE_STDCALL(00465FE2, XNetStartup, 1) {
    Init();
    return 0;
}
HLE_STDCALL(00465FF9, WSAStartup, 2) {
    Init();
    if (a[1]) {
        std::memset(HostPtr(a[1]), 0, 400);
        W32(a[1], 0x02020202);  // wVersion, wHighVersion
    }
    return 0;
}
HLE_STDCALL(00050488, XNetGetEthernetLinkStatus, 0) { return kLinkUp; }
HLE_STDCALL(00478F54, XOnlineStartup, 1) { return 0; }
HLE_STDCALL(00479051, XOnlineCleanup, 0) { return 0; }
HLE_STDCALL(00473143, WSAGetLastError, 0) { return t_error; }

HLE_STDCALL(00465B8F, XNetRandom, 2) {
    Random(a[0], a[1]);
    return 0;
}
HLE_STDCALL(00465B9A, XNetCreateKey, 2) {
    Random(a[0], 8);
    Random(a[1], 16);
    kernel::G<uint8_t>(a[0]) &= 0x0F;
    return 0;
}
HLE_STDCALL(00465BA5, XNetRegisterKey, 2) {
    std::lock_guard<std::mutex> lock(g_lock);
    std::memcpy(g_kid.data(), HostPtr(a[0]), 8);
    return 0;
}
HLE_STDCALL(00465BB0, XNetUnregisterKey, 1) { return 0; }

HLE_STDCALL(00465C2A, XNetGetTitleXnAddr, 1) {
    Init();
    OwnXnAddr(static_cast<uint8_t*>(HostPtr(a[0])));
    return kXnAddrEthernet;
}
HLE_STDCALL(00465BBB, XNetXnAddrToInAddr, 3) {
    uint32_t ina = R32(a[0]);
    if (!ina) return WSAEINVAL;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        std::memcpy(g_peers[ina].data(), HostPtr(a[0]), kXnAddrSize);
    }
    W32(a[2], ina);
    return 0;
}
HLE_STDCALL(00465DAF, XNetInAddrToXnAddr, 3) {
    uint32_t ina = a[0];
    uint8_t x[kXnAddrSize];
    std::lock_guard<std::mutex> lock(g_lock);
    auto it = g_peers.find(ina);
    if (!ina || ina == g_local) {
        OwnXnAddr(x);
    } else if (it != g_peers.end()) {
        std::memcpy(x, it->second.data(), kXnAddrSize);
    } else {
        std::memset(x, 0, kXnAddrSize);
        std::memcpy(x, &ina, 4);
    }
    if (a[1]) std::memcpy(HostPtr(a[1]), x, kXnAddrSize);
    if (a[2]) std::memcpy(HostPtr(a[2]), g_kid.data(), 8);
    return 0;
}

HLE_STDCALL(00465BD2, XNetDnsLookup, 3) {
    std::string name = kernel::ReadCString(a[0], 256);
    uint32_t dns = mem::AllocSystem(kXnDnsSize);
    std::memset(HostPtr(dns), 0, kXnDnsSize);
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* res = nullptr;
    uint32_t count = 0;
    if (getaddrinfo(name.c_str(), nullptr, &hints, &res) == 0) {
        for (addrinfo* r = res; r && count < 8; r = r->ai_next)
            W32(dns + 8 + 4 * count++, reinterpret_cast<sockaddr_in*>(r->ai_addr)->sin_addr.s_addr);
        freeaddrinfo(res);
    }
    W32(dns, count ? 0 : WSAHOST_NOT_FOUND);
    W32(dns + 4, count);
    W32(a[2], dns);
    LOGI("xnet: dns %s -> %u addresses", name.c_str(), count);
    Signal(a[1]);
    return 0;
}
HLE_STDCALL(00465BE9, XNetDnsRelease, 1) {
    if (a[0]) mem::FreeSystem(a[0]);
    return 0;
}

HLE_STDCALL(00465C35, socket, 3) {
    Init();
    uint32_t af = a[0], type = a[1];  // the protocol: UDP, or VDP, which is UDP between two copies of this
    if (af != AF_INET) return Fail(WSAEAFNOSUPPORT);
    bool udp = type == SOCK_DGRAM;
    if (!udp && type != SOCK_STREAM) return Fail(WSAESOCKTNOSUPPORT);
    SOCKET s = ::socket(AF_INET, int(type), udp ? IPPROTO_UDP : IPPROTO_TCP);
    if (s == INVALID_SOCKET) return Fail(HostError());
    u_long on = 1;
    ::ioctlsocket(s, FIONBIO, &on);
    if (udp) {
        BOOL off = FALSE;
        DWORD ret = 0;
        WSAIoctl(s, kUdpConnReset, &off, sizeof(off), nullptr, 0, &ret, nullptr, nullptr);
    }
    std::lock_guard<std::mutex> lock(g_lock);
    uint32_t id = g_next_socket;
    g_next_socket += 4;
    g_sockets[id] = {s, udp, false};
    return id;
}
HLE_STDCALL(00465C40, closesocket, 1) {
    std::lock_guard<std::mutex> lock(g_lock);
    auto it = g_sockets.find(a[0]);
    if (it == g_sockets.end()) return Fail(WSAENOTSOCK);
    for (size_t i = 0; i < g_pending.size();) {
        if (g_pending[i].sock == a[0])
            g_pending.erase(g_pending.begin() + ptrdiff_t(i));
        else
            i++;
    }
    ::closesocket(it->second.s);
    g_sockets.erase(it);
    return 0;
}
HLE_STDCALL(00465C4B, shutdown, 2) {
    Socket k = Find(a[0]);
    if (k.s == INVALID_SOCKET) return Fail(WSAENOTSOCK);
    return ::shutdown(k.s, int(a[1])) == 0 ? 0 : Fail(HostError());
}
HLE_STDCALL(00465C56, ioctlsocket, 3) {
    std::lock_guard<std::mutex> lock(g_lock);
    auto it = g_sockets.find(a[0]);
    if (it == g_sockets.end()) return Fail(WSAENOTSOCK);
    if (a[1] == uint32_t(FIONBIO)) {
        it->second.nonblocking = R32(a[2]) != 0;
        return 0;
    }
    u_long v = R32(a[2]);
    if (::ioctlsocket(it->second.s, long(a[1]), &v) != 0) return Fail(HostError());
    W32(a[2], uint32_t(v));
    return 0;
}
HLE_STDCALL(00465C61, setsockopt, 5) {
    Socket k = Find(a[0]);
    if (k.s == INVALID_SOCKET) return Fail(WSAENOTSOCK);
    if (::setsockopt(k.s, int(a[1]), int(a[2]), static_cast<const char*>(HostPtr(a[3])), int(a[4])) == 0) return 0;
    LOG_FIRST(10, "xnet: setsockopt %X level %X option %X ignored (%d)", a[0], a[1], a[2], WSAGetLastError());
    return 0;
}
HLE_STDCALL(00465C70, getsockname, 3) {
    Socket k = Find(a[0]);
    if (k.s == INVALID_SOCKET) return Fail(WSAENOTSOCK);
    sockaddr_in me{};
    int len = sizeof(me);
    if (::getsockname(k.s, reinterpret_cast<sockaddr*>(&me), &len) != 0) return Fail(HostError());
    std::memcpy(HostPtr(a[1]), &me, sizeof(me));
    W32(a[2], sizeof(me));
    return 0;
}
HLE_STDCALL(00465C7B, bind, 3) {
    Socket k = Find(a[0]);
    if (k.s == INVALID_SOCKET) return Fail(WSAENOTSOCK);
    sockaddr_in at = GuestAddr(a[1]);
    if (g_instance && at.sin_addr.s_addr == INADDR_ANY) at.sin_addr.s_addr = g_local;
    if (::bind(k.s, reinterpret_cast<sockaddr*>(&at), sizeof(at)) != 0) {
        int e = HostError();
        LOGW("xnet: bind %X to %s failed (%d)", a[0], Addr(at).c_str(), e);
        return Fail(e);
    }
    LOGI("xnet: bind %X to %s", a[0], Addr(at).c_str());
    return 0;
}
HLE_STDCALL(00465C86, connect, 3) {
    Socket k = Find(a[0]);
    if (k.s == INVALID_SOCKET) return Fail(WSAENOTSOCK);
    sockaddr_in to = GuestAddr(a[1]);
    if (::connect(k.s, reinterpret_cast<sockaddr*>(&to), sizeof(to)) == 0) return 0;
    int e = HostError();
    if (e != WSAEWOULDBLOCK || k.nonblocking) return Fail(e);
    if (!Wait(k.s, true, 10000)) return Fail(WSAETIMEDOUT);
    int err = 0, len = sizeof(err);
    ::getsockopt(k.s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
    return err ? Fail(err) : 0;
}
HLE_STDCALL(00465C91, select, 5) {
    uint32_t guest[3] = {a[1], a[2], a[3]};
    fd_set host[3];
    std::vector<std::pair<uint32_t, SOCKET>> used[3];
    int total = 0;
    for (int i = 0; i < 3; i++) {
        FD_ZERO(&host[i]);
        if (!guest[i]) continue;
        uint32_t n = std::min(R32(guest[i]), kFdSetMax);
        for (uint32_t j = 0; j < n; j++) {
            uint32_t id = R32(guest[i] + 4 + 4 * j);
            Socket k = Find(id);
            if (k.s == INVALID_SOCKET) continue;
            FD_SET(k.s, &host[i]);
            used[i].push_back({id, k.s});
            total++;
        }
    }
    timeval tv{};
    timeval* ptv = nullptr;
    if (a[4]) {
        tv.tv_sec = long(R32(a[4]));
        tv.tv_usec = long(R32(a[4] + 4));
        ptv = &tv;
    }
    int r = 0;
    if (!total) {
        if (ptv) Sleep(DWORD(tv.tv_sec * 1000 + tv.tv_usec / 1000));
    } else {
        r = ::select(0, guest[0] ? &host[0] : nullptr, guest[1] ? &host[1] : nullptr, guest[2] ? &host[2] : nullptr, ptv);
        if (r < 0) return Fail(HostError());
    }
    for (int i = 0; i < 3; i++) {
        if (!guest[i]) continue;
        uint32_t k = 0;
        for (auto& [id, s] : used[i])
            if (FD_ISSET(s, &host[i])) W32(guest[i] + 4 + 4 * k++, id);
        W32(guest[i], k);
    }
    return uint32_t(r);
}

HLE_STDCALL(00465CC9, recvfrom, 6) {
    std::vector<Span> spans{{a[1], a[2]}};
    for (;;) {
        Socket k = Find(a[0]);
        if (k.s == INVALID_SOCKET) return Fail(WSAENOTSOCK);
        int err = 0;
        int n;
        {
            std::lock_guard<std::mutex> lock(g_lock);
            n = Recv(a[0], k.s, spans, a[4], a[5], err);
        }
        if (n >= 0) return uint32_t(n);
        if (err != WSAEWOULDBLOCK || k.nonblocking) return Fail(err);
        Wait(k.s, false, 50);
    }
}
HLE_STDCALL(00465CF6, sendto, 6) {
    Socket k = Find(a[0]);
    if (k.s == INVALID_SOCKET) return Fail(WSAENOTSOCK);
    sockaddr_in to{};
    if (a[4]) to = GuestAddr(a[4]);
    int n = Send(a[0], k.s, static_cast<const uint8_t*>(HostPtr(a[1])), int(a[2]), a[4] ? &to : nullptr);
    return n < 0 ? Fail(HostError()) : uint32_t(n);
}

namespace {

uint32_t SendOverlapped(uint32_t sock, uint32_t bufs, uint32_t count, uint32_t sent, uint32_t to_va, uint32_t ovl) {
    Socket k = Find(sock);
    if (k.s == INVALID_SOCKET) return Fail(WSAENOTSOCK);
    auto data = Gather(bufs, count);
    sockaddr_in to{};
    if (to_va) to = GuestAddr(to_va);
    int n = Send(sock, k.s, data.data(), int(data.size()), to_va ? &to : nullptr);
    if (n < 0) return Fail(HostError());
    if (sent) W32(sent, uint32_t(n));
    if (ovl) Complete(ovl, 0, uint32_t(n));
    return 0;
}

uint32_t RecvOverlapped(uint32_t sock, uint32_t bufs, uint32_t count, uint32_t received, uint32_t flags, uint32_t from,
                        uint32_t fromlen, uint32_t ovl) {
    PendingRecv p{sock, ovl, from, fromlen, Spans(bufs, count)};
    for (;;) {
        Socket k = Find(sock);
        if (k.s == INVALID_SOCKET) return Fail(WSAENOTSOCK);
        std::unique_lock<std::mutex> lock(g_lock);
        int err = 0;
        int n = Recv(sock, k.s, p.spans, from, fromlen, err);
        if (n >= 0) {
            if (received) W32(received, uint32_t(n));
            if (flags) W32(flags, 0);
            if (ovl) Complete(ovl, 0, uint32_t(n));
            return 0;
        }
        if (err != WSAEWOULDBLOCK) return Fail(err);
        if (ovl) {
            W32(ovl + kOvlStatus, kOvlPending);
            if (auto o = kernel::FromHandle(R32(ovl + kOvlEvent))) kernel::ResetEvent(*o);
            g_pending.push_back(std::move(p));
            return Fail(WSA_IO_PENDING);
        }
        if (k.nonblocking) return Fail(err);
        lock.unlock();
        Wait(k.s, false, 50);
    }
}

}  // namespace

HLE_STDCALL(00465CD8, WSARecvFrom, 9) { return RecvOverlapped(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]); }
HLE_STDCALL(00465CBA, WSARecv, 7) { return RecvOverlapped(a[0], a[1], a[2], a[3], a[4], 0, 0, a[5]); }
HLE_STDCALL(00465D05, WSASendTo, 9) { return SendOverlapped(a[0], a[1], a[2], a[3], a[5], a[7]); }
HLE_STDCALL(00465CE7, WSASend, 7) { return SendOverlapped(a[0], a[1], a[2], a[3], 0, a[5]); }

HLE_STDCALL(00465CA0, WSAGetOverlappedResult, 5) {
    uint32_t ovl = a[1];
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(g_lock);
            auto it = std::find_if(g_pending.begin(), g_pending.end(), [&](const PendingRecv& p) { return p.ovl == ovl; });
            if (it == g_pending.end() || TryComplete(*it)) {
                if (it != g_pending.end()) g_pending.erase(it);
                uint32_t status = R32(ovl + kOvlStatus);
                if (status == kOvlPending) {
                    t_error = WSA_IO_INCOMPLETE;
                    return 0;
                }
                if (a[2]) W32(a[2], R32(ovl + kOvlBytes));
                if (a[4]) W32(a[4], 0);
                if (status) {
                    t_error = status;
                    return 0;
                }
                return 1;
            }
        }
        if (!a[3]) {
            t_error = WSA_IO_INCOMPLETE;
            return 0;
        }
        Sleep(1);
    }
}
HLE_STDCALL(00465CAF, WSACancelOverlappedIO, 1) {
    std::lock_guard<std::mutex> lock(g_lock);
    for (size_t i = 0; i < g_pending.size();) {
        if (g_pending[i].sock == a[0]) {
            Complete(g_pending[i].ovl, WSA_OPERATION_ABORTED, 0);
            g_pending.erase(g_pending.begin() + ptrdiff_t(i));
        } else {
            i++;
        }
    }
    return 0;
}
