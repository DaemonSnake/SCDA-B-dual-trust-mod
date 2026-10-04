// Mouse look in versus. The controller's native PlayerInput (sub_0003FB80) turns aTurn / aLookUp into the look
// rates it leaves at +0x4D8 / +0x4DC (+-1000 is full stick) through a 220 dead zone, a clamp to full stick, a response
// curve and smoothing; the mouse replaces its outputs after it, so none of those touch it. The binoculars drop the
// rates while the stick magnitude it leaves is 0, so that follows the mouse as well. Divided by dt, a count turns the
// same at any frame rate.
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <iterator>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"
#include "game/magma.h"
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

// Menus, as in story: the cursor focuses the control under it and selects a list's row, a click presses A or
// scrolls on a list's arrows, the wheel steps up and down.
// manager: +0x14 begin, +0x18 end of the input stack's {page, flags}; drawer: +0xAC page width, +0xAE height
constexpr uint32_t kMagma = 0x3D8D74, kDrawer = 0x3D8D8C;
// page: +4 id, +0xC begin, +0x10 end of instances, +0x24 focused
// instance: +4 shown, +5 hidden, +0x10 widget, +0xB8 flags (2 enabled, 8 focused), vtable +0xC Focus(direction, notify)
// widget vtable: +0xC type, +0x10 GetBounds(short rect[4] left right top bottom), +0x28 MouseDown and +0x2C MouseUp
// (instance, event: +4 point), +0x3C MouseLeave(instance)
// list: +0x30 up arrow, +0x44 down arrow, +0x54 selection, +0x88 up arrow rect, +0x98 down arrow rect
constexpr uint32_t kListType = 8, kSliderType = 10;
constexpr uint32_t kPageSetFocus = 0x25C420, kListHitTest = 0x25DF00, kListSelect = 0x25EE00;
constexpr uint32_t kHudPages[] = {game::magma::Id("p_hudatt"), game::magma::Id("p_interatt")};
constexpr uint32_t kPopUp = game::magma::Id("p_popupmessageauto");
// viewport +0x18 client, +0x28 engine, vtable +0x7C InputEvent(viewport, key, action, delta)
constexpr uint32_t kKeyA = 0xC8, kKeyUp = 0xD0, kKeyDown = 0xD1, kPress = 1, kRelease = 3;

struct Ui {
    X86Context& ctx;
    uint32_t viewport, scratch;  // scratch: point at +0, rect at +8, mouse event at +0x10
};

uint32_t Call(Ui& u, uint32_t fn, uint32_t self, std::initializer_list<uint32_t> args = {}) {
    u.ctx.ecx = self;
    return CallGuest(u.ctx, fn, args);
}

uint32_t Virtual(Ui& u, uint32_t self, uint32_t offset, std::initializer_list<uint32_t> args = {}) {
    return Call(u, R32(R32(self) + offset), self, args);
}

void Tap(Ui& u, uint32_t key) {
    uint32_t engine = R32(R32(u.viewport + 0x18) + 0x28);
    Virtual(u, engine, 0x7C, {u.viewport, key, kPress, 0});
    Virtual(u, engine, 0x7C, {u.viewport, key, kRelease, 0});
}

bool Shown(uint32_t instance) { return G<uint8_t>(instance + 4) && !G<uint8_t>(instance + 5); }

bool Focusable(Ui& u, uint32_t instance) {
    uint32_t widget = R32(instance + 0x10);
    if (!widget || !Shown(instance)) return false;
    switch (Virtual(u, widget, 0xC)) {
        case 1: case 2: case 3: case 7: case 8: case 10: return G<uint8_t>(instance + 0xB8) & 2;
        default: return false;
    }
}

bool Inside(Ui& u, uint32_t rect) {
    int16_t x = int16_t(R16(u.scratch)), y = int16_t(R16(u.scratch + 2));
    int16_t l = int16_t(R16(rect)), r = int16_t(R16(rect + 2)), t = int16_t(R16(rect + 4)), b = int16_t(R16(rect + 6));
    return l < r && t < b && x >= l && x <= r && y >= t && y <= b;
}

uint32_t g_page = 0, g_clicks = 0, g_point = ~0u;

