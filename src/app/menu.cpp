// the options panel on F10
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <imgui.h>

#include "app/app.h"
#include "core/input.h"
#include "game/console.h"

namespace app {

namespace {

std::atomic<bool> g_open{false};

constexpr ImVec4 kPanelBg{0.05f, 0.03f, 0.03f, 0.94f};
constexpr ImVec4 kBorder{0.88f, 0.84f, 0.76f, 0.80f};
constexpr ImVec4 kText{0.93f, 0.91f, 0.87f, 1.0f};
constexpr ImVec4 kHighlight{0.55f, 0.12f, 0.12f, 1.0f};
constexpr ImVec4 kHeading{0.82f, 0.28f, 0.22f, 1.0f};
constexpr ImVec4 kDim{0.62f, 0.59f, 0.55f, 1.0f};

constexpr uint32_t kAnisoValues[] = {0, 2, 4, 8, 16};
const char* const kAnisoNames[] = {"GAME DEFAULT", "2x", "4x", "8x", "16x"};
constexpr uint32_t kShadowSizes[] = {0, 512, 1024, 2048, 4096, 8192};
constexpr uint32_t kFpsCaps[] = {0, 30, 60, 75, 90, 120, 144, 165, 240, 360};
// co-op steps last whole ms, so each rate is the top of its step: 1000/rate rounds down to the same period
constexpr uint32_t kTickRates[] = {0, 30, 50, 60, 100, 111, 125, 142};
constexpr uint32_t kDelayFloors[] = {0, 10, 20, 30, 40, 50};

int StepIndex(int index, int step, int count) {
    if (count <= 0) return 0;
    index = (index + step) % count;
    return index < 0 ? index + count : index;
}

int NearestIndex(const uint32_t* values, int count, uint32_t value) {
    int best = 0;
    uint32_t best_d = UINT32_MAX;
    for (int i = 0; i < count; i++) {
        uint32_t d = values[i] > value ? values[i] - value : value - values[i];
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

template <size_t N>
int StepTable(const uint32_t (&values)[N], uint32_t& value, int step) {
    int i = NearestIndex(values, int(N), value);
    if (step) {
        i = StepIndex(i, step, int(N));
        value = values[i];
    }
    return i;
}

std::vector<Mode> Modes() {
    std::vector<Mode> modes;
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++) {
        if (dm.dmBitsPerPel < 32 || dm.dmPelsWidth < 640) continue;
        Mode m{dm.dmPelsWidth, dm.dmPelsHeight};
        if (std::none_of(modes.begin(), modes.end(), [&](const Mode& o) { return o.width == m.width && o.height == m.height; }))
            modes.push_back(m);
    }
    std::sort(modes.begin(), modes.end(), [](const Mode& a, const Mode& b) { return a.width != b.width ? a.width > b.width : a.height > b.height; });
    return modes;
}

void Section(const char* title) {
    float em = ImGui::GetFontSize();
    ImGui::Dummy(ImVec2(0, em * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, kHeading);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, em * 0.2f));
}

// label left, arrows and value right, the row is the only nav stop
int OptionRow(const char* label, const char* value) {
    const ImGuiStyle& style = ImGui::GetStyle();
    float em = ImGui::GetFontSize(), row_w = ImGui::GetContentRegionAvail().x, row_h = ImGui::GetFrameHeight();
    float value_w = em * 12, block_w = row_h * 2 + value_w + style.ItemSpacing.x * 2;
    float row_x = ImGui::GetCursorPosX(), row_y = ImGui::GetCursorPosY();
    ImGui::PushID(label);
    int step = 0;
    ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(row_w, row_h));
    if (ImGui::IsItemFocused()) {
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_Header));
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft)) step = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) step = 1;
    }
    ImGui::SetCursorPos(ImVec2(row_x + style.ItemInnerSpacing.x, row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    float block_x = row_x + row_w - block_w;
    ImGui::SetCursorPos(ImVec2(block_x, row_y));
    if (ImGui::ArrowButton("##dec", ImGuiDir_Left)) step = -1;
    float value_x = block_x + row_h + style.ItemSpacing.x;
    float pad = (value_w - ImGui::CalcTextSize(value).x) * 0.5f;
    ImGui::SetCursorPos(ImVec2(value_x + std::max(pad, 0.0f), row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(value);
    ImGui::SetCursorPos(ImVec2(value_x + value_w + style.ItemSpacing.x, row_y));
    if (ImGui::ArrowButton("##inc", ImGuiDir_Right)) step = 1;
    ImGui::PopItemFlag();
    ImGui::PopID();
    ImGui::SetCursorPos(ImVec2(row_x, row_y));
    ImGui::Dummy(ImVec2(row_w, row_h));
    return step;
}

// label left, a text field where the value sits; true when an edit is done
bool TextRow(const char* label, char* buf, size_t size) {
    const ImGuiStyle& style = ImGui::GetStyle();
    float em = ImGui::GetFontSize(), row_w = ImGui::GetContentRegionAvail().x, row_h = ImGui::GetFrameHeight();
    float value_w = em * 12, block_w = row_h * 2 + value_w + style.ItemSpacing.x * 2;
    float row_x = ImGui::GetCursorPosX(), row_y = ImGui::GetCursorPosY();
    ImGui::PushID(label);
    ImGui::SetCursorPos(ImVec2(row_x + style.ItemInnerSpacing.x, row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SetCursorPos(ImVec2(row_x + row_w - block_w, row_y));
    ImGui::SetNextItemWidth(block_w);
    ImGui::InputText("##text", buf, size);
    bool done = ImGui::IsItemDeactivatedAfterEdit();
    ImGui::PopID();
    ImGui::SetCursorPos(ImVec2(row_x, row_y));
    ImGui::Dummy(ImVec2(row_w, row_h));
    return done;
}

const char* OnOff(bool on) { return on ? "ON" : "OFF"; }

void Changed(bool display) {
    if (display) ApplyDisplay();
    Save();
}

void DrawRows() {
    Settings& s = Get();
    char text[64];
    Section("DISPLAY");
    {
        static const char* const kModes[] = {"FULLSCREEN", "BORDERLESS", "WINDOWED"};
        int i = int(s.display_mode);
        if (int step = OptionRow("DISPLAY MODE", kModes[i])) {
            s.display_mode = DisplayMode(StepIndex(i, step, 3));
            Changed(true);
        }
    }
    {
        static std::vector<Mode> modes = Modes();
        int i = 0;
        for (size_t k = 0; k < modes.size() && !i; k++)
            if (modes[k].width == s.width && modes[k].height == s.height) i = int(k) + 1;
        Mode m = i ? modes[size_t(i - 1)] : Desktop();
        std::snprintf(text, sizeof(text), i ? "%ux%u" : "DESKTOP (%ux%u)", m.width, m.height);
        if (int step = OptionRow("RESOLUTION", text)) {
            i = StepIndex(i, step, int(modes.size()) + 1);
            s.width = i ? modes[size_t(i - 1)].width : 0;
            s.height = i ? modes[size_t(i - 1)].height : 0;
            Changed(true);
        }
    }
    {
        Mode out = Output();
        std::snprintf(text, sizeof(text), "%u%% (%ux%u)", s.render_percent, out.width * s.render_percent / 100, out.height * s.render_percent / 100);
        if (int step = OptionRow("RENDER RESOLUTION", text)) {
            s.render_percent = uint32_t(std::clamp(int(s.render_percent) + step * 5, 20, 200));
            Changed(false);
        }
    }
    {
        uint32_t cap = kFpsCaps[NearestIndex(kFpsCaps, int(std::size(kFpsCaps)), s.fps_cap)];
        if (cap) std::snprintf(text, sizeof(text), "%u", cap);
        if (int step = OptionRow("MAX FRAME RATE", cap ? text : "UNLIMITED")) {
            StepTable(kFpsCaps, s.fps_cap, step);
            Changed(false);
        }
    }
    if (OptionRow("VSYNC", OnOff(s.vsync))) {
        s.vsync = !s.vsync;
        Changed(false);
    }
    if (OptionRow("FPS COUNTER", OnOff(s.fps_counter))) {
        s.fps_counter = !s.fps_counter;
        Changed(false);
    }
    Section("GRAPHICS");
    if (int step = OptionRow("ANISOTROPIC FILTERING", kAnisoNames[NearestIndex(kAnisoValues, 5, s.aniso)])) {
        StepTable(kAnisoValues, s.aniso, step);
        Changed(false);
    }
    {
        uint32_t size = kShadowSizes[NearestIndex(kShadowSizes, int(std::size(kShadowSizes)), s.shadow_resolution)];
        std::snprintf(text, sizeof(text), size == 512 ? "%u (CONSOLE)" : "%u", size);
        if (int step = OptionRow("SHADOW RESOLUTION", size ? text : "AUTO")) {
            StepTable(kShadowSizes, s.shadow_resolution, step);
            Changed(false);
        }
    }
    Section("GAME");
    if (OptionRow("SKIP INTROS", OnOff(s.skip_intros))) {
        s.skip_intros = !s.skip_intros;
        Changed(false);
    }
    Section("CO-OP");
    {
        uint32_t rate = kTickRates[NearestIndex(kTickRates, int(std::size(kTickRates)), s.coop_tick_rate)];
        if (rate) std::snprintf(text, sizeof(text), "%u HZ", rate);
        if (int step = OptionRow("TICK RATE", rate ? text : "GAME (22 HZ)")) {
            StepTable(kTickRates, s.coop_tick_rate, step);
            Changed(false);
        }
    }
    {
        uint32_t floor = kDelayFloors[NearestIndex(kDelayFloors, int(std::size(kDelayFloors)), s.coop_input_delay_floor)];
        std::snprintf(text, sizeof(text), floor == 50 ? "%u MS (GAME)" : "%u MS", floor);
        if (int step = OptionRow("MIN INPUT DELAY", text)) {
            StepTable(kDelayFloors, s.coop_input_delay_floor, step);
            Changed(false);
        }
    }
    {
        static char join[64] = {};
        static bool loaded = false;
        if (!loaded) std::snprintf(join, sizeof(join), "%s", s.join_ip.c_str());
        loaded = true;
        if (TextRow("JOIN IP", join, sizeof(join))) {
            s.join_ip = join;
            Changed(false);
        }
    }
}

// the row being rebound, -1 when idle; its keys must be released before the next press binds
std::atomic<int> g_capture{-1};
bool g_capture_armed = false;
int g_capture_done = 0;  // frame of the last bind, the key that bound must not start another
int g_page = 0;
const char* const kPages[] = {"SETTINGS", "MOUSE AND KEYBOARD", "LEVEL SELECT"};
constexpr int kPageCount = int(std::size(kPages));
constexpr int kControlsPage = 1, kLevelsPage = 2;

// the DataXb\LMaps folders; Menu is the main menu
const char* const kSoloLevels[] = {
    "x01_Iceland_a",    "x01_iceland_b",    "x02_prison_a",   "x02_prison_b",   "x04_moneytrain_a", "x04_moneytrain_b",
    "x05_hq2_r",        "x06_cozumel_a",    "x06_cozumel_b",  "x08_okhotsk_a",  "x08_okhotsk_b",    "x10_kinshasa_a",
    "x10_kinshasa_b",   "x11_hq5_r",        "x12_ny_a",       "x12_ny_b",       "x12_ny_c",
};
const char* const kCoopLevels[] = {
    "x01_coop_ice_a",  "x01_coop_ice_b",  "x01_coop_ice_c",  "x01_coop_ice_d",  "x01_coop_ice_e",
    "x02_coop_pris_a", "x02_coop_pris_b", "x02_coop_pris_c", "x02_coop_pris_d", "x02_coop_pris_e",
    "x03_coop_kin_a",  "x03_coop_kin_b",  "x03_coop_kin_c",  "x03_coop_kin_f",
};

// label left, keys right; Enter or a click starts the capture
bool BindRow(const char* label, const std::string& keys, bool capturing) {
    const ImGuiStyle& style = ImGui::GetStyle();
    float em = ImGui::GetFontSize(), row_w = ImGui::GetContentRegionAvail().x, row_h = ImGui::GetFrameHeight();
    float value_w = em * 12, block_w = row_h * 2 + value_w + style.ItemSpacing.x * 2;
    float row_x = ImGui::GetCursorPosX(), row_y = ImGui::GetCursorPosY();
    ImGui::PushID(label);
    bool pressed = ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(row_w, row_h));
    if (ImGui::IsItemFocused() || capturing)
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_Header));
    ImGui::SetCursorPos(ImVec2(row_x + style.ItemInnerSpacing.x, row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    std::string value = capturing ? std::string("PRESS A KEY") : keys.empty() ? std::string("UNBOUND") : keys;
    for (char& c : value) c = c == ',' ? '/' : char(std::toupper(uint8_t(c)));
    float value_x = row_x + row_w - block_w + row_h + style.ItemSpacing.x;
    ImGui::SetCursorPos(ImVec2(value_x + std::max((value_w - ImGui::CalcTextSize(value.c_str()).x) * 0.5f, 0.0f), row_y));
    ImGui::AlignTextToFramePadding();
    if (capturing) ImGui::PushStyleColor(ImGuiCol_Text, kHeading);
    ImGui::TextUnformatted(value.c_str());
    if (capturing) ImGui::PopStyleColor();
    ImGui::PopID();
    ImGui::SetCursorPos(ImVec2(row_x, row_y));
    ImGui::Dummy(ImVec2(row_w, row_h));
    return pressed;
}

// Escape cancels, any other bindable key or mouse button binds
void PollCapture() {
    if (g_capture < 0) return;
    int vk = input::PressedKey();
    if (!g_capture_armed) {
        g_capture_armed = !vk;
        return;
    }
    if (!vk) return;
    if (vk != VK_ESCAPE) input::Bind(size_t(g_capture.load()), vk);
    g_capture = -1;
    g_capture_done = ImGui::GetFrameCount();
}

void DrawControls() {
    Settings& s = Get();
    char text[64];
    Section("MOUSE");
    std::snprintf(text, sizeof(text), "%.1f", s.mouse_sensitivity);
    if (int step = OptionRow("MOUSE SENSITIVITY", text)) {
        s.mouse_sensitivity = std::clamp(std::round((s.mouse_sensitivity + step * 0.1f) * 10.0f) / 10.0f, 0.1f, 5.0f);
        Changed(false);
    }
    if (OptionRow("INVERT MOUSE", OnOff(s.invert_mouse))) {
        s.invert_mouse = !s.invert_mouse;
        Changed(false);
    }
    Section("KEYBOARD");
    size_t count = 0;
    const input::Action* actions = input::Actions(&count);
    for (size_t i = 0; i < count; i++)
        if (BindRow(actions[i].label, input::Binding(i), g_capture == int(i)) && g_capture < 0 &&
            ImGui::GetFrameCount() > g_capture_done + 1) {
            g_capture = int(i);
            g_capture_armed = false;
        }
}

void LevelList(const char* title, const char* const* levels, size_t count) {
    Section(title);
    for (size_t i = 0; i < count; i++)
        if (ImGui::Selectable(levels[i], false, 0, ImVec2(0.0f, ImGui::GetFrameHeight()))) {
            game::QueueCommand(std::string("open ") + levels[i]);
            ToggleMenu();
        }
}

void DrawLevels() {
    LevelList("SOLO", kSoloLevels, std::size(kSoloLevels));
    LevelList("CO-OP", kCoopLevels, std::size(kCoopLevels));
}

void Defaults() {
    g_capture = -1;
    if (g_page == kLevelsPage) return;
    if (g_page == kControlsPage) {
        Settings& s = Get();
        s.mouse_sensitivity = Settings{}.mouse_sensitivity;
        s.invert_mouse = false;
        input::ResetBindings();
        return;
    }
    Settings keep = Get();
    Get() = Settings{};
    Get().mouse_sensitivity = keep.mouse_sensitivity;
    Get().invert_mouse = keep.invert_mouse;
    Get().keys = keep.keys;
    Changed(true);
}

void DrawPanel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float em = ImGui::GetFontSize();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(em * 50, em * 32), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kPanelBg);
    ImGui::PushStyleColor(ImGuiCol_Border, kBorder);
    ImGui::PushStyleColor(ImGuiCol_Text, kText);
    ImGui::PushStyleColor(ImGuiCol_Header, kHighlight);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kHighlight);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, kHighlight);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 1.6f, em * 1.2f));
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("##settings", nullptr, kFlags)) {
        const char* title = "SETTINGS";
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(title).x) * 0.5f, 0.0f));
        ImGui::TextUnformatted(title);
        ImGui::Separator();
        if (int step = OptionRow("PAGE", kPages[g_page])) {
            g_page = StepIndex(g_page, step, kPageCount);
            g_capture = -1;
        }
        float footer = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() + em;
        ImGui::BeginChild("##rows", ImVec2(0, -footer), ImGuiChildFlags_NavFlattened);
        if (g_page == kLevelsPage) DrawLevels();
        else if (g_page == kControlsPage) DrawControls();
        else DrawRows();
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, kDim);
        ImGui::TextUnformatted(g_capture >= 0            ? "Press a key or mouse button to bind, Escape cancels"
                               : g_page == kLevelsPage ? "Up and Down select, Enter or a click loads the level, F10 or Escape closes"
                                                       : "Up and Down select, Left and Right change, Enter rebinds, F10 or Escape closes");
        ImGui::PopStyleColor();
        float bw = em * 8;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(ImGui::GetContentRegionAvail().x - bw * 2 - ImGui::GetStyle().ItemSpacing.x, 0.0f));
        if (ImGui::Button("Back", ImVec2(bw, 0))) ToggleMenu();
        ImGui::SameLine();
        if (ImGui::Button("Defaults", ImVec2(bw, 0))) Defaults();
    }
    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(6);
}

