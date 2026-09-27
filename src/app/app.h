#pragma once
#include <cstdint>
#include <filesystem>
#include <mutex>

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
void* Window();  // HWND

// options menu, drawn by the renderer each frame with the lock held
std::recursive_mutex& UiLock();
bool MenuOpen();
void ToggleMenu();
void DrawUi(double fps);

// sleeps out the rest of the frame for the fps cap
void Pace();

}  // namespace app
