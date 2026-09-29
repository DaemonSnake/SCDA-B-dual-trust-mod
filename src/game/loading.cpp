// Mission loading screen. The front end shows <map>_loading<n>.tga, then relaunches the title into the mission,
// and the Xbox keeps that picture up (D3DDevice_PersistDisplay) while the new instance loads. On a PC the load is
// over at once, so the picture waits for a key or button before the relaunch, as SCCT's PC loading screen does.
#include <bit>
#include <cctype>
#include <string>

#include "core/input.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

bool g_mission_load = false;  // game thread

// The next instance draws the load's progress over the same picture: sub_00039566, from the render device at
// 0x5E3E2C, a solid tile through sub_002E65A4 thiscall(x0, y0, x1, y1, u0, v0, u1, v1, texture, r, g, b), viewport
// pixels, black at 335,135, 4 high, 240 wide at 100%. Drawn full here, so the picture waiting for a key doesn't read
// as a load stuck at 0.
constexpr uint32_t kRenderDevice = 0x5E3E2C, kDrawTile = 0x2E65A4;
constexpr float kBarX = 335, kBarY = 135, kBarWidth = 240, kBarHeight = 4;

void FullBar(X86Context& ctx) {
    uint32_t device = R32(kRenderDevice);
    if (!device) return;
    auto f = [](float v) { return std::bit_cast<uint32_t>(v); };
    uint32_t ecx = ctx.ecx;
    ctx.ecx = device;
    CallGuest(ctx, kDrawTile, {f(kBarX), f(kBarY), f(kBarX + kBarWidth), f(kBarY + kBarHeight), f(0), f(0), f(1), f(1), 0,
                               f(0), f(0), f(0)});
    ctx.ecx = ecx;
    CallGuest(ctx, SYM(D3DDevice_Swap), {0});
}

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
        if (wait) {
            FullBar(ctx);
            input::WaitForPress();
        }
    }
    __imp__sub_0026AB34(ctx, base);
}

// The controller layout screen (loading screen vtable +0x34 sub_00038C3F, thiscall) shows controllerN.tga and polls
// vtable +0x40 (sub_000367E4 -> sub_0004B46F, A or Start let go on any pad, the movies' skip check) until it answers.
// It says "press A"; any key, click or pad button answers here, as on the mission picture.
namespace {
bool g_controls_screen = false;  // game thread
}  // namespace

X86_EXTERN_FUNC(__imp__sub_00038C3F);
X86_FUNC(sub_00038C3F) {
    g_controls_screen = true;
    __imp__sub_00038C3F(ctx, base);
    g_controls_screen = false;
}

X86_EXTERN_FUNC(__imp__sub_000367E4);
X86_FUNC(sub_000367E4) {
    __imp__sub_000367E4(ctx, base);
    if (!ctx.eax && g_controls_screen && input::Pressed()) ctx.eax = 1;
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
