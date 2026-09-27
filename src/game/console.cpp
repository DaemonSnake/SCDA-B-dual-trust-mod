#include "game/console.h"

#include <algorithm>
#include <mutex>
#include <vector>

#include "xdk/hle.h"

using namespace kernel;

namespace {

// UGameEngine::Exec(const TCHAR* cmd, FOutputDevice& ar), thiscall on its FExec base at engine +0x2C
constexpr uint32_t kEngineExec = 0x1CEB87, kEngineFExec = 0x2C;
// an output device whose Serialize does nothing (vtable 0x4FDB68)
constexpr uint32_t kNullOutput = 0x5B57F0;
constexpr size_t kMaxChars = 200;

std::mutex g_lock;
std::vector<std::string> g_queue;

}  // namespace

namespace game {

void QueueCommand(std::string command) {
    std::lock_guard lock(g_lock);
    g_queue.push_back(std::move(command));
}

void RunCommands(X86Context& ctx, uint32_t engine) {
    std::vector<std::string> commands;
    {
        std::lock_guard lock(g_lock);
        commands.swap(g_queue);
    }
    uint32_t esp = ctx.esp;
    for (const std::string& command : commands) {
        // the wide string goes below the stack pointer, the call below it
        uint32_t text = (esp - 0x40 - uint32_t(kMaxChars + 1) * 2) & ~0xFu;
        size_t n = std::min(command.size(), kMaxChars);
        for (size_t i = 0; i < n; i++) W16(text + uint32_t(i) * 2, uint8_t(command[i]));
        W16(text + uint32_t(n) * 2, 0);
        LOGI("console: %s", command.c_str());
        ctx.esp = text - 0x10;
        ctx.ecx = engine + kEngineFExec;
        CallGuest(ctx, kEngineExec, {text, R32(kNullOutput)});
        ctx.esp = esp;
    }
}

}  // namespace game
