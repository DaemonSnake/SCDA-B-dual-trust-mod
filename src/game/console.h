#pragma once
#include <string>

#include "cpu/x86_context.h"

// console commands from the host, run through the engine on the game thread
namespace game {

void QueueCommand(std::string command);  // any thread
// from the input update; a command such as open may load a map, so nothing may use the level after this
void RunCommands(X86Context& ctx, uint32_t engine);

}  // namespace game