void DrawFps(double fps) {
    char text[32];
    std::snprintf(text, sizeof(text), "FPS: %.1f", fps);
    float em = ImGui::GetFontSize();
    ImDrawList* list = ImGui::GetForegroundDrawList();
    ImVec2 at(em * 0.8f, em * 0.5f);
    list->AddText(ImVec2(at.x + 1, at.y + 1), IM_COL32(0, 0, 0, 200), text);
    list->AddText(at, IM_COL32(180, 255, 120, 255), text);
}

}  // namespace

bool MenuOpen() { return g_open.load(std::memory_order_relaxed); }

bool Capturing() { return g_capture >= 0; }

void ToggleMenu() {
    bool open = !g_open.load(std::memory_order_relaxed);
    g_open.store(open, std::memory_order_relaxed);
    g_capture = -1;
    input::SetBlocked(open || ConsoleOpen());
}

void DrawUi(double fps) {
    ImGuiIO& io = ImGui::GetIO();
    if (MenuOpen() || ConsoleOpen()) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    // scale the font with the output height
    io.FontGlobalScale = std::max(1.0f, io.DisplaySize.y / 720.0f);
    if (Get().fps_counter) DrawFps(fps);
    if (ConsoleOpen()) DrawConsole();
    if (MenuOpen()) {
        PollCapture();
        DrawPanel();
    }
}

}  // namespace app
