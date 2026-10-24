#pragma once
#include <cstdint>
#include <string>
#include <string_view>
struct X86Context;
namespace game::trust {
void Tick(X86Context& ctx, uint32_t viewport);
void Page(uint32_t id);
bool Element(uint32_t id);
void EndFrame();
bool Command(std::string_view command, std::string& output);
void Localized(uint32_t section, uint32_t key);
}
