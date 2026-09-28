// The OPSAT key closes the OPSAT from any of its pages, as SCCT's PC version does. On Xbox the key does nothing
// inside it, and B / crouch only backs out one page.
#include <cwctype>
#include <string_view>

#include "xdk/hle.h"

using namespace kernel;

namespace {

// COpsatMenu: +0x18 its manager (+0x14 input handled), +0x18C open. Only opening the OPSAT sets it, so a training
// video from its list closes with it, and one from a PLAY VIDEO interaction (the same page, the OPSAT closed) does not.
constexpr uint32_t kManager = 0x18, kHandled = 0x14, kOpen = 0x18C;
// the full close the in-game menus run when the player controller resets the OPSAT (NotifyMenu(Opsat, 0, 1))
constexpr uint32_t kClose = 0x19A1B0;    // thiscall COpsatMenu
constexpr uint32_t kEventAction = 0x0C;  // input event: FString* bound action

bool IsAction(uint32_t event, std::wstring_view name) {
    uint32_t fs = event ? R32(event + kEventAction) : 0;
    uint32_t data = fs ? R32(fs) : 0, count = fs ? R32(fs + 4) : 0;
    if (!data || count != name.size() + 1) return false;
    for (size_t i = 0; i < name.size(); i++)
        if (std::towlower(wchar_t(R16(data + uint32_t(i) * 2))) != std::towlower(name[i])) return false;
    return true;
}

}  // namespace

// COpsatMenu input handler (thiscall, the element, the input event, ret 8, returns handled)
X86_EXTERN_FUNC(__imp__sub_00199620);
X86_FUNC(sub_00199620) {
    uint32_t menu = ctx.ecx;
    if (G<uint8_t>(menu + kOpen) && IsAction(R32(ctx.esp + 8), L"OpsatMenu")) {
        LOGI("opsat: closing");
        CallGuest(ctx, kClose, {});
        G<uint8_t>(R32(menu + kManager) + kHandled) = 1;
        ctx.eax = 1;
        ctx.esp += 12;  // ret 8
        return;
    }
    __imp__sub_00199620(ctx, base);
}
