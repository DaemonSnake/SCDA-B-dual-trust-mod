#pragma once
#include <cstdint>

namespace input {

// the console controller's report, analog buttons are 0-255
struct Pad {
    bool connected = false;
    uint16_t buttons = 0;  // dpad up/down/left/right, start, back, left thumb, right thumb
    uint8_t analog[8] = {};  // A B X Y black white left trigger right trigger
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

void Init(void* hwnd);
// keyboard and mouse buttons are merged into port 0, which is always connected
Pad Poll(int port);
void Rumble(int port, uint16_t left, uint16_t right);
void SetBlocked(bool blocked);  // menus take the input

// window thread, WM_INPUT
void OnRawInput(void* handle);
void ReleaseCursor();
// raw mouse counts since the last call, scaled by the settings; false when there are none
bool TakeMouse(float& dx, float& dy);

}  // namespace input
