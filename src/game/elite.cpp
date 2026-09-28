#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr bool kUnlockElite = true;

// CNewLoadSaveGame +0x20 the menus interaction; its +0xCC the profile, whose +0x1C3C is set by sub_001BBA56 when
// the last solo mission is finished
constexpr uint32_t kInteraction = 0x20, kCampaignDone = 0x1D08;

template <typename F>
void WithCampaignDone(uint32_t menu, F&& call) {
    uint32_t interaction = menu ? R32(menu + kInteraction) : 0;
    if (!interaction || !kUnlockElite) return call();
    uint32_t saved = R32(interaction + kCampaignDone);
    W32(interaction + kCampaignDone, 1);
    call();
    W32(interaction + kCampaignDone, saved);
}

}  // namespace

// CNewLoadSaveGame page setup (thiscall, no args), fills Lst_Difficulty when the page opens on it
X86_EXTERN_FUNC(__imp__sub_00188B30);
X86_FUNC(sub_00188B30) {
    WithCampaignDone(ctx.ecx, [&] { __imp__sub_00188B30(ctx, base); });
}

// CNewLoadSaveGame press handler (thiscall, the element, ret 4), fills Lst_Difficulty on NEW GAME in Lst_Missions
X86_EXTERN_FUNC(__imp__sub_0018A821);
X86_FUNC(sub_0018A821) {
    WithCampaignDone(ctx.ecx, [&] { __imp__sub_0018A821(ctx, base); });
}
