#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

// the host side: window, settings, options menu
namespace app {

enum class DisplayMode : uint32_t { Fullscreen, Borderless, Windowed };

struct Settings {
    DisplayMode display_mode = DisplayMode::Borderless;
    uint32_t width = 0, height = 0;  // 0 is the desktop mode
    uint32_t render_percent = 100;   // of the output height
    uint32_t fps_cap = 60;           // 0 unlimited
    bool fps_counter = false;
    bool vsync = true;
    uint32_t aniso = 0;              // 0 as the game asks
    float mouse_sensitivity = 1.0f;
    bool invert_mouse = false;
    bool skip_intros = false;        // logos video and rating screen at boot
    uint32_t coop_tick_rate = 0;           // co-op step frequency, Hz; 0 is the game's 22
    uint32_t coop_input_delay_floor = 50;  // ms, the game's 50
    std::map<std::string, std::string> keys;  // action id to key names, absent is the default
};

Settings& Get();
void Load();
void Save();
void ApplyDisplay();  // any thread
struct Mode {
    uint32_t width, height;
};
Mode Desktop();
Mode Output();
float RenderScale(uint32_t guest_height);

std::filesystem::path ExeDir();
int Instance();  // --instance N on the command line, 0 without: a second copy on this PC for system link tests
void* Window();  // HWND

// options menu, drawn by the renderer each frame with the lock held
std::recursive_mutex& UiLock();
bool MenuOpen();
bool Capturing();  // a key binding waits for a key
void ToggleMenu();
void DrawUi(double fps);

// the console on the backtick key, drawn by DrawUi
bool ConsoleOpen();
void ToggleConsole();
void DrawConsole();

// sleeps out the rest of the frame for the fps cap
void Pace();

}  // namespace app
