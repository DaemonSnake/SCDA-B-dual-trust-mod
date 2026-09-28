// Co-op is Quazal Net-Z's SyncSim, a fixed-step lockstep. The session init sub_0025480C caps the step frequency at
// 22 Hz and puts a 50 ms floor under the input delay; both are options here. Both machines need the same values:
// the session runs at the lowest peer's rate, and different floors commit inputs on different steps.
// The main loop (sub_0003F950) holds every frame of a synced session to 1/22 s on its own as well, in 7 ms sleeps;
// with a raised rate that limit goes and the lockstep alone paces the frames.
#include <algorithm>

#include "app/app.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// SyncSim setters, thiscall(sim, value), each called once, by the session init: +0x84 the step frequency this
// machine advertises (Hz), +0x8C the input delay floor (ms)
constexpr uint32_t kMaxRate = 250;  // a step is at least 4 whole ms
constexpr uint32_t kFrameLimit = 0x4F64D0;  // double, seconds; read only at 0003F9F2
constexpr double kStockFrameLimit = double(1.0f / 22.0f);
constexpr uint32_t kMaxDelayFloor = 200;

}  // namespace

X86_EXTERN_FUNC(__imp__sub_0040A7A0);
X86_FUNC(sub_0040A7A0) {
    uint32_t rate = app::Get().coop_tick_rate;
    if (rate) W32(ctx.esp + 4, std::min(rate, kMaxRate));
    G<double>(kFrameLimit) = rate ? 0.0 : kStockFrameLimit;
    LOGI("coop: step frequency ceiling %u Hz", R32(ctx.esp + 4));
    __imp__sub_0040A7A0(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_0040A740);
X86_FUNC(sub_0040A740) {
    W32(ctx.esp + 4, std::min(app::Get().coop_input_delay_floor, kMaxDelayFloor));
    LOGI("coop: input delay floor %u ms", R32(ctx.esp + 4));
    __imp__sub_0040A740(ctx, base);
}

// The step delta. The game returns 1/rate s (rate = SyncSim +0x7C, left in eax), but each step waits
// floor(1000/rate) whole ms (sub_0040C039), so game time ran fast: 1% at 22 Hz, 6% at 60. Return the real step.
X86_EXTERN_FUNC(__imp__sub_002535D6);
X86_FUNC(sub_002535D6) {
    __imp__sub_002535D6(ctx, base);
    if (ctx.eax) X87_ST(0) = double(1000 / ctx.eax) / 1000.0;
}
