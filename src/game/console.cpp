#include "game/console.h"

#include <algorithm>
#include <mutex>
#include <vector>

#include "xdk/hle.h"

using namespace kernel;

namespace {

// UGameEngine::Exec(const TCHAR* cmd, FOutputDevice& ar), thiscall on its FExec base at engine +0x2C
constexpr uint32_t kEngineExec = 0x1CEB87, kEngineFExec = 0x2C;
// UPlayer::Exec, slot 0 of the viewport's FExec base at +0x30
constexpr uint32_t kViewportFExec = 0x30;
// an output device whose Serialize (sub_000EE9D1, ret 8) does nothing; commands get it and their output is
// read in that Serialize
constexpr uint32_t kNullOutput = 0x5B57F0;
constexpr size_t kMaxChars = 200;

struct Command {
    std::string text;
    game::Route route;
};

std::mutex g_lock;
std::vector<Command> g_queue;
std::vector<std::string> g_output;  // under g_lock
bool g_running = false;              // game thread

void Print(std::string line) {
    std::lock_guard lock(g_lock);
    if (g_output.size() < 1000) g_output.push_back(std::move(line));
}

}  // namespace

// FOutputDevice::Serialize(const TCHAR* text, EName event), thiscall; the same ret 8 body serves other classes,
// so only the null device during a command counts
X86_EXTERN_FUNC(__imp__sub_000EE9D1);
X86_FUNC(sub_000EE9D1) {
    if (g_running && ctx.ecx == R32(kNullOutput)) {
        std::string line;
        for (uint32_t p = R32(ctx.esp + 4), n = 0; p && n < 512; p += 2, n++) {
            uint16_t c = R16(p);
            if (!c) break;
            line += c < 0x80 ? char(c) : '?';
        }
        Print(std::move(line));
    }
    __imp__sub_000EE9D1(ctx, base);
}

namespace game {

void QueueCommand(std::string command, Route route) {
    std::lock_guard lock(g_lock);
    g_queue.push_back({std::move(command), route});
}

std::vector<std::string> TakeOutput() {
    std::lock_guard lock(g_lock);
    std::vector<std::string> out;
    out.swap(g_output);
    return out;
}

void RunCommands(X86Context& ctx, uint32_t engine, uint32_t viewport) {
    std::vector<Command> commands;
    {
        std::lock_guard lock(g_lock);
        commands.swap(g_queue);
    }
    uint32_t esp = ctx.esp;
    for (const Command& command : commands) {
        // the wide string goes below the stack pointer, the call below it
        uint32_t text = (esp - 0x40 - uint32_t(kMaxChars + 1) * 2) & ~0xFu;
        size_t n = std::min(command.text.size(), kMaxChars);
        for (size_t i = 0; i < n; i++) W16(text + uint32_t(i) * 2, uint8_t(command.text[i]));
        W16(text + uint32_t(n) * 2, 0);
        LOGI("console: %s", command.text.c_str());
        ctx.esp = text - 0x10;
        uint32_t fn = kEngineExec;
        ctx.ecx = engine + kEngineFExec;
        if (command.route == Route::Player) {
            ctx.ecx = viewport + kViewportFExec;
            fn = R32(R32(ctx.ecx));
        }
        g_running = true;
        uint32_t handled = CallGuest(ctx, fn, {text, R32(kNullOutput)});
        g_running = false;
        ctx.esp = esp;
        if (!handled) Print("unrecognized command: " + command.text);
    }
}

}  // namespace game
