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

// the travel URL, map name first; the main menu's map is Menu and has no loading picture
std::string Url(uint32_t text) {
    std::string url;
    for (uint32_t n = 0; text && n < 260; text += 2, n++) {
        uint16_t c = R16(text);
        if (!c) break;
        url += c < 0x80 ? char(std::tolower(c)) : '?';
    }
    return url;
}

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
        if (url.rfind("menu", 0) != 0) {
            LOGI("loading: %s, waiting for a key", url.c_str());
            input::WaitForPress();
        }
    }
    __imp__sub_0026AB34(ctx, base);
}
