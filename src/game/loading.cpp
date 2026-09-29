// Mission loading screen. The front end shows <map>_loading<n>.tga, then relaunches the title into the mission,
// and the Xbox keeps that picture up (D3DDevice_PersistDisplay) while the new instance loads. On a PC the load is
// over at once, so the picture waits for a key or button before the relaunch, as SCCT's PC loading screen does.
#include <cctype>
#include <string>

#include "core/input.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

bool g_mission_load = false;  // game thread

// launch data text: "mapname=<map>?<options> playername=... lastscreen=..."
std::string Url(uint32_t text) {
    std::string url;
    for (uint32_t n = 0; text && n < 260; text += 2, n++) {
        uint16_t c = R16(text);
        if (!c) break;
        url += c < 0x80 ? char(std::tolower(c)) : '?';
    }
    return url;
}

std::string MapName(const std::string& url) {
    size_t at = url.find("mapname=");
    if (at == std::string::npos) return {};
    at += 8;
    return url.substr(at, url.find_first_of("? ", at) - at);
}

// single player missions only: the main menu (map Menu) has nothing to read, and in co-op (x01_coop_ice_a,
// every co-op map has _coop_) a player held on the picture would fall behind the other
bool Waits(const std::string& map) { return !map.empty() && map != "menu" && map.find("_coop_") == std::string::npos; }

}  // namespace

// front end: loading screen, launch data, relaunch
X86_EXTERN_FUNC(__imp__sub_000444B4);
X86_FUNC(sub_000444B4) {
    g_mission_load = true;
    __imp__sub_000444B4(ctx, base);
    g_mission_load = false;
}

// launch data writer (type, url, launch), thiscall; launch relaunches the title
X86_EXTERN_FUNC(__imp__sub_0026AB34);
X86_FUNC(sub_0026AB34) {
    if (g_mission_load && R32(ctx.esp + 12)) {
        g_mission_load = false;
        std::string url = Url(R32(ctx.esp + 8));
        bool wait = Waits(MapName(url));
        LOGI("loading: %s%s", url.c_str(), wait ? ", waiting for a key" : "");
        if (wait) input::WaitForPress();
    }
    __imp__sub_0026AB34(ctx, base);
}

// level load progress (thiscall(progress), ret 4): draws the bar alone and presents it over whatever the back buffer
// holds. The saving screen behind it is drawn once, into one of the two buffers, so every other frame showed the
// bar on the other buffer's older picture. The frame just shown is copied into the back buffer for the next bar.
X86_EXTERN_FUNC(__imp__sub_001CD157);
X86_FUNC(sub_001CD157) {
    __imp__sub_001CD157(ctx, base);
    if (!ctx.eax) return;
    uint32_t front = CallGuest(ctx, SYM(D3DDevice_GetBackBuffer2), {0xFFFFFFFFu});
    uint32_t back = CallGuest(ctx, SYM(D3DDevice_GetBackBuffer2), {0});
    CallGuest(ctx, SYM(D3DDevice_CopyRects), {front, 0, 0, back, 0});
    W32(front, R32(front) - 1);
    W32(back, R32(back) - 1);
}
