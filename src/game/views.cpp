// The EEV and the sticky camera with the mouse wheel. Both are states of the player controller.
// EEV: the pad's left stick click (ZoomInOut) steps EEEV.ZoomIdx through FOVs and wraps. The wheel sets a target
// level instead, up zooms in and down zooms out, stopping at the ends, and the click is tapped until the level
// is reached. Sticky camera: the input side zooms it with the stick and remaps its keys.
// Scripted events (in-game cinematics): s_ScriptedEvent's ProcessPauseMenu skips them, so Enter presses Start too.
#include <algorithm>
#include <chrono>
#include <cstring>

#include "core/input.h"
#include "game/unreal.h"
#include "xdk/hle.h"

using namespace kernel;
namespace unreal = game::unreal;

namespace {

constexpr uint32_t kViewportActor = 0x34;
constexpr uint16_t kLeftThumb = 1 << 6;
// a tap the game did not act on (EEV busy scanning or hacking) drops the target
constexpr auto kTapTimeout = std::chrono::milliseconds(400);

bool g_in_eev = false;
int g_target = 0, g_last = 0;
bool g_tapping = false;
std::chrono::steady_clock::time_point g_tap_time;

void Eev(uint32_t pc) {
    uint32_t eev_offset = unreal::Offset(pc, unreal::Name("EEV"));
    uint32_t eev = eev_offset ? R32(pc + eev_offset) : 0;
    uint32_t zoom = eev ? unreal::Offset(eev, unreal::Name("ZoomIdx")) : 0;
    uint32_t fovs = eev ? unreal::Offset(eev, unreal::Name("FOVs")) : 0;
    int notches = input::TakeWheel();
    if (!zoom || !fovs) return;
    int level = int(R32(eev + zoom)), count = int(R32(eev + fovs + 4));
    auto now = std::chrono::steady_clock::now();
    if (!g_in_eev) {
        g_in_eev = true;
        g_target = g_last = level;
        g_tapping = false;
    }
    if (g_tapping && level != g_last) {
        g_tapping = false;
        g_last = level;
    } else if (g_tapping && now - g_tap_time > kTapTimeout) {
        g_tapping = false;
        g_target = level;
    }
    // the player clicked the stick or the game reset the zoom
    if (!g_tapping && level != g_last) g_target = level;
    g_last = level;
    if (notches && count > 0) g_target = std::clamp(g_target + notches, 0, count - 1);
    if (!g_tapping && level != g_target) {
        input::TapButtons(kLeftThumb);
        g_tapping = true;
        g_tap_time = now;
    }
}

}  // namespace

namespace game {

// the viewport, each input update on the game thread
void UpdateView(uint32_t viewport) {
    uint32_t pc = R32(viewport + kViewportActor);
    uint32_t state = unreal::StateOf(pc);
    input::View view = input::View::Normal;
    float zoom_speed = 0.0f;
    if (state == unreal::Name("s_EEV")) {
        view = input::View::Eev;
    } else if (state == unreal::Name("s_ScriptedEvent")) {
        view = input::View::ScriptedEvent;
    } else if (state == unreal::Name("s_Camera")) {
        uint32_t base_offset = unreal::Offset(pc, unreal::Name("Base"));
        uint32_t camera = base_offset ? R32(pc + base_offset) : 0;
        bool sticky = unreal::IsA(camera, unreal::Name("EStickyCamera"));
        uint32_t speed = sticky ? unreal::Offset(camera, unreal::Name("ZoomSpeed")) : 0;
        if (speed) {
            view = input::View::StickyCamera;
            std::memcpy(&zoom_speed, HostPtr(camera + speed), 4);
        }
    }
    input::SetView(view, zoom_speed);
    if (view == input::View::Eev) Eev(pc);
    else g_in_eev = false;
}

}  // namespace game