// window cursor to page space: pages are 4:3 in the middle of a 16:9 frame
bool Point(uint32_t& point) {
    int x, y, w, h;
    if (!input::Cursor(x, y, w, h) || w <= 0 || h <= 0) return false;
    uint32_t drawer = R32(kDrawer);
    float sw = drawer ? float(int16_t(R16(drawer + 0xAC))) : 0, sh = drawer ? float(int16_t(R16(drawer + 0xAE))) : 0;
    if (sw <= 0 || sh <= 0) sw = 640, sh = 480;
    bool wide = app::Widescreen();
    float aspect = wide ? 16.0f / 9.0f : 4.0f / 3.0f;
    float vw = std::min(float(w), h * aspect), vh = vw / aspect;
    float fx = (x - (w - vw) / 2) / vw;
    if (wide) fx = (fx - 0.125f) / 0.75f;
    int px = int(fx * sw), py = int((y - (h - vh) / 2) / vh * sh);
    point = uint32_t(uint16_t(int16_t(px))) | uint32_t(uint16_t(int16_t(py))) << 16;
    return true;
}

// true while a menu takes the input: the top input page shows its focused control, or is the "press A" pop up.
// The HUD pages keep a hidden list focused through play, shown for the pad alone.
bool Menu(Ui& u) {
    uint32_t m = R32(kMagma), first = m ? R32(m + 0x14) : 0, last = m ? R32(m + 0x18) : 0;
    uint32_t top = first && last > first ? R32(last - 8) : 0;
    uint32_t id = top ? R32(top + 4) : 0, focus = top ? R32(top + 0x24) : 0;
    uint32_t point = g_point, clicks = input::Clicks();
    // a new page starts from where the cursor rests
    bool fresh = top != g_page;
    bool moved = Point(point) && point != g_point && !fresh, clicked = clicks != g_clicks && !fresh;
    g_page = top;
    g_point = point;
    g_clicks = clicks;
    bool hud = std::ranges::find(kHudPages, id) != std::end(kHudPages);
    if (hud || !(id == kPopUp || (focus && Shown(focus)))) return false;
    if (id == kPopUp && clicked) Tap(u, kKeyA);
    if (id == kPopUp || (!moved && !clicked)) return true;
    W32(u.scratch, point);
    uint32_t hovered = 0;
    for (uint32_t it = R32(top + 0xC), end = R32(top + 0x10); it < end; it += 4) {
        uint32_t instance = R32(it);
        if (!instance || !Focusable(u, instance)) continue;
        Virtual(u, R32(instance + 0x10), 0x10, {u.scratch + 8});
        if (Inside(u, u.scratch + 8) && (!hovered || instance == focus)) hovered = instance;
    }
    if (!hovered) return true;
    if (hovered != focus) Call(u, kPageSetFocus, top, {hovered});
    uint32_t widget = R32(hovered + 0x10), type = Virtual(u, widget, 0xC);
    if (type == kSliderType && clicked) {
        // its own mouse handlers step on the arrows
        W32(u.scratch + 0x10, 0);
        W32(u.scratch + 0x14, point);
        Virtual(u, widget, 0x28, {hovered, u.scratch + 0x10});
        Virtual(u, widget, 0x2C, {hovered, u.scratch + 0x10});
        Virtual(u, widget, 0x3C, {hovered});
    }
    if (type != kListType) {
        if (clicked && type != kSliderType) Tap(u, kKeyA);
        return true;
    }
    int32_t hit = int32_t(Call(u, kListHitTest, widget, {u.scratch}));
    if (hit >= 0 && hit != int32_t(R32(widget + 0x54))) {
        Call(u, kListSelect, widget, {uint32_t(hit), 0});
        Virtual(u, hovered, 0xC, {0, 1});
    }
    if (!clicked) return true;
    if (R32(widget + 0x30) && Inside(u, widget + 0x88)) Tap(u, kKeyUp);
    else if (R32(widget + 0x44) && Inside(u, widget + 0x98)) Tap(u, kKeyDown);
    else if (hit >= 0) Tap(u, kKeyA);
    return true;
}

}  // namespace

// thiscall(viewport, reset, float delta seconds), ret 8
X86_EXTERN_FUNC(__imp__sub_00188E80);
X86_FUNC(sub_00188E80) {
    uint32_t viewport = ctx.ecx;
    __imp__sub_00188E80(ctx, base);
    g_player = R32(viewport + kViewportActor);
    Zoom(g_player);
    // guest calls go below a scratch block under the caller's stack
    uint32_t esp = ctx.esp;
    Ui u{ctx, viewport, (esp - 0x40) & ~0xFu};
    ctx.esp = u.scratch - 0x10;
    bool menu = Menu(u);
    input::SetMenu(menu);
    for (int n = input::TakeMenuWheel(); menu && n; n += n > 0 ? -1 : 1) Tap(u, n > 0 ? kKeyUp : kKeyDown);
    ctx.esp = esp;
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
}
