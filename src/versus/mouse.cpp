// Mouse look in versus. The controller's native PlayerInput (sub_0003FB80) turns aTurn / aLookUp into the look
// rates it leaves at +0x4D8 / +0x4DC (+-1000 is full stick) through a 220 dead zone, a clamp to full stick, a response
// curve and smoothing; the mouse replaces its outputs after it, so none of those touch it. The binoculars drop the
// rates while the stick magnitude it leaves is 0, so that follows the mouse as well. Divided by dt, a count turns the
// same at any frame rate.
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>

#include "core/input.h"
#include "core/log.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// viewport: +0x30 actor, the local player's controller
constexpr uint32_t kViewportActor = 0x30;
// controller: +0x4D0 stick magnitude after the curve and +0x4D4 smoothed (0 to 1), +0x4D8 turn rate, +0x4DC look up
// rate, +0x4E0 / +0x4E4 the largest |aTurn| / |aLookUp| seen (the rates are aTurn and aLookUp over them), +0x504 aTurn,
// +0x50C aLookUp, +0x720 (+0x320 flags 0x180: no look), +0xF4C bit 0x400 invert look (the look rate only)
constexpr uint32_t kMagnitude = 0x4D0, kSmoothMagnitude = 0x4D4, kTurnRate = 0x4D8, kLookRate = 0x4DC,
                   kTurnMax = 0x4E0, kLookMax = 0x4E4, kTurn = 0x504, kLookUp = 0x50C, kLookBlock = 0x720,
                   kLookBlockFlags = 0x320, kOptions = 0xF4C;
constexpr uint32_t kNoLook = 0x180, kInvertLook = 0x400;
// +0xF48 bit 0x2000000 while the binoculars are up (the sniper view)
constexpr uint32_t kFlags = 0xF48, kBinoculars = 0x2000000;
// the binoculars zoom on the D-pad; one press is held 80 ms (input::TapButtons), then let go for at least a frame
constexpr uint16_t kDpadUp = 1 << 0, kDpadDown = 1 << 1;
constexpr auto kZoomTapGap = std::chrono::milliseconds(150);
// PlayerInput's other way out: the object at 0x3AA3FC with +0x34 bit 2 set (sub_000DC110)
constexpr uint32_t kLookGate = 0x3AA3FC;
constexpr float kFullStick = 1000.0f;
// full stick for one second is this many counts
constexpr float kCountsPerStickSecond = 1500.0f;
// the binoculars slow the look down with the zoom; the mouse gets some of it back
constexpr float kBinocularSpeed = 1.5f;

uint32_t g_player = 0;
// the counts of the last viewport update, for every PlayerInput after it
float g_dx = 0.0f, g_dy = 0.0f;
bool g_binoculars = false;
int g_zoom = 0;  // wheel notches not pressed yet, up positive
std::chrono::steady_clock::time_point g_zoom_next;

bool Looking(uint32_t controller) {
    uint32_t gate = R32(kLookGate);
    if (gate && (R32(gate + 0x34) >> 2 & 1)) return false;
    uint32_t block = R32(controller + kLookBlock);
    return !block || !(R32(block + kLookBlockFlags) & kNoLook);
}

// in the binoculars the wheel zooms as D-pad up and down, one press a notch
void Zoom(uint32_t controller) {
    bool binoculars = controller && (R32(controller + kFlags) & kBinoculars);
    if (binoculars != g_binoculars) {
        g_binoculars = binoculars;
        g_zoom = 0;
        input::SetView(binoculars ? input::View::Eev : input::View::Normal);
        LOGI("versus mouse: binoculars %s", binoculars ? "up" : "down");
    }
    if (!binoculars) return;
    int notches = input::TakeWheel();
    if (notches * g_zoom < 0) g_zoom = 0;
    g_zoom += notches;
    auto now = std::chrono::steady_clock::now();
    if (!g_zoom || now < g_zoom_next) return;
    input::TapButtons(g_zoom > 0 ? kDpadUp : kDpadDown);
    g_zoom += g_zoom > 0 ? -1 : 1;
    g_zoom_next = now + kZoomTapGap;
}

void WriteFloat(uint32_t address, float value) { W32(address, std::bit_cast<uint32_t>(value)); }

float Rate(float counts, float seconds) {
    float rate = kFullStick * counts / (kCountsPerStickSecond * seconds);
    return g_binoculars ? rate * kBinocularSpeed : rate;
}

// the rate, and the axis it would have come from
void Look(uint32_t rate, uint32_t axis, uint32_t max, float r, bool invert) {
    float m = std::bit_cast<float>(R32(max));
    WriteFloat(rate, invert ? -r : r);
    WriteFloat(axis, m > 0.0f ? r * m / kFullStick : r);
}

}  // namespace

// thiscall(viewport, reset, float delta seconds), ret 8
X86_EXTERN_FUNC(__imp__sub_00188E80);
X86_FUNC(sub_00188E80) {
    uint32_t viewport = ctx.ecx;
    __imp__sub_00188E80(ctx, base);
    g_player = R32(viewport + kViewportActor);
    Zoom(g_player);
    // counts from a menu or a load are dropped, not saved up for the next look
    if (!input::TakeMouse(g_dx, g_dy)) g_dx = g_dy = 0.0f;
}

// thiscall(controller, float delta seconds), ret 4
X86_EXTERN_FUNC(__imp__sub_0003FB80);
X86_FUNC(sub_0003FB80) {
    uint32_t controller = ctx.ecx;
    float seconds = std::bit_cast<float>(R32(ctx.esp + 4));
    __imp__sub_0003FB80(ctx, base);
    if (controller != g_player || (g_dx == 0.0f && g_dy == 0.0f) || !(seconds > 0.0f) || !Looking(controller)) return;
    // the stick's up is positive, the mouse's is negative
    bool invert = R32(controller + kOptions) & kInvertLook;
    float turn = Rate(g_dx, seconds), up = Rate(-g_dy, seconds);
    if (turn != 0.0f) Look(controller + kTurnRate, controller + kTurn, controller + kTurnMax, turn, false);
    if (up != 0.0f) Look(controller + kLookRate, controller + kLookUp, controller + kLookMax, up, invert);
    float magnitude = std::min(1.0f, std::sqrt(turn * turn + up * up) / kFullStick);
    WriteFloat(controller + kMagnitude, magnitude);
    WriteFloat(controller + kSmoothMagnitude, magnitude);
    LOG_FIRST(8, "versus mouse: %g %g over %g s", g_dx, g_dy, seconds);
}
