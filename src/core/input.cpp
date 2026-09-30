#include "core/input.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xinput.h>

#include "app/app.h"
#include "core/log.h"
#include "image_symbols.h"

namespace input {

namespace {

std::atomic<bool> g_blocked{false}, g_menu{false};
// keys held while the game had no focus (the focusing click, Escape closing the options) wait for release
std::atomic<bool> g_settle{false};
// a click that opened or closed a menu must not reach the game, the mouse buttons wait for release; keys stay held
// through the change, so a held button that opens a menu (quick inventory) keeps it open
std::atomic<bool> g_settle_mouse{false};
HWND g_hwnd = nullptr;
std::atomic<int> g_dx{0}, g_dy{0};
std::atomic<uint32_t> g_clicks{0};

// While the keyboard or mouse was used after the pad's last new press, port 0's pad does not rumble. Only new
// pad presses count, so a drifting stick never takes over from the keyboard.
std::atomic<uint64_t> g_keyboard_ms{0}, g_pad_ms{0};
std::atomic<uint32_t> g_keys_held{0}, g_pad_held{0};
std::atomic<bool> g_rumbling{false};  // port 0's motors were last set running
std::atomic<uint64_t> g_movie_ms{0};  // the last movie frame
constexpr uint64_t kMovieMs = 250;

uint64_t NowMs() {
    auto since = std::chrono::steady_clock::now().time_since_epoch();
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(since).count()) + 1;
}

bool KeyboardLast() { return g_keyboard_ms > g_pad_ms; }

std::atomic<int> g_view{0};
std::atomic<float> g_zoom_speed{0.0f};
std::atomic<int> g_wheel{0};
std::atomic<uint16_t> g_tap{0};
std::atomic<uint64_t> g_tap_until{0};
constexpr uint64_t kTapMs = 80;

// As SCCT's PC sticky camera: a wheel notch zooms 105 degrees a second for one 30 Hz tick. The stick tops out
// at the camera's zoom speed, so the notch holds it at full for as long as the same angle takes.
constexpr float kNotchDegrees = 105.0f / 30.0f;
float g_zoom_left = 0.0f;  // seconds of full stick still to hold, negative zooms out
std::chrono::steady_clock::time_point g_zoom_time;

// The sticky camera reads Interaction for gas, Scope for noise, Duck to leave, and Fire and AltFire to switch
// cameras. Fire and alt fire are moved onto gas and noise, pause onto leave, and strafing switches cameras.
struct Remap {
    int from, to;
};
// Interaction is the pad's A and Scope its X.
constexpr Remap kCameraRemap[] = {{7, 0}, {6, 2}, {8 + 4, 1}, {16 + 2, 6}, {16 + 3, 7}};
// the target each held action went down as plus one, 0 while up, so a key cannot stick when the camera opens or
// closes under it
std::array<std::atomic<int>, 32> g_down_as{};

// In a menu pause (Escape) backs out as B, except in the pause menu, which only Start closes.
constexpr Remap kMenuRemap[] = {{8 + 4, 1}};
// In the OPSAT pause (Escape) backs out as B, on its pages that are no menu too.
constexpr Remap kOpsatRemap[] = {{8 + 4, 1}};
std::atomic<bool> g_title{false}, g_pause_menu{false}, g_opsat{false}, g_quick{false};
std::atomic<uint32_t> g_stick_keys{0};  // StickKeys
constexpr uint16_t kStart = 1 << 4;  // Pad::buttons

template <size_t N>
int Remapped(const Remap (&table)[N], int b) {
    for (const Remap& r : table)
        if (r.from == b) return r.to;
    return b;
}

// Lockpicking. The Xbox script (EPickLockSystem) takes the quadrant from the stick's diagonals and tilts the pick
// only when the stick moved by more than MoveThreshold (0.3) since the last tilt; the PC version gives each key one
// quadrant and tilts while it is held. So W, A, S and D stand for the PC's diagonals (up-left, down-left, down-right,
// up-right) and go out as the stick, its length alternating so the pick keeps tilting. Per move key (forward, back,
// left, right): forward and strafe.
constexpr float kPickStick[4][2] = {{1.0f, -1.0f}, {-1.0f, 1.0f}, {-1.0f, -1.0f}, {1.0f, 1.0f}};
constexpr auto kWiggle = std::chrono::milliseconds(40);
int g_pick_last = -1;  // the held move key pressed last
bool g_wiggle = false;
std::chrono::steady_clock::time_point g_wiggle_time;

// Walking speed steps for the move keys, set with the wheel. The stick bindings have a 0.3 dead zone,
// these land the game's axis on 0.31 0.48 0.66 0.83 1.0: 0.31 is SCCT PC's slowest (its AxisMod floor), and
// below 0.3 some states (gun drawn) do not move at all.
constexpr float kSpeeds[] = {0.517f, 0.638f, 0.759f, 0.879f, 1.0f};
constexpr float kDeadZone = 0.3f;
std::atomic<int> g_speed{4};

std::atomic<uint32_t> g_events{0};  // key events pressed in play and not yet taken

std::atomic<bool> g_net{false};
constexpr int kBlack = 4;  // Pad::analog

// what an action presses: 0-7 analog A B X Y black white LT RT, 8-15 the buttons bits, 16-19 the move keys;
// or, with an event, no pad button: the game side acts on it once per press (KeyEvent)
struct Target {
    Action action;
    int button;
    bool play_only = false;  // a pad button that also moves through menus, pressed only in play
    KeyEvent event = KeyEvent::None;
};
constexpr Target kStoryActions[] = {
    {{"move_forward", "MOVE FORWARD", "W"}, 16},
    {{"move_back", "MOVE BACK", "S"}, 17},
    {{"strafe_left", "STRAFE LEFT", "A"}, 18},
    {{"strafe_right", "STRAFE RIGHT", "D"}, 19},
    {{"fire", "FIRE", "LMB"}, 7},
    {{"alt_fire", "ALT FIRE", "RMB"}, 6},
    {{"interact", "INTERACT", "Space,Enter"}, 0},
    {{"jump", "JUMP", "Shift"}, 3},
    {{"crouch", "CROUCH", "C"}, 1},
    {{"scope", "SCOPE", "E,MMB"}, 2},
    {{"back_to_wall", "BACK TO WALL", "Q"}, 14},
    {{"zoom", "EEV / ZOOM", "1"}, 15},
    {{"quick_inventory", "QUICK INVENTORY", "Ctrl"}, 5},
    // the pad's black button brings up a picker between these two; the keys do each at once
    {{"whistle", "WHISTLE", "V"}, -1, true, KeyEvent::Whistle},
    {{"sidekick", "SIDEKICK / CO-OP ACTION", "F"}, -1, true, KeyEvent::SidekickCommand},
    {{"goggles", "EMF VISION", "4"}, 8},
    {{"next_vision", "THERMAL VISION", "3"}, 11},
    {{"previous_vision", "NIGHT VISION", "2"}, 10},
    {{"switch_camera", "SWITCH CAMERA", "5"}, 9, true},
    {{"opsat", "OPSAT", "F1"}, 13},
    {{"pause", "PAUSE", "Escape"}, 12},
    {{"quick_save", "QUICK SAVE", "F5"}, -1, true, KeyEvent::QuickSave},
    {{"quick_load", "QUICK LOAD", "F8"}, -1, true, KeyEvent::QuickLoad},
};
// Versus (sc4_adv.xbe) binds the pad differently. Its own ids keep rebinds in one game.
constexpr Target kVersusActions[] = {
    {{"move_forward", "MOVE FORWARD", "W"}, 16},
    {{"move_back", "MOVE BACK", "S"}, 17},
    {{"strafe_left", "STRAFE LEFT", "A"}, 18},
    {{"strafe_right", "STRAFE RIGHT", "D"}, 19},
    {{"fire", "FIRE", "LMB"}, 7},
    {{"alt_fire", "ALT FIRE", "RMB"}, 6},
    {{"versus_interact", "INTERACT", "Shift"}, 0},
    {{"versus_jump", "JUMP", "Space"}, 3},
    {{"crouch", "CROUCH", "C"}, 1},
    {{"versus_scope", "SCOPE", "E"}, 2},
    {{"versus_zoom", "ZOOM / LOOK BACK", "MMB"}, 15},
    {{"laser", "LASER", "B"}, 8},
    {{"night_vision", "NIGHT VISION", "Tab"}, 10},
    {{"previous_camera", "PREVIOUS CAMERA", "V"}, 9, true},
    {{"help", "HELP", "F1"}, 4},
    {{"pause", "PAUSE", "Escape"}, 12},
};
constexpr bool kVersus = std::string_view(IMAGE_XBE) == "sc4_adv.xbe";
constexpr std::span<const Target> kActions =
    kVersus ? std::span<const Target>(kVersusActions) : std::span<const Target>(kStoryActions);
constexpr size_t kCount = kActions.size();
static_assert(kCount <= 27, "the keyboard activity mask holds the actions in bits 0-26");

struct Name {
    int vk;
    const char* name;
};
constexpr Name kNames[] = {
    {VK_LBUTTON, "LMB"}, {VK_RBUTTON, "RMB"}, {VK_MBUTTON, "MMB"}, {VK_XBUTTON1, "Mouse4"}, {VK_XBUTTON2, "Mouse5"},
    {VK_SPACE, "Space"}, {VK_RETURN, "Enter"}, {VK_ESCAPE, "Escape"}, {VK_BACK, "Backspace"}, {VK_TAB, "Tab"},
    {VK_SHIFT, "Shift"}, {VK_CONTROL, "Ctrl"}, {VK_MENU, "Alt"}, {VK_CAPITAL, "CapsLock"},
    {VK_UP, "Up"}, {VK_DOWN, "Down"}, {VK_LEFT, "Left"}, {VK_RIGHT, "Right"},
    {VK_INSERT, "Insert"}, {VK_DELETE, "Delete"}, {VK_HOME, "Home"}, {VK_END, "End"},
    {VK_PRIOR, "PageUp"}, {VK_NEXT, "PageDown"},
    {'A', "A"}, {'B', "B"}, {'C', "C"}, {'D', "D"}, {'E', "E"}, {'F', "F"}, {'G', "G"}, {'H', "H"}, {'I', "I"},
    {'J', "J"}, {'K', "K"}, {'L', "L"}, {'M', "M"}, {'N', "N"}, {'O', "O"}, {'P', "P"}, {'Q', "Q"}, {'R', "R"},
    {'S', "S"}, {'T', "T"}, {'U', "U"}, {'V', "V"}, {'W', "W"}, {'X', "X"}, {'Y', "Y"}, {'Z', "Z"},
    {'0', "0"}, {'1', "1"}, {'2', "2"}, {'3', "3"}, {'4', "4"}, {'5', "5"}, {'6', "6"}, {'7', "7"}, {'8', "8"}, {'9', "9"},
    {VK_F1, "F1"}, {VK_F2, "F2"}, {VK_F4, "F4"}, {VK_F5, "F5"}, {VK_F6, "F6"}, {VK_F7, "F7"},
    {VK_F8, "F8"}, {VK_F9, "F9"}, {VK_F11, "F11"}, {VK_F12, "F12"},
    {VK_NUMPAD0, "Numpad0"}, {VK_NUMPAD1, "Numpad1"}, {VK_NUMPAD2, "Numpad2"}, {VK_NUMPAD3, "Numpad3"},
    {VK_NUMPAD4, "Numpad4"}, {VK_NUMPAD5, "Numpad5"}, {VK_NUMPAD6, "Numpad6"}, {VK_NUMPAD7, "Numpad7"},
    {VK_NUMPAD8, "Numpad8"}, {VK_NUMPAD9, "Numpad9"}, {VK_ADD, "NumpadPlus"}, {VK_SUBTRACT, "NumpadMinus"},
    {VK_MULTIPLY, "NumpadStar"}, {VK_DIVIDE, "NumpadSlash"}, {VK_DECIMAL, "NumpadPeriod"},
    {VK_OEM_3, "Backtick"}, {VK_OEM_MINUS, "Minus"}, {VK_OEM_PLUS, "Equals"}, {VK_OEM_4, "LBracket"},
    {VK_OEM_6, "RBracket"}, {VK_OEM_5, "Backslash"}, {VK_OEM_1, "Semicolon"}, {VK_OEM_7, "Quote"},
    {VK_OEM_COMMA, "Comma"}, {VK_OEM_PERIOD, "Period"}, {VK_OEM_2, "Slash"},
};

// arrows move at full tilt so menus always see a press, Backspace always backs out of a menu
constexpr int kArrows[] = {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT};

std::mutex g_bind_lock;
std::array<std::vector<int>, kCount> g_binds;

int Vk(const std::string& name) {
    for (const Name& n : kNames)
        if (_stricmp(n.name, name.c_str()) == 0) return n.vk;
    return 0;
}

const char* NameOf(int vk) {
    for (const Name& n : kNames)
        if (n.vk == vk) return n.name;
    return nullptr;
}

std::vector<int> Parse(const std::string& list) {
    std::vector<int> out;
    size_t start = 0;
    while (start <= list.size()) {
        size_t comma = list.find(',', start);
        if (comma == std::string::npos) comma = list.size();
        if (int vk = Vk(list.substr(start, comma - start))) out.push_back(vk);
        start = comma + 1;
    }
    return out;
}

std::string Join(const std::vector<int>& vks) {
    std::string s;
    for (int vk : vks) {
        if (!s.empty()) s += ',';
        s += NameOf(vk);
    }
    return s;
}

bool Active() { return g_hwnd && GetForegroundWindow() == g_hwnd && !g_blocked; }
bool Down(int vk) { return GetAsyncKeyState(vk) < 0; }
bool Mouse(int vk) { return vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON || vk == VK_XBUTTON1 || vk == VK_XBUTTON2; }

void Keyboard(Pad& p) {
    std::array<std::vector<int>, kCount> binds;
    {
        std::lock_guard lock(g_bind_lock);
        binds = g_binds;
    }
    bool menu = g_menu;
    if (g_settle) {
        for (auto& b : binds)
            for (int vk : b)
                if (Down(vk)) return;
        for (int vk : kArrows)
            if (Down(vk)) return;
        if (Down(VK_BACK)) return;
        g_settle = false;
    }
    if (g_settle_mouse && !Down(VK_LBUTTON) && !Down(VK_RBUTTON) && !Down(VK_MBUTTON) && !Down(VK_XBUTTON1) &&
        !Down(VK_XBUTTON2))
        g_settle_mouse = false;
    bool no_mouse = menu || g_settle_mouse;
    bool camera = View(g_view.load()) == View::StickyCamera && !menu;
    int move[4] = {};
    uint32_t keys = 0;
    for (size_t i = 0; i < kCount; i++) {
        bool held = false;
        // in a menu the mouse buttons belong to the cursor
        for (int vk : binds[i]) held |= Down(vk) && !(no_mouse && Mouse(vk));
        if (kActions[i].play_only && menu) held = false;
        if (!held) {
            g_down_as[i] = 0;
            continue;
        }
        keys |= 1u << i;
        if (kActions[i].event != KeyEvent::None && g_net) {
            if (kActions[i].event == KeyEvent::SidekickCommand) p.analog[kBlack] = 0xFF;
            g_down_as[i] = 1;
            continue;
        }
        if (kActions[i].event != KeyEvent::None) {
            if (!g_down_as[i]) g_events |= 1u << int(kActions[i].event);
            g_down_as[i] = 1;
            continue;
        }
        int b = g_down_as[i] - 1;
        if (b < 0) {
            int button = kActions[i].button;
            b = camera                    ? Remapped(kCameraRemap, button)
                : g_opsat               ? Remapped(kOpsatRemap, button)
                : menu && !g_pause_menu ? Remapped(kMenuRemap, button)
                                        : button;
            g_down_as[i] = b + 1;
            if (b >= 16) g_pick_last = b - 16;
        }
        if (b >= 16) move[b - 16] = 1;
        else if (b < 8) p.analog[b] = 0xFF;
        else p.buttons |= uint16_t(1u << (b - 8));
        if (b == 0 && g_title) p.buttons |= kStart;
    }
    if (Down(VK_BACK) || (menu && Down(VK_RBUTTON))) p.analog[1] = 0xFF;
    // Enter confirms in every menu whatever interact is bound to
    if (menu && Down(VK_RETURN)) {
        p.analog[0] = 0xFF;
        if (g_title) p.buttons |= kStart;
    }
    if (!menu && View(g_view.load()) == View::ScriptedEvent && Down(VK_RETURN)) p.buttons |= kStart;
    for (int i = 0; i < 4; i++) keys |= uint32_t(Down(kArrows[i])) << (27 + i);
    keys |= uint32_t(Down(VK_BACK)) << 31;
    if (keys & ~g_keys_held.exchange(keys)) g_keyboard_ms = NowMs();
    int x = move[3] - move[2], y = move[0] - move[1];
    // the camera zooms at the stick's rate times its zoom speed, so it gets the full stick
    bool full = camera || menu || g_quick || View(g_view.load()) == View::Ledge;
    float v = full ? 1.0f : kSpeeds[g_speed];
    auto now = std::chrono::steady_clock::now();
    float seconds = std::min(0.1f, std::chrono::duration<float>(now - g_zoom_time).count());
    g_zoom_time = now;
    float zoom_speed = g_zoom_speed;
    if (!camera || zoom_speed <= 0.0f) {
        g_zoom_left = 0.0f;
    } else {
        if (int notches = g_wheel.exchange(0)) {
            if (notches * g_zoom_left < 0.0f) g_zoom_left = 0.0f;
            g_zoom_left += float(notches) * kNotchDegrees / zoom_speed;
        }
        if (g_zoom_left > 0.0f) g_zoom_left = std::max(0.0f, g_zoom_left - seconds);
        if (g_zoom_left < 0.0f) g_zoom_left = std::min(0.0f, g_zoom_left + seconds);
        if (!y && g_zoom_left != 0.0f) y = g_zoom_left > 0.0f ? 1 : -1;
    }
    if (int ax = Down(VK_RIGHT) - Down(VK_LEFT), ay = Down(VK_UP) - Down(VK_DOWN); ax || ay) {
        x = ax;
        y = ay;
        v = 1.0f;
    }
    // The dead zone is per axis, so a diagonal puts each axis at 1/sqrt(2) of the speed past it and two keys move
    // as fast as one. Not where the full stick is forced: a ledge climb wants aForward past 0.9.
    if (x && y && !full) v = kDeadZone + (v - kDeadZone) * 0.70710678f;
    if (x) p.lx = int16_t(float(x) * v * 32767.0f);
    if (y) p.ly = int16_t(float(y) * v * 32767.0f);
    if (View(g_view.load()) != View::Lockpick || menu) {
        g_stick_keys = (y > 0 ? 1u : 0u) | (y < 0 ? 2u : 0u) | (x < 0 ? 4u : 0u) | (x > 0 ? 8u : 0u);
        return;
    }
    if (g_pick_last < 0 || !move[g_pick_last]) {
        g_pick_last = -1;
        for (int i = 0; i < 4; i++)
            if (move[i]) g_pick_last = i;
    }
    if (g_pick_last < 0) return;
    if (now - g_wiggle_time >= kWiggle) {
        g_wiggle = !g_wiggle;
        g_wiggle_time = now;
    }
    float length = g_wiggle ? 1.0f : 0.6f;
    p.ly = int16_t(kPickStick[g_pick_last][0] * length * 32767.0f);
    p.lx = int16_t(kPickStick[g_pick_last][1] * length * 32767.0f);
}

void Clip() {
    RECT r, cur;
    GetClientRect(g_hwnd, &r);
    MapWindowPoints(g_hwnd, nullptr, reinterpret_cast<POINT*>(&r), 2);
    if (!GetClipCursor(&cur) || !EqualRect(&r, &cur)) ClipCursor(&r);
}

}  // namespace

