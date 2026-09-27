#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

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
void SetBlocked(bool blocked);  // the options panel takes the input

// window thread
void OnRawInput(void* handle);
void OnClick();
void ReleaseCursor();
// raw mouse counts since the last call, scaled by the settings; false when there are none
bool TakeMouse(float& dx, float& dy);

// a game menu takes the input: the cursor is free and the mouse buttons stop firing
void SetMenu(bool menu);
bool InMenu();
// the cursor in window client pixels and the client size; false when the game is not taking input
bool Cursor(int& x, int& y, int& w, int& h);
uint32_t Clicks();

// actions the keyboard and mouse can be bound to, key names comma separated
struct Action {
    const char* id;
    const char* label;
    const char* defaults;
};
const Action* Actions(size_t* count);
std::string Binding(size_t action);
void Bind(size_t action, int vk);
void ResetBindings();
void LoadBindings();  // after the settings load
int PressedKey();     // a bindable key or mouse button held now, 0 for none

}  // namespace input
