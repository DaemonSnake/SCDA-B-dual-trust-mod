// scda.toml beside the exe
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

#include <toml++/toml.hpp>

#include "app/app.h"
#include "audio/output.h"
#include "core/log.h"

namespace app {

namespace {

const char* kModes[] = {"fullscreen", "windowed", "borderless"};

// "keyboard", "controllerN", both joined by '+', or "none"
std::string PortName(const Port& p) {
    std::string name = p.keyboard ? "keyboard" : "";
    if (p.pad >= 0) name += (name.empty() ? "controller" : "+controller") + std::to_string(p.pad + 1);
    return name.empty() ? "none" : name;
}

Port ParsePort(const std::string& name, Port fallback) {
    Port p;
    p.keyboard = name.find("keyboard") != std::string::npos;
    if (size_t at = name.find("controller"); at != std::string::npos && at + 10 < name.size()) {
        int n = name[at + 10] - '0';  // the digit after "controller"
        if (n >= 1 && n <= 4) p.pad = n - 1;
    }
    return p.keyboard || p.pad >= 0 || name == "none" ? p : fallback;
}

std::filesystem::path File() { return ExeDir() / "scda.toml"; }

}  // namespace

Settings& Get() {
    static Settings s;
    return s;
}

void Load() {
    Settings& s = Get();
    toml::table t;
    try {
        t = toml::parse_file(File().string());
    } catch (...) {
        return;
    }
    s.monitor = t["monitor"].value_or(s.monitor);
    std::string mode = t["display_mode"].value_or(std::string(kModes[uint32_t(s.display_mode)]));
    for (uint32_t i = 0; i < 3; i++)
        if (mode == kModes[i]) s.display_mode = DisplayMode(i);
    s.width = t["width"].value_or(s.width);
    s.height = t["height"].value_or(s.height);
    s.render_percent = t["render_percent"].value_or(s.render_percent);
    s.fps_cap = t["fps_cap"].value_or(s.fps_cap);
    s.fov = std::clamp(t["fov"].value_or(s.fov), 45u, 140u);
    s.fps_counter = t["fps_counter"].value_or(s.fps_counter);
    s.vsync = t["vsync"].value_or(s.vsync);
    s.aniso = t["anisotropic_filtering"].value_or(s.aniso);
    s.msaa = t["msaa"].value_or(s.msaa);
    s.smaa = t["smaa"].value_or(s.smaa);
    s.shadow_resolution = std::min(t["shadow_resolution"].value_or(s.shadow_resolution), 8192u);
    // 0 was the old UNLIMITED, now the furthest
    uint32_t shadow_distance = t["shadow_distance"].value_or(s.shadow_distance);
    s.shadow_distance = shadow_distance ? std::min(shadow_distance, 8u) : 8u;
    s.shader_model = t["shader_model"].value_or(s.shader_model) >= 3 ? 3u : 1u;
    s.hdr_rendering = t["hdr_rendering"].value_or(s.hdr_rendering);
    s.tone_mapping = t["tone_mapping"].value_or(s.tone_mapping);
    // true was the old ON, now SSAO
    if (auto on = t["ambient_occlusion"].value<bool>()) s.ambient_occlusion = *on;
    s.ambient_occlusion = std::min(t["ambient_occlusion"].value_or(s.ambient_occlusion), 2u);
    s.async_shaders = t["async_shaders"].value_or(s.async_shaders);
    s.mouse_sensitivity = float(t["mouse_sensitivity"].value_or(double(s.mouse_sensitivity)));
    s.invert_mouse = t["invert_mouse"].value_or(s.invert_mouse);
    s.skip_intros = t["skip_intros"].value_or(s.skip_intros);
    std::string language = t["language"].value_or(s.language);
    if (std::find(std::begin(kLanguages), std::end(kLanguages), language) != std::end(kLanguages)) s.language = language;
    s.audio_output = std::min(t["audio_output"].value_or(s.audio_output), audio::kOutputMono);
    audio::SetOutputMode(s.audio_output);
    s.coop_tick_rate = t["coop_tick_rate"].value_or(s.coop_tick_rate);
    s.coop_input_delay_floor = t["coop_input_delay_floor_ms"].value_or(s.coop_input_delay_floor);
    s.join_ip = t["join_ip"].value_or(s.join_ip);
    s.microphone = t["microphone"].value_or(s.microphone);
    s.push_to_talk = t["push_to_talk"].value_or(s.push_to_talk);
    Port p1 = ParsePort(t["player1"].value_or(PortName(s.players[0])), s.players[0]);
    Port p2 = ParsePort(t["player2"].value_or(PortName(s.players[1])), s.players[1]);
    // a device sits on one port only
    if (p1.keyboard) p2.keyboard = false;
    if (p1.pad == p2.pad) p2.pad = -1;
    // player 1 drives single player and versus alone
    if (!p1.keyboard && p1.pad < 0) std::swap(p1, p2);
    s.players[0] = p1;
    s.players[1] = p2;
    if (const toml::table* keys = t["keys"].as_table())
        for (auto&& [id, value] : *keys)
            if (auto v = value.value<std::string>()) s.keys[std::string(id.str())] = *v;
    if (const toml::table* pads = t["controller"].as_table())
        for (auto&& [id, value] : *pads)
            if (auto v = value.value<std::string>()) s.pad_buttons[std::string(id.str())] = *v;
}

namespace {

toml::table Table() {
    const Settings& s = Get();
    toml::table t{{"monitor", s.monitor},
                  {"display_mode", kModes[uint32_t(s.display_mode)]},
                  {"width", int64_t(s.width)},
                  {"height", int64_t(s.height)},
                  {"render_percent", int64_t(s.render_percent)},
                  {"fps_cap", int64_t(s.fps_cap)},
                  {"fov", int64_t(s.fov)},
                  {"fps_counter", s.fps_counter},
                  {"vsync", s.vsync},
                  {"anisotropic_filtering", int64_t(s.aniso)},
                  {"msaa", int64_t(s.msaa)},
                  {"smaa", s.smaa},
                  {"shadow_resolution", int64_t(s.shadow_resolution)},
                  {"shadow_distance", int64_t(s.shadow_distance)},
                  {"shader_model", int64_t(s.shader_model)},
                  {"hdr_rendering", s.hdr_rendering},
                  {"tone_mapping", s.tone_mapping},
                  {"ambient_occlusion", int64_t(s.ambient_occlusion)},
                  {"async_shaders", s.async_shaders},
                  {"mouse_sensitivity", double(s.mouse_sensitivity)},
                  {"invert_mouse", s.invert_mouse},
                  {"skip_intros", s.skip_intros},
                  {"language", s.language},
                  {"audio_output", int64_t(s.audio_output)},
                  {"coop_tick_rate", int64_t(s.coop_tick_rate)},
                  {"coop_input_delay_floor_ms", int64_t(s.coop_input_delay_floor)},
                  {"join_ip", s.join_ip},
                  {"microphone", s.microphone},
                  {"push_to_talk", s.push_to_talk},
                  {"player1", PortName(s.players[0])},
                  {"player2", PortName(s.players[1])}};
    toml::table keys;
    for (const auto& [id, value] : s.keys) keys.insert(id, value);
    t.insert("keys", keys);
    toml::table pads;
    for (const auto& [id, value] : s.pad_buttons) pads.insert(id, value);
    t.insert("controller", pads);
    return t;
}

}  // namespace

void Save() {
    std::ofstream f(File());
    f << Table() << "\n";
}

void LogSettings() {
    std::ostringstream text;
    text << Table();
    std::istringstream lines(text.str());
    LOGR("Settings:");
    for (std::string line; std::getline(lines, line);)
        if (!line.empty()) LOGR("  %s", line.c_str());
}

const std::string& BootLanguage() {
    static const std::string language = Get().language;
    return language;
}

Mode Desktop() {
    HMONITOR mon = MonitorFromWindow(static_cast<HWND>(Window()), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (GetMonitorInfoW(mon, &mi) && EnumDisplaySettingsW(mi.szDevice, ENUM_REGISTRY_SETTINGS, &dm))
        return {dm.dmPelsWidth, dm.dmPelsHeight};
    return {uint32_t(GetSystemMetrics(SM_CXSCREEN)), uint32_t(GetSystemMetrics(SM_CYSCREEN))};
}

std::vector<Display> Displays() {
    std::vector<Display> out;
    auto add = [](HMONITOR mon, HDC, LPRECT, LPARAM list) -> BOOL {
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof(mi);
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (!GetMonitorInfoW(mon, &mi) || !EnumDisplaySettingsW(mi.szDevice, ENUM_REGISTRY_SETTINGS, &dm)) return TRUE;
        Display d{std::string(), 0, {dm.dmPelsWidth, dm.dmPelsHeight}, (mi.dwFlags & MONITORINFOF_PRIMARY) != 0};
        for (const wchar_t* c = mi.szDevice; *c; c++) d.device += char(*c);
        size_t digits = d.device.find_last_not_of("0123456789") + 1;
        d.number = digits < d.device.size() ? std::atoi(d.device.c_str() + digits) : 0;
        reinterpret_cast<std::vector<Display>*>(list)->push_back(d);
        return TRUE;
    };
    EnumDisplayMonitors(nullptr, nullptr, add, LPARAM(&out));
    std::sort(out.begin(), out.end(), [](const Display& a, const Display& b) { return a.number < b.number; });
    return out;
}

void* MonitorHandle(const std::string& device) {
    struct Find {
        std::wstring device;
        HMONITOR found;
    } find{std::wstring(device.begin(), device.end()), nullptr};
    auto match = [](HMONITOR mon, HDC, LPRECT, LPARAM p) -> BOOL {
        auto* f = reinterpret_cast<Find*>(p);
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(mon, &mi) && f->device == mi.szDevice) f->found = mon;
        return !f->found;
    };
    if (!device.empty()) EnumDisplayMonitors(nullptr, nullptr, match, LPARAM(&find));
    return find.found ? find.found : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

Mode Output() {
    const Settings& s = Get();
    if (s.width && s.height) return {s.width, s.height};
    return Desktop();
}

float Aspect() {
    // asked per frame and per HUD quad: the desktop mode lookup only runs when the resolution setting changes
    static std::atomic<uint64_t> key{~0ull};
    static std::atomic<float> aspect{16.0f / 9.0f};
    const Settings& s = Get();
    uint64_t k = (uint64_t(s.width) << 32 | s.height) ^ uint64_t(g_display_applied) << 48;
    if (key.exchange(k) != k) {
        Mode m = Output();
        aspect = std::max(float(m.width) / std::max(m.height, 1u), 4.0f / 3.0f);
    }
    return aspect;
}

float FovScale() {
    constexpr float kHalf = 3.14159265f / 360.0f;
    return std::tan(Get().fov * kHalf) / std::tan(75 * kHalf);
}

float RenderScale(uint32_t guest_height) {
    float h = Output().height * (Get().render_percent / 100.0f);
    return guest_height ? std::max(h / guest_height, 0.1f) : 1.0f;
}

}  // namespace app