void SetBlocked(bool b) {
    g_blocked = b;
    if (b) ReleaseCursor();
}

void Init(void* hwnd) {
    g_hwnd = static_cast<HWND>(hwnd);
    RAWINPUTDEVICE mouse{1, 2, 0, g_hwnd};
    RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
}

void OnRawInput(void* handle) {
    RAWINPUT ri;
    UINT size = sizeof(ri);
    if (GetRawInputData(static_cast<HRAWINPUT>(handle), RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) == UINT(-1) ||
        ri.header.dwType != RIM_TYPEMOUSE || !Active())
        return;
    // only the clip: settling would drop a held key that keeps a menu open (quick inventory)
    if (g_menu) ClipCursor(nullptr);
    else Clip();
    const RAWMOUSE& m = ri.data.mouse;
    if (m.lLastX || m.lLastY || m.usButtonFlags) g_keyboard_ms = NowMs();
    if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE)) {
        g_dx += m.lLastX;
        g_dy += m.lLastY;
    }
    if ((m.usButtonFlags & RI_MOUSE_WHEEL) && !g_menu) {
        int notch = SHORT(m.usButtonData) > 0 ? 1 : -1;
        int s = g_speed + notch;
        if (View view = View(g_view.load()); view == View::Eev || view == View::StickyCamera) g_wheel += notch;
        else if (s >= 0 && s < int(std::size(kSpeeds))) g_speed = s;
    }
}

