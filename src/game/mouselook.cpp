// Mouse look the way the PC viewport feeds it: after UXboxViewport::UpdateInput, the raw counts go to
// UEngine::InputEvent as IK_MouseX and IK_MouseY axis events, which the game's ini binds to aMouseX and aMouseY.
#include "core/input.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// viewport: +0x18 client; client: +0x2C engine, +0x30 viewports; engine vtable +0x78 InputEvent(viewport, key, action, delta)
constexpr uint32_t kMouseX = 0xE4, kMouseY = 0xE5, kAxis = 4;

void Send(X86Context& ctx, uint32_t engine, uint32_t viewport, uint32_t key, float delta) {
    uint32_t d;
    std::memcpy(&d, &delta, 4);
    ctx.ecx = engine;
    CallGuest(ctx, R32(R32(engine) + 0x78), {viewport, key, kAxis, d});
}

// UInput bindings: FString[255] at +0x2B0, wide text
std::string Binding(uint32_t input, uint32_t key) {
    std::string s;
    uint32_t text = R32(input + 0x2B0 + key * 12);
    for (uint32_t i = 0; text && i < 64; i++) {
        uint16_t c = R16(text + i * 2);
        if (!c) break;
        s += char(c);
    }
    return s;
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_0004B525);
X86_FUNC(sub_0004B525) {
    uint32_t viewport = ctx.ecx;
    __imp__sub_0004B525(ctx, base);
    float dx, dy;
    uint32_t client = R32(viewport + 0x18);
    uint32_t engine = client ? R32(client + 0x2C) : 0;
    // the axis command stores its value, so each axis gets one summed event per update
    if (!engine || R32(R32(client + 0x30)) != viewport || !input::TakeMouse(dx, dy)) return;
    LOG_FIRST(1, "mouselook: IK_MouseX is bound to \"%s\"", Binding(R32(viewport + 0x60), kMouseX).c_str());
    if (dx != 0.0f) Send(ctx, engine, viewport, kMouseX, dx);
    if (dy != 0.0f) Send(ctx, engine, viewport, kMouseY, dy);
}
