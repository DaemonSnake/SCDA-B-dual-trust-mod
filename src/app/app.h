#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct X86Context;

// the host side: window, settings, options menu
namespace app {

inline constexpr int kVersion = 1;  // release number, the GitHub release tag is V<number>

enum class DisplayMode : uint32_t { Fullscreen, Windowed, Borderless };

// what a player's controller port has plugged in: the keyboard and mouse, a controller, or both
struct Port {
    bool keyboard = false;
    int pad = -1;  // controller slot 0-3, -1 for none
};

struct Settings {
    DisplayMode display_mode = DisplayMode::Borderless;
    uint32_t width = 0, height = 0;  // 0 is the desktop mode
    uint32_t render_percent = 100;   // of the output height
    uint32_t fps_cap = 60;           // 0 unlimited
    uint32_t fov = 75;               // horizontal degrees on a 4:3 frame, 75 is the game's
    bool fps_counter = false;
    bool vsync = true;
    uint32_t aniso = 0;              // 0 as the game asks
    uint32_t msaa = 0;               // samples a pixel of the scene, 0 off
    uint32_t shadow_resolution = 0;  // shadow map edge, 0 follows the render scale; 512 is the console's, edge blur included
    uint32_t shadow_distance = 1;    // lights and their shadows turn off this many times further away, 0 never
    uint32_t shader_model = 3;       // 3 turns on the Chaos Theory PC style features below, 1 is the console's look
    bool hdr_rendering = true;
    bool tone_mapping = true;
    bool hq_soft_shadows = true;
    bool ambient_occlusion = false;  // screen space, from the scene's depth
    bool async_shaders = true;       // a draw whose pipeline needs compiling is skipped until the build threads have it; scda.toml only
    float mouse_sensitivity = 1.0f;
    bool invert_mouse = false;
    bool skip_intros = false;        // logos video and rating screen at boot
    uint32_t coop_tick_rate = 0;           // co-op step frequency, Hz; 0 is the game's 22
    uint32_t coop_input_delay_floor = 50;  // ms, the game's 50
    std::string join_ip;                   // co-op: a host past the LAN the system link search also asks
    std::string microphone;                // recording device by name, empty the system default
    bool push_to_talk = false;             // voice only while the push to talk key is held
    Port players[2] = {{true, 0}, {false, 1}};  // guest ports 0 and 1; 2 and 3 stay empty
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
// tangent of the fov setting's half angle over that of the game's 75 degrees
float FovScale();
// the output is 3:2 or wider: the console reports a widescreen TV and the frame is shown 16:9, else 4:3 as on a 4:3 TV
bool Widescreen();

std::filesystem::path ExeDir();
// the game in assets (game_files.cpp): its folder and this executable's xbe, from the extracted files or a disc
// image, which it mounts as D:; false after telling the player what is missing
struct GameFiles {
    std::filesystem::path dir;
    std::vector<uint8_t> xbe;
};
bool FindGame(GameFiles& game);
int Instance();  // --instance N on the command line, 0 without: a second copy on this PC for system link tests
void* Window();  // HWND
// boot splash: SplinterCell4Logo.bmp or .png beside the exe, up before the game window
bool OpenSplash();        // window thread, at launch; false without one, and the game window opens at once
void SplashFirstFrame();  // the game's first Swap: waits out the splash's minimum, then opens the game window
void CloseSplash();       // window thread
void OpenGameWindow(bool apply = true);  // any thread but the window thread's; returns once the window is up
// relaunched: the previous instance keeps its window up until this one has shown its first frames (handover.cpp)
bool StartHandover();   // window thread, at launch; true: the game window starts hidden
bool HandingOver();
void FinishHandover();  // any thread but the window thread's: opens the game window, lets the old instance go
// the game menus' cursor (HCURSOR): cursor.png or cursor.dds beside the exe sized to the frame, else the system arrow
void* MenuCursor();

// options menu, drawn by the renderer each frame with the lock held
std::recursive_mutex& UiLock();
bool MenuOpen();
bool Capturing();  // a key binding waits for a key
void ToggleMenu();
void DrawUi();

// frames shown a second, from the renderer
inline std::atomic<float> g_fps{0};
// the frame rate in the game's font, from the image's Magma frame hook
void DrawFps(X86Context& ctx);
// "XBE Recomp V1" bottom right, from the game mode page draw
void DrawWatermark(X86Context& ctx);
// the hide HUD key: the HUD layer's Magma pages are not drawn in a level
inline std::atomic<bool> g_hide_hud{false};

// option rows shared by the F10 panel and the game's options menu; page 0 recomp settings, 1 mouse and keyboard
namespace rows {
enum class Kind { Heading, Value, Bind, Text };
struct Row {
    Kind kind;
    int id;  // a bind row's input action
    const char* label;
};
const Row* Page(int page, size_t* count);
std::string Value(const Row& row);
void Step(const Row& row, int step);
void Defaults(int page);
// a value row's list, empty when it only steps; current is the one it has
std::vector<std::string> Choices(const Row& row, int* current);
void Choose(const Row& row, int index);
// false for a row the game menu leaves out: the shader model 3.0 features at 1.1, the microphone and push to talk (F10 only)
bool Shown(const Row& row);
}  // namespace rows

// the console on the backtick key, drawn by DrawUi
bool ConsoleOpen();
void ToggleConsole();
void DrawConsole();

// sleeps out the rest of the frame for the fps cap
void Pace();

}  // namespace app
