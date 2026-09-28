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
// the title screen, which only Start gets past: Enter and the interact key press it there
void SetTitleScreen(bool title);
// the pause menu, which Start closes and B does not: the pause key stays Start there
void SetPauseMenu(bool pause);
// an OPSAT page, which B closes: the pause key and the OPSAT key are B there
void SetOpsat(bool opsat);
// the quick inventory, which is no menu to the mouse (it keeps looking) but the move keys push the full stick
void SetQuickInventory(bool open);

// What the player looks through, set by the game side each input update. In the EEV and the sticky camera
// the wheel zooms instead of stepping the walking speed. The sticky camera zooms at zoom_speed degrees a second at full stick.
// A scripted event (in-game cinematic) is skipped with Start, which Enter presses there as well as Escape.
// On a ledge, a narrow ladder or a pipe the move keys push the stick fully: climbing wants aForward past 0.9,
// which slow walking speeds never reach. Lockpicking turns the move keys into the stick's diagonals.
enum class View { Normal, StickyCamera, Eev, ScriptedEvent, Ledge, Lockpick };
void SetView(View view, float zoom_speed = 0.0f);
// EEV: wheel notches since the last call, up positive
int TakeWheel();
// the directions the move keys push port 0's stick (up, down, left, right as bits 0-3), whatever the walking speed
uint32_t StickKeys();
// keys the game side acts on itself, once per press in play
enum class KeyEvent { None, Whistle, SidekickCommand, QuickSave, QuickLoad };
// A synced co-op session. Its lockstep carries the pad, not what the game side calls on its own, so there the
// sidekick key holds the black button (co-op action) and whistle is off.
void SetNetSession(bool net);
bool TakeEvent(KeyEvent event);  // pressed since the last call
// holds pad buttons (Pad::buttons bits) long enough for the game to read one press
void TapButtons(uint16_t buttons);

// blocks until a key, mouse button or pad button goes down in the game window; those held on entry must come up
void WaitForPress();
bool InMenu();
// the keyboard or mouse was used after the pad's last new press
bool KeyboardActive();
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
