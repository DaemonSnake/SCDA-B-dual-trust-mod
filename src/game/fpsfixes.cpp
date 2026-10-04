// Vision modes reroll their grain every frame, so it races at high fps. Each roll is held for 1/30 s.
#include <algorithm>
#include <chrono>

#include "xdk/hle.h"

using namespace kernel;

namespace {

using Clock = std::chrono::steady_clock;
constexpr auto kStep = std::chrono::microseconds(33333);

struct Roll {
    Clock::time_point next;
    bool fresh;
    double v[3];
};
Roll g_rolls[4];

// return address of a grain roll as roll * 4 + value, else -1
int Site(uint32_t ret) {
    switch (ret) {
    case 0x2F02E8: return 0;  // noise overlay: angle, v, u
    case 0x2F0343: return 1;
    case 0x2F034E: return 2;
    case 0x2F2119: return 4;
    case 0x2F213E: return 5;
    case 0x2F2149: return 6;
    case 0x2F2A6A: return 8;
    case 0x2F2A9D: return 9;
    case 0x2F2AB9: return 10;
    case 0x2F308D:
    case 0x2F309C: return 12;  // thermal distortion angle
    }
    return -1;
}

template <class T>
void Hold(int site, T& value) {
    if (site < 0) return;
    Roll& r = g_rolls[site / 4];
    if (site % 4 == 0) {
        auto now = Clock::now();
        if ((r.fresh = now >= r.next)) r.next = std::max(r.next + kStep, now);
    }
    if (r.fresh) r.v[site % 4] = double(value);
    else value = T(r.v[site % 4]);
}

}  // namespace

// appFrand
X86_EXTERN_FUNC(__imp__sub_00067760);
X86_FUNC(sub_00067760) {
    int site = Site(R32(ctx.esp));
    __imp__sub_00067760(ctx, base);
    Hold(site, X87_ST(0));
}

// rand
X86_EXTERN_FUNC(__imp__sub_00067732);
X86_FUNC(sub_00067732) {
    int site = Site(R32(ctx.esp));
    __imp__sub_00067732(ctx, base);
    Hold(site, ctx.eax);
}