void OnClick() {
    if (Active() && g_menu) g_clicks++;
}

void ReleaseCursor() {
    ClipCursor(nullptr);
    g_settle = true;
}

bool TakeMouse(float& dx, float& dy) {
    int x = g_dx.exchange(0), y = g_dy.exchange(0);
    if ((!x && !y) || !Active() || g_menu) return false;
    const app::Settings& s = app::Get();
    dx = float(x) * s.mouse_sensitivity;
    dy = float(s.invert_mouse ? -y : y) * s.mouse_sensitivity;
    return true;
}

void SetTitleScreen(bool title) { g_title = title; }

void SetPauseMenu(bool pause) { g_pause_menu = pause; }

void SetOpsat(bool opsat) { g_opsat = opsat; }

void SetQuickInventory(bool open) { g_quick = open; }

void SetMenu(bool menu) {
    if (g_menu.exchange(menu) != menu) {
        if (menu) ClipCursor(nullptr);
        g_settle_mouse = true;
    }
}

bool InMenu() { return g_menu; }

void MovieFrame() {
    uint64_t now = NowMs();
    if (now - g_movie_ms.exchange(now) > kMovieMs)
        PostMessageW(g_hwnd, WM_SETCURSOR, WPARAM(g_hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}

bool Movie() { return NowMs() - g_movie_ms < kMovieMs; }

bool KeyboardActive() { return g_keyboard_ms >= g_pad_ms; }

void SetView(View view, float zoom_speed) {
    if (g_view.exchange(int(view)) != int(view)) g_wheel = 0;
    g_zoom_speed = zoom_speed;
}

int TakeWheel() { return g_wheel.exchange(0); }

uint32_t StickKeys() { return g_stick_keys; }

void SetNetSession(bool net) {
    if (g_net.exchange(net) != net) LOGI("input: %s a synced co-op session", net ? "in" : "out of");
}

bool TakeEvent(KeyEvent event) { return g_events.fetch_and(~(1u << int(event))) >> int(event) & 1; }

void TapButtons(uint16_t buttons) {
    g_tap = buttons;
    g_tap_until = NowMs() + kTapMs;
}

namespace {

// "press a key": a key, mouse button or pad button (any port) going down in the game window; those held when the
// watch starts must come up first
struct PressWatch {
    std::array<bool, 256> held{};
    uint64_t pad_held = 0;
    bool started = false;
};

uint64_t Pads() {
    uint64_t down = 0;
    for (DWORD port = 0; port < 4; port++) {
        XINPUT_STATE s{};
        if (XInputGetState(port, &s) != ERROR_SUCCESS) continue;
        // the triggers take the two bits XInput leaves unused
        uint32_t bits = s.Gamepad.wButtons | uint32_t(s.Gamepad.bLeftTrigger > 30) << 10 |
                        uint32_t(s.Gamepad.bRightTrigger > 30) << 11;
        down |= uint64_t(bits) << (port * 16);
    }
    return down;
}

// F10, F3 and the console key belong to the host; modifiers (Alt+Tab), the Windows keys, Print Screen, the lock
// keys and the media keys are no answer to "press a key", nor are codes Windows leaves unassigned or reserved: some
// keyboards' Print Screen comes as 0xFF, and the 0xC1..0xDA block is the pads' own
bool AnswerKey(int vk) {
    switch (vk) {
        case VK_F10: case VK_F3: case VK_OEM_3: case VK_SNAPSHOT: case VK_SHIFT: case VK_CONTROL: case VK_MENU:
        case VK_LSHIFT: case VK_RSHIFT: case VK_LCONTROL: case VK_RCONTROL: case VK_LMENU: case VK_RMENU:
        case VK_LWIN: case VK_RWIN: case VK_APPS: case VK_CAPITAL: case VK_NUMLOCK: case VK_SCROLL: case VK_PAUSE:
        case 0x07: case 0x0A: case 0x0B: case 0x0E: case 0x0F: case 0x5E: case 0xE0: case 0xE8: case 0xFF:
            return false;
    }
    if ((vk >= VK_BROWSER_BACK && vk <= VK_LAUNCH_APP2) || (vk >= 0x3A && vk <= 0x40) || (vk >= 0x88 && vk <= 0x9F) ||
        (vk >= 0xC1 && vk <= 0xDA))
        return false;
    return Down(vk);
}

// the key (virtual key code) or pad button (0x100 + port * 16 + bit) that went down, 0 for none. Away from the
// window (Snipping Tool after Print Screen, Alt+Tab) it starts over, so the key or click that brings the game back
// is not an answer either.
int NewPress(PressWatch& w) {
    bool live = w.started && Active();
    int pressed = 0;
    uint64_t pad = Pads(), fresh = pad & ~w.pad_held;
    if (live && fresh) pressed = 0x100 + std::countr_zero(fresh);
    w.pad_held = pad;
    for (int vk = 1; vk < 256; vk++) {
        bool d = AnswerKey(vk);
        if (live && d && !w.held[vk] && !pressed) pressed = vk;
        w.held[vk] = d;
    }
    w.started = Active();
    return pressed;
}

}  // namespace

void WaitForPress() {
    // the picture is no menu: the cursor goes, now rather than at the next mouse move
    SetMenu(false);
    PostMessageW(g_hwnd, WM_SETCURSOR, WPARAM(g_hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    PressWatch w;
    NewPress(w);
    int pressed;
    while (!(pressed = NewPress(w))) Sleep(10);
    bool pad = pressed >= 0x100;
    LOGI("input: the wait ended by %s %X", pad ? "pad button" : "key", pad ? pressed - 0x100 : pressed);
}

bool Pressed() {
    static PressWatch w;
    return NewPress(w) != 0;
}

bool Cursor(int& x, int& y, int& w, int& h) {
    POINT pt;
    RECT r;
    if (!Active() || !GetCursorPos(&pt) || !ScreenToClient(g_hwnd, &pt) || !GetClientRect(g_hwnd, &r)) return false;
    x = pt.x;
    y = pt.y;
    w = r.right;
    h = r.bottom;
    return true;
}

uint32_t Clicks() { return g_clicks; }

const Action* Actions(size_t* count) {
    static const auto list = [] {
        std::array<Action, kCount> a{};
        for (size_t i = 0; i < kCount; i++) a[i] = kActions[i].action;
        return a;
    }();
    *count = kCount;
    return list.data();
}

std::string Binding(size_t action) {
    std::lock_guard lock(g_bind_lock);
    return action < kCount ? Join(g_binds[action]) : std::string();
}

void Bind(size_t action, int vk) {
    if (action >= kCount || !NameOf(vk)) return;
    {
        std::lock_guard lock(g_bind_lock);
        g_binds[action] = {vk};
        app::Get().keys[kActions[action].action.id] = Join(g_binds[action]);
    }
    app::Save();
}

void ResetBindings() {
    app::Get().keys.clear();
    LoadBindings();
    app::Save();
}

void LoadBindings() {
    std::lock_guard lock(g_bind_lock);
    const auto& keys = app::Get().keys;
    for (size_t i = 0; i < kCount; i++) {
        auto it = keys.find(kActions[i].action.id);
        g_binds[i] = Parse(it != keys.end() ? it->second : kActions[i].action.defaults);
    }
}

int PressedKey() {
    for (const Name& n : kNames)
        if (Down(n.vk)) return n.vk;
    return 0;
}

Pad Poll(int port) {
    Pad p;
    XINPUT_STATE s{};
    bool pad = XInputGetState(DWORD(port), &s) == ERROR_SUCCESS;
    p.connected = pad || port == 0;
    if (!p.connected || g_blocked) return p;
    if (pad) {
        const XINPUT_GAMEPAD& g = s.Gamepad;
        auto b = [&](WORD m) { return (g.wButtons & m) != 0; };
        p.buttons = uint16_t((b(XINPUT_GAMEPAD_DPAD_UP) << 0) | (b(XINPUT_GAMEPAD_DPAD_DOWN) << 1) |
                             (b(XINPUT_GAMEPAD_DPAD_LEFT) << 2) | (b(XINPUT_GAMEPAD_DPAD_RIGHT) << 3) |
                             (b(XINPUT_GAMEPAD_START) << 4) | (b(XINPUT_GAMEPAD_BACK) << 5) |
                             (b(XINPUT_GAMEPAD_LEFT_THUMB) << 6) | (b(XINPUT_GAMEPAD_RIGHT_THUMB) << 7));
        auto a = [&](WORD m) { return uint8_t(b(m) ? 0xFF : 0); };
        p.analog[0] = a(XINPUT_GAMEPAD_A);
        p.analog[1] = a(XINPUT_GAMEPAD_B);
        p.analog[2] = a(XINPUT_GAMEPAD_X);
        p.analog[3] = a(XINPUT_GAMEPAD_Y);
        p.analog[4] = a(XINPUT_GAMEPAD_RIGHT_SHOULDER);  // black
        p.analog[5] = a(XINPUT_GAMEPAD_LEFT_SHOULDER);   // white
        p.analog[6] = g.bLeftTrigger;
        p.analog[7] = g.bRightTrigger;
        p.lx = g.sThumbLX;
        p.ly = g.sThumbLY;
        p.rx = g.sThumbRX;
        p.ry = g.sThumbRY;
        if (port == 0) {
            uint32_t held = p.buttons;
            for (int i = 0; i < 8; i++) held |= uint32_t(p.analog[i] > 30) << (8 + i);
            int bit = 16;
            for (int16_t axis : {p.lx, p.ly, p.rx, p.ry}) held |= uint32_t(std::abs(int(axis)) > 8000) << bit++;
            if (held & ~g_pad_held.exchange(held)) g_pad_ms = NowMs();
        }
    }
    // a motor the game left running when the keyboard took over, the game may not set it again
    if (port == 0 && g_rumbling && KeyboardLast()) {
        XINPUT_VIBRATION off{};
        XInputSetState(0, &off);
        g_rumbling = false;
    }
    if (port == 0) g_stick_keys = 0;
    if (port == 0 && Active()) Keyboard(p);
    if (port == 0 && NowMs() < g_tap_until) p.buttons |= g_tap;
    return p;
}

void Rumble(int port, uint16_t left, uint16_t right) {
    if (port == 0 && KeyboardLast()) left = right = 0;
    if (port == 0) g_rumbling = left || right;
    XINPUT_VIBRATION v{left, right};
    XInputSetState(DWORD(port), &v);
}

}  // namespace input
