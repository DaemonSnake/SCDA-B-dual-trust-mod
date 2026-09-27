#include "core/input.h"

#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>

namespace input {

static std::atomic<bool> g_blocked{false};

void SetBlocked(bool b) { g_blocked = b; }

void Init() {}

Pad Poll(int port) {
    Pad p;
    XINPUT_STATE s{};
    if (XInputGetState(DWORD(port), &s) != ERROR_SUCCESS) return p;
    p.connected = true;
    if (g_blocked) return p;
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
    return p;
}

void Rumble(int port, uint16_t left, uint16_t right) {
    XINPUT_VIBRATION v{left, right};
    XInputSetState(DWORD(port), &v);
}

}  // namespace input
