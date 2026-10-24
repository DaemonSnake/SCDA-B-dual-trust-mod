#pragma once
// Optional host integration points. Only the solo image installs these callbacks.
#include <filesystem>
#include <cstdint>

namespace mod_events {
inline void (*draw_ui)() = nullptr;
inline void (*file_closed)(const std::filesystem::path&, bool written, uint64_t read_bytes) = nullptr;
inline void (*relaunch)() = nullptr;
}
