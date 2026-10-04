// The wideband voice link beside the game's own voice. While the game sends its 8 kHz Voxware voice to a peer on UDP
// 1010, a 16 kHz Opus copy of the same microphone goes to that peer on 1011, and the peer plays it in place of the
// game's. The game's path stays whole, so voice activation, muting and guards hearing the player are still the game's.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opus.h>
#include <SDL3/SDL.h>

#include "app/app.h"
#include "audio/audio.h"
#include "core/log.h"

namespace audio {

namespace {

constexpr uint16_t kPort = 1011;            // the game's voice port, plus one
constexpr int kRate = 16000, kFrame = 320;  // 20 ms
constexpr int kBitrate = 16000, kComplexity = 10;
constexpr char kMagic[4] = {'S', 'C', 'D', 'V'};
constexpr uint8_t kVersion = 1;
constexpr size_t kHeader = 8;  // magic, version, 0, sequence
constexpr size_t kMaxPayload = 1276;
// a peer the game sent voice to this recently is sent ours; ours heard this recently mutes the game's
constexpr int64_t kSendHoldMs = 300, kHeardHoldMs = 400, kTalkerTimeoutMs = 10000;
// jitter buffer: playback starts at kTargetFrames, a gap is concealed for at most kMaxConcealed frames, a deeper queue
// than kMaxQueued drops its oldest
constexpr int kTargetFrames = 2, kMaxConcealed = 3, kMaxQueued = 10;
constexpr DWORD kUdpConnReset = 0x9800000C;  // SIO_UDP_CONNRESET: off, or a peer without the link fails the next recv
const SDL_AudioSpec kSpec{SDL_AUDIO_S16, 1, kRate};

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// fed by the worker, drained by SDL's playback thread
struct Talker {
    SDL_AudioStream* stream = nullptr;
    OpusDecoder* decoder = nullptr;
    std::mutex lock;
    std::map<uint16_t, std::vector<uint8_t>> queue;  // by sequence, wrapping
    bool playing = false;
    uint16_t next = 0;
    int concealed = 0;
    int64_t last = 0;
};

std::mutex g_lock;  // starting and stopping
int g_users = 0;
std::thread g_worker;
std::atomic<bool> g_stop{false};
std::atomic<int64_t> g_heard{0};
std::mutex g_peers_lock;
std::map<uint32_t, int64_t> g_peers;  // address -> the game's last voice send to it

bool Before(uint16_t a, uint16_t b) { return int16_t(a - b) < 0; }

uint16_t Oldest(const Talker& t) {
    uint16_t oldest = t.queue.begin()->first;
    for (const auto& [sequence, packet] : t.queue)
        if (Before(sequence, oldest)) oldest = sequence;
    return oldest;
}

// one frame of this talker's voice, false while they are silent
bool Pull(Talker& t, int16_t (&pcm)[kFrame]) {
    std::lock_guard lock(t.lock);
    if (!t.playing) {
        if (int(t.queue.size()) < kTargetFrames) return false;
        t.playing = true;
        t.next = Oldest(t);
        t.concealed = 0;
    }
    while (int(t.queue.size()) > kMaxQueued) {
        uint16_t oldest = Oldest(t);
        t.queue.erase(oldest);
        t.next = uint16_t(oldest + 1);
    }
    // a gap longer than concealment covers is skipped, not filled
    if (!t.queue.empty()) {
        uint16_t oldest = Oldest(t);
        if (int16_t(oldest - t.next) > kMaxConcealed) t.next = oldest;
    }
    if (auto it = t.queue.find(t.next); it != t.queue.end()) {
        int n = opus_decode(t.decoder, it->second.data(), opus_int32(it->second.size()), pcm, kFrame, 0);
        t.queue.erase(it);
        t.next++;
        t.concealed = 0;
        if (n != kFrame) std::fill(std::begin(pcm), std::end(pcm), int16_t(0));
        return true;
    }
    if (t.queue.empty() && t.concealed >= kMaxConcealed) {
        t.playing = false;
        return false;
    }
    opus_decode(t.decoder, nullptr, 0, pcm, kFrame, 0);
    t.next++;
    t.concealed++;
    return true;
}

void SDLCALL Feed(void* user, SDL_AudioStream* stream, int additional, int) {
    auto& t = *static_cast<Talker*>(user);
    int16_t pcm[kFrame];
    for (int fed = 0; fed < additional; fed += int(sizeof(pcm))) {
        if (!Pull(t, pcm)) return;
        SDL_PutAudioStreamData(stream, pcm, sizeof(pcm));
    }
}

// destroying the stream waits out a callback in progress
void Destroy(Talker* t) {
    SDL_DestroyAudioStream(t->stream);
    opus_decoder_destroy(t->decoder);
    delete t;
}

std::string Ip(uint32_t a) {
    char s[32];
    std::snprintf(s, sizeof(s), "%u.%u.%u.%u", a & 0xFF, (a >> 8) & 0xFF, (a >> 16) & 0xFF, a >> 24);
    return s;
}

SOCKET OpenSocket() {
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return s;
    u_long nonblocking = 1;
    ::ioctlsocket(s, FIONBIO, &nonblocking);
    BOOL off = FALSE;
    DWORD bytes = 0;
    ::WSAIoctl(s, kUdpConnReset, &off, sizeof(off), nullptr, 0, &bytes, nullptr, nullptr);
    sockaddr_in at{};
    at.sin_family = AF_INET;
    at.sin_port = htons(kPort);
    // --instance N: the game's sockets are on 127.0.0.N, so this one is too
    at.sin_addr.s_addr = app::Instance() ? htonl(0x7F000000u | uint32_t(app::Instance())) : htonl(INADDR_ANY);
    if (::bind(s, reinterpret_cast<sockaddr*>(&at), sizeof(at)) != 0) {
        LOGW("voice link: bind to %s:%u failed (%d), not heard", Ip(at.sin_addr.s_addr).c_str(), kPort, WSAGetLastError());
        return s;
    }
    LOGI("voice link: on %s:%u", Ip(at.sin_addr.s_addr).c_str(), kPort);
    return s;
}

void Run(std::string microphone) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET s = OpenSocket();
    VoiceStream* capture = OpenMicrophone(microphone, kRate, 1);
    int error = OPUS_OK;
    OpusEncoder* encoder = opus_encoder_create(kRate, 1, OPUS_APPLICATION_VOIP, &error);
    if (encoder) {
        opus_encoder_ctl(encoder, OPUS_SET_BITRATE(kBitrate));
        opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(kComplexity));
        opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    } else {
        LOGE("voice link: no Opus encoder: %s", opus_strerror(error));
    }
    SDL_AudioDeviceID playback = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (playback) SDL_ResumeAudioDevice(playback);
    else LOGE("voice link: no playback device: %s", SDL_GetError());
    std::map<uint64_t, Talker*> talkers;  // by sender address and port
    std::vector<uint32_t> sending;
    uint16_t sequence = 0;
    int16_t pcm[kFrame];
    uint8_t packet[kHeader + kMaxPayload];
    std::memcpy(packet, kMagic, 4);
    packet[4] = kVersion;
    packet[5] = 0;
    while (!g_stop) {
        fd_set f;
        FD_ZERO(&f);
        FD_SET(s, &f);
        timeval tv{0, 5000};
        ::select(0, &f, nullptr, nullptr, &tv);
        int64_t now = NowMs();
        for (;;) {
            uint8_t in[kHeader + kMaxPayload];
            sockaddr_in from{};
            int len = sizeof(from);
            int n = ::recvfrom(s, reinterpret_cast<char*>(in), sizeof(in), 0, reinterpret_cast<sockaddr*>(&from), &len);
            if (n < 0) break;
            if (n <= int(kHeader) || std::memcmp(in, kMagic, 4) || in[4] != kVersion || !playback) continue;
            uint64_t key = uint64_t(from.sin_addr.s_addr) << 16 | from.sin_port;
            Talker*& t = talkers[key];
            if (!t) {
                auto made = new Talker;
                made->decoder = opus_decoder_create(kRate, 1, &error);
                made->stream = made->decoder ? SDL_CreateAudioStream(&kSpec, &kSpec) : nullptr;
                if (!made->stream || !SDL_SetAudioStreamGetCallback(made->stream, Feed, made) ||
                    !SDL_BindAudioStream(playback, made->stream)) {
                    LOGE("voice link: no playback for %s: %s", Ip(from.sin_addr.s_addr).c_str(), SDL_GetError());
                    if (made->stream) SDL_DestroyAudioStream(made->stream);
                    if (made->decoder) opus_decoder_destroy(made->decoder);
                    delete made;
                    talkers.erase(key);
                    continue;
                }
                t = made;
                LOGI("voice link: hearing %s", Ip(from.sin_addr.s_addr).c_str());
            }
            uint16_t seq = uint16_t(in[6] | in[7] << 8);
            std::lock_guard lock(t->lock);
            t->last = now;
            g_heard = now;
            if (t->playing && Before(seq, t->next)) continue;
            t->queue[seq].assign(in + kHeader, in + n);
        }
        for (auto it = talkers.begin(); it != talkers.end();) {
            int64_t last;
            {
                std::lock_guard lock(it->second->lock);
                last = it->second->last;
            }
            if (now - last < kTalkerTimeoutMs) {
                ++it;
                continue;
            }
            LOGI("voice link: %s silent, dropped", Ip(uint32_t(it->first >> 16)).c_str());
            Destroy(it->second);
            it = talkers.erase(it);
        }
        // every frame is encoded, sent or not, so the encoder never jumps
        while (capture && encoder && Available(capture) >= sizeof(pcm)) {
            Read(capture, pcm, sizeof(pcm));
            int bytes = opus_encode(encoder, pcm, kFrame, packet + kHeader, opus_int32(kMaxPayload));
            uint16_t seq = sequence++;
            if (bytes <= 0) continue;
            packet[6] = uint8_t(seq);
            packet[7] = uint8_t(seq >> 8);
            sending.clear();
            {
                std::lock_guard lock(g_peers_lock);
                for (const auto& [address, at] : g_peers)
                    if (now - at < kSendHoldMs) sending.push_back(address);
            }
            for (uint32_t address : sending) {
                sockaddr_in to{};
                to.sin_family = AF_INET;
                to.sin_port = htons(kPort);
                to.sin_addr.s_addr = address;
                ::sendto(s, reinterpret_cast<const char*>(packet), int(kHeader) + bytes, 0, reinterpret_cast<sockaddr*>(&to),
                         sizeof(to));
            }
        }
    }
    for (auto& [key, t] : talkers) Destroy(t);
    if (playback) SDL_CloseAudioDevice(playback);
    if (encoder) opus_encoder_destroy(encoder);
    if (capture) Close(capture);
    if (s != INVALID_SOCKET) ::closesocket(s);
    LOGI("voice link: stopped");
}

}  // namespace

void StartVoiceLink(const std::string& microphone) {
    std::lock_guard lock(g_lock);
    if (g_users++) return;
    g_stop = false;
    g_heard = 0;
    g_worker = std::thread(Run, microphone);
}

void StopVoiceLink() {
    std::lock_guard lock(g_lock);
    if (g_users == 0 || --g_users) return;
    g_stop = true;
    if (g_worker.joinable()) g_worker.join();
    g_heard = 0;
    std::lock_guard peers(g_peers_lock);
    g_peers.clear();
}

void VoiceSent(uint32_t address) {
    std::lock_guard lock(g_peers_lock);
    auto [it, added] = g_peers.insert_or_assign(address, NowMs());
    if (added) LOGI("voice link: the game talks to %s", Ip(address).c_str());
}

bool VoiceLinkHeard() {
    int64_t heard = g_heard;
    return heard && NowMs() - heard < kHeardHoldMs;
}

}  // namespace audio
