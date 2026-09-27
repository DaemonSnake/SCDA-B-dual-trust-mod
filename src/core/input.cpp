#include "core/input.h"

#include <atomic>
#include <iterator>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>

#include "app/app.h"

namespace input {

namespace {

std::atomic<bool> g_blocked{false};
// keys held while the game had no focus (the focusing click, Escape closing the options) wait for release
std::atomic<bool> g_settle{false};
HWND g_hwnd = nullptr;
std::atomic<int> g_dx{0}, g_dy{0};

// Walking speed steps for WASD, set with the wheel. The stick bindings have a 0.3 dead zone, these
// land the game's axis on 0.2 0.4 0.6 0.8 1.0.
constexpr float kSpeeds[] = {0.44f, 0.58f, 0.72f, 0.86f, 1.0f};
std::atomic<int> g_speed{4};

// pad button index, 0-7 analog A B X Y black white LT RT, 8-15 the buttons bits
struct Key {
    int vk;
    int button;
};
constexpr Key kKeys[] = {
    {VK_SPACE, 0},    {VK_RETURN, 0},                      // interact
    {'C', 1},         {VK_BACK, 1},                        // crouch, menu back
    {'E', 2},         {VK_MBUTTON, 2},                     // scope
    {VK_SHIFT, 3},                                         // jump
    {'V', 4},                                              // whistle
    {VK_CONTROL, 5},                                       // quick inventory
    {VK_RBUTTON, 6},  {VK_LBUTTON, 7},                     // alt fire, fire
    {'4', 8},         {'1', 9},  {'2', 10}, {'3', 11},     // dpad up down left right
    {VK_ESCAPE, 12},  {VK_F1, 13}, {VK_TAB, 13},           // start, opsat
    {'Q', 14},        {'Z', 15},                           // left and right thumb
};
constexpr int kMoveKeys[] = {'W', 'A', 'S', 'D', VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT};

bool Active() { return g_hwnd && GetForegroundWindow() == g_hwnd && !g_blocked; }
int Down(int vk) { return GetAsyncKeyState(vk) < 0; }

void Keyboard(Pad& p) {
    if (g_settle) {
        for (const Key& k : kKeys)
            if (Down(k.vk)) return;
        for (int vk : kMoveKeys)
            if (Down(vk)) return;
        g_settle = false;
    }
    for (const Key& k : kKeys) {
        if (!Down(k.vk)) continue;
        if (k.button < 8) p.analog[k.button] = 0xFF;
        else p.buttons |= uint16_t(1u << (k.button - 8));
    }
    int x = Down('D') - Down('A'), y = Down('W') - Down('S');
    float v = kSpeeds[g_speed];
    // arrows move at full tilt so menus always see a press
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
    Clip();
    const RAWMOUSE& m = ri.data.mouse;
    if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE)) {
        g_dx += m.lLastX;
        g_dy += m.lLastY;
    }
    if (m.usButtonFlags & RI_MOUSE_WHEEL) {
        int step = SHORT(m.usButtonData) > 0 ? 1 : -1;
        int s = g_speed + step;
        if (s >= 0 && s < int(std::size(kSpeeds))) g_speed = s;
    }
}

void ReleaseCursor() {
    ClipCursor(nullptr);
    g_settle = true;
}

bool TakeMouse(float& dx, float& dy) {
    int x = g_dx.exchange(0), y = g_dy.exchange(0);
    if ((!x && !y) || !Active()) return false;
    const app::Settings& s = app::Get();
    dx = float(x) * s.mouse_sensitivity;
    dy = float(s.invert_mouse ? -y : y) * s.mouse_sensitivity;
    return true;
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
