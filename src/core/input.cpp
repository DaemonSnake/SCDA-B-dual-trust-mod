#include "core/input.h"

#include <array>
#include <atomic>
#include <iterator>
#include <mutex>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>

#include "app/app.h"

namespace input {

namespace {

std::atomic<bool> g_blocked{false}, g_menu{false};
// keys held while the game had no focus (the focusing click, Escape closing the options) wait for release
std::atomic<bool> g_settle{false};
HWND g_hwnd = nullptr;
std::atomic<int> g_dx{0}, g_dy{0};
std::atomic<uint32_t> g_clicks{0};

// Walking speed steps for the move keys, set with the wheel. The stick bindings have a 0.3 dead zone,
// these land the game's axis on 0.2 0.4 0.6 0.8 1.0.
constexpr float kSpeeds[] = {0.44f, 0.58f, 0.72f, 0.86f, 1.0f};
std::atomic<int> g_speed{4};

// what an action presses: 0-7 analog A B X Y black white LT RT, 8-15 the buttons bits, 16-19 the move keys
struct Target {
    Action action;
    int button;
};
constexpr Target kActions[] = {
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
    {{"zoom", "ZOOM TOGGLE", "Z"}, 15},
    {{"quick_inventory", "QUICK INVENTORY", "Ctrl"}, 5},
    {{"whistle", "WHISTLE", "V"}, 4},
    {{"goggles", "GOGGLES", "4"}, 8},
    {{"next_vision", "NEXT VISION MODE", "3"}, 11},
    {{"previous_vision", "PREVIOUS VISION MODE", "2"}, 10},
    {{"switch_camera", "SWITCH CAMERA", "1"}, 9},
    {{"opsat", "OPSAT", "F1,Tab"}, 13},
    {{"pause", "PAUSE", "Escape"}, 12},
};
constexpr size_t kCount = std::size(kActions);

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
    int move[4] = {};
    for (size_t i = 0; i < kCount; i++) {
        bool held = false;
        // in a menu the mouse buttons belong to the cursor
        for (int vk : binds[i]) held |= Down(vk) && !(menu && Mouse(vk));
        if (!held) continue;
        int b = kActions[i].button;
        if (b >= 16) move[b - 16] = 1;
        else if (b < 8) p.analog[b] = 0xFF;
        else p.buttons |= uint16_t(1u << (b - 8));
    }
    if (Down(VK_BACK) || (menu && Down(VK_RBUTTON))) p.analog[1] = 0xFF;
    int x = move[3] - move[2], y = move[0] - move[1];
    float v = kSpeeds[g_speed];
    if (int ax = Down(VK_RIGHT) - Down(VK_LEFT), ay = Down(VK_UP) - Down(VK_DOWN); ax || ay) {
        x = ax;
        y = ay;
        v = 1.0f;
    }
    if (x) p.lx = int16_t(float(x) * v * 32767.0f);
    if (y) p.ly = int16_t(float(y) * v * 32767.0f);
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
    if (g_menu) ReleaseCursor();
    else Clip();
    const RAWMOUSE& m = ri.data.mouse;
    if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE)) {
        g_dx += m.lLastX;
        g_dy += m.lLastY;
    }
    if ((m.usButtonFlags & RI_MOUSE_WHEEL) && !g_menu) {
        int s = g_speed + (SHORT(m.usButtonData) > 0 ? 1 : -1);
        if (s >= 0 && s < int(std::size(kSpeeds))) g_speed = s;
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

void SetMenu(bool menu) {
    if (g_menu.exchange(menu) != menu) {
        if (menu) ClipCursor(nullptr);
        g_settle = true;
    }
}

bool InMenu() { return g_menu; }

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
    }
    if (port == 0 && Active()) Keyboard(p);
    return p;
}

void Rumble(int port, uint16_t left, uint16_t right) {
    XINPUT_VIBRATION v{left, right};
    XInputSetState(DWORD(port), &v);
}

}  // namespace input
