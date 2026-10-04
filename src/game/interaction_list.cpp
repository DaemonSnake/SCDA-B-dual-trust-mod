// The HUD's interaction list. The interact key focuses it (the box with BACK TO GAME on top); while it is focused
// its key handler steps it on the AnalogUp / AnalogDown aliases, which the mouse wheel taps there (mouse.cpp),
// as SCCT PC's handler takes IK_MouseWheelUp / Down.
#include "xdk/hle.h"

using namespace kernel;

namespace {

// interaction list: vtable 0x5491D0, +0x22 focused
constexpr uint32_t kListVtable = 0x5491D0;
constexpr uint32_t kFocused = 0x22;

uint32_t g_list = 0;  // the list focused last

}  // namespace

namespace game {

bool InteractionListFocused() {
    return g_list && R32(g_list) == kListVtable && G<uint8_t>(g_list + kFocused);
}

}  // namespace game

// the list's focus (thiscall, no args)
X86_EXTERN_FUNC(__imp__sub_001B9755);
X86_FUNC(sub_001B9755) {
    uint32_t list = ctx.ecx;
    __imp__sub_001B9755(ctx, base);
    if (G<uint8_t>(list + kFocused)) g_list = list;
}
