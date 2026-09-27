#include <cstdlib>
#include <fstream>

#include "kernel/kernel.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "xbe/xbe.h"

namespace dispatch {
void Init();
uint32_t KernelThunkAddress(uint32_t ordinal);
}  // namespace dispatch

namespace kernel {

uint8_t* g_base = nullptr;
std::string g_launch_file;
const xbe::Image* g_image = nullptr;
static xbe::Image g_xbe;

bool Boot(const std::string& xbe_path, const std::string& game_dir, const std::string& data_dir) {
    if (!mem::Init()) return false;
    g_base = mem::Base();

    std::ifstream f(xbe_path, std::ios::binary);
    if (!f) {
        LOGE("boot: cannot open %s", xbe_path.c_str());
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    std::string err;
    if (!g_xbe.Parse(std::move(data), err)) {
        LOGE("boot: %s: %s", xbe_path.c_str(), err.c_str());
        return false;
    }
    g_image = &g_xbe;
    const xbe::Image& img = g_xbe;
    if (img.entry != X86EntryPoint) {
        LOGE("boot: %s entry %08X does not match the recompiled entry %08X, wrong executable", xbe_path.c_str(), img.entry,
             X86EntryPoint);
        return false;
    }
    LOGI("boot: title %08X, entry %08X, %zu sections", img.title_id, img.entry, img.sections.size());
    for (auto& l : img.libraries) LOGI("boot: library %-8s %u.%u.%u", l.name.c_str(), l.major, l.minor, l.build);

    mem::ReserveImage(img.base, img.image_size);
    std::memcpy(g_base + img.base, img.file.data(), std::min<size_t>(img.header_size, img.file.size()));
    uint32_t sec_hdr = img.U32(0x120);
    for (size_t i = 0; i < img.sections.size(); i++) {
        auto& s = img.sections[i];
        if (!(s.flags & xbe::kPreload)) continue;
        std::memcpy(g_base + s.va, img.file.data() + s.raw, std::min(s.rsize, s.vsize));
        W32(sec_hdr + uint32_t(i) * 56 + 0x18, 1);
    }

    InitKernelData();
    dispatch::Init();

    uint32_t missing = 0;
    for (uint32_t slot = img.kernel_thunk;; slot += 4) {
        uint32_t v = R32(slot);
        if (!v) break;
        uint32_t ord = v & 0x7FFFFFFF;
        uint32_t data = DataAddress(ord);
        W32(slot, data ? data : dispatch::KernelThunkAddress(ord));
        if (!data && std::string(ExportName(ord)) == "?") {
            LOGW("boot: kernel import %u has no implementation", ord);
            missing++;
        }
    }
    LOGI("boot: kernel imports resolved, %u missing", missing);

    MountDefaults(game_dir, data_dir);

    // a quick reboot into this title left its launch data page behind
    g_launch_file = data_dir + "/launch.bin";
    std::ifstream lf(g_launch_file, std::ios::binary);
    if (lf) {
        uint32_t page = mem::AllocContiguous(0x1000, 0, 0xFFFFFFFF, 0x1000);
        lf.read(static_cast<char*>(HostPtr(page)), 0x1000);
        lf.close();
        std::remove(g_launch_file.c_str());
        W32(g_launch_data_page_va, page);
        LOGI("boot: launch data from the previous run, type %u", R32(page));
    }

    uint32_t tls = 0;
    if (img.tls_addr) tls = ((img.tls.data_end - img.tls.data_start + img.tls.zero_fill + 15) & ~15u) + 4;
    ThreadRef t = CreateThread(std::max<uint32_t>(img.stack_commit, 0x10000), tls, 0, img.entry, 0, false);
    StartThread(t);
    return true;
}

void Relaunch() {
    logx::Flush();
#ifdef _WIN32
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(exe, GetCommandLineW(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        ::CloseHandle(pi.hThread);
        ::CloseHandle(pi.hProcess);
    }
    std::_Exit(0);
#else
    execl("/proc/self/exe", "/proc/self/exe", nullptr);
    std::_Exit(0);
#endif
}

void Shutdown(int code) {
    LOGI("shutdown (%d)", code);
    logx::Flush();
    std::_Exit(code);
}

}  // namespace kernel
