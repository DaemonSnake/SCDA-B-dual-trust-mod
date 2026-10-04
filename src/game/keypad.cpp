// Keypads typed on the numpad, as SCCT PC: digits press their button, Backspace, Delete and Numpad * press * (which
// clears the code) and Numpad . presses #. v2's keypad key handler (sub_001661F2) still has SCCT PC's table, but on
// the console it only selects the button and no key ever reaches it, so the button is selected, pushed and lit here,
// as the pad's Interaction press does.
#include "core/input.h"
#include "game/unreal.h"
#include "xdk/hle.h"

using namespace kernel;
namespace unreal = game::unreal;

namespace {

constexpr uint32_t kViewportActor = 0x34;
// EKeyPadInteraction +0x2D4 MyKeyPad; EKeyPad +0x3FC SelectedButton (0-8 the digits 1-9, 9 *, 10 0, 11 #)
constexpr uint32_t kMyKeyPad = 0x2D4, kSelectedButton = 0x3FC;
// thiscall on the keypad, no arguments
constexpr uint32_t kGlowSelected = 0x14AD42, kKeyPushed = 0x14AE47;

}  // namespace

namespace game {

void KeypadKeys(X86Context& ctx, uint32_t viewport, bool menu, bool net) {
    int button = input::TakeKeypadButton();
    uint32_t pc = R32(viewport + kViewportActor);
    // a synced session's lockstep carries the pad, not what the game side does on its own
    if (button < 0 || menu || net || !pc) return;
    uint32_t state = unreal::StateOf(pc);
    if (state != unreal::Name("s_KeyPadInteract") && state != unreal::Name("s_KeyPadInteractHanged")) return;
    uint32_t offset = unreal::Offset(pc, unreal::Name("Interaction"));
    uint32_t interaction = offset ? R32(pc + offset) : 0;
    if (!interaction || !unreal::IsA(interaction, unreal::Name("EKeyPadInteraction"))) return;
    // the handler takes keys only while the keypad is in use (not showing granted / denied)
    uint32_t keypad = R32(interaction + kMyKeyPad);
    if (!keypad || unreal::StateOf(keypad) != unreal::Name("s_Use")) return;
    W32(keypad + kSelectedButton, uint32_t(button));
    ctx.ecx = keypad;
    CallGuest(ctx, kKeyPushed, {});
    ctx.ecx = keypad;
    CallGuest(ctx, kGlowSelected, {});
}

}  // namespace game
