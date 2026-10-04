// Movies on a Magma page (the main menu's Cinematics, PH_Video) are drawn by the render interface's movie draw,
// thiscall(movie) with the Bink at movie +0x88: sub_002E6D5B decodes and draws, sub_002E708A draws a held frame. Its
// fit takes a pixel aspect of 4/3 in a widescreen mode, except in the menu map (the controller's vtable +0xDC,
// sub_001D05CB, map name "menu"). That keeps the 4:3 menu background from being pillarboxed, but letterboxed the 16:9
// cinematics as well, so a movie wider than 4:3 is fitted there as it is in a level.
#include <cstdint>

#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr uint32_t kMovieBink = 0x88;  // Bink: +0 width, +4 height
thread_local bool g_wide_movie = false;

bool Wide(uint32_t movie) {
    uint32_t bink = movie ? R32(movie + kMovieBink) : 0;
    return bink && uint64_t(R32(bink)) * 3 > uint64_t(R32(bink + 4)) * 4;
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_002E6D5B);
X86_FUNC(sub_002E6D5B) {
    g_wide_movie = Wide(R32(ctx.esp + 4));
    __imp__sub_002E6D5B(ctx, base);
    g_wide_movie = false;
}

X86_EXTERN_FUNC(__imp__sub_002E708A);
X86_FUNC(sub_002E708A) {
    g_wide_movie = Wide(R32(ctx.esp + 4));
    __imp__sub_002E708A(ctx, base);
    g_wide_movie = false;
}

// the level is the menu (thiscall, no arguments)
X86_EXTERN_FUNC(__imp__sub_001D05CB);
X86_FUNC(sub_001D05CB) {
    if (g_wide_movie) {
        ctx.eax = 0;
        ctx.esp += 4;
        return;
    }
    __imp__sub_001D05CB(ctx, base);
}
