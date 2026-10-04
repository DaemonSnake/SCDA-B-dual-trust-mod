// Game text replaced by key: Xbox wording for the PC, and System Link called LAN everywhere the game shows it (the
// co-op button, its page titles and the lobby all read [COMMON] MainTitleLan). Every language gets the English text.
// The storage messages never show while the hard disk reports its fixed free space; their "Select OK to free %i
// blocks" tail went to the dashboard, so it is dropped.
// On keyboard and mouse the English tutorial text names the bound keys in place of the pad's buttons.
#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>

#include "core/input.h"
#include "xdk/hle.h"

using namespace kernel;

namespace game {
std::string KeyNames(std::initializer_list<const char*> ids);  // prompts.cpp
}

namespace {

// Localize(section, key, package, ...), cdecl, returns one of its own TCHAR buffers (0x400 characters)
struct Text {
    std::u16string_view key, text;
};
constexpr Text kTexts[] = {
    {u"MainTitleLan", u"LAN"},
    {u"SaveWarning", u"Please don't close the game or turn off your PC."},
    {u"ProfileErrorCreateMsg", u"Your PC doesn't have enough free storage to save a profile."},
    {u"ErrorSavingMsg", u"Your PC doesn't have enough free storage to save the checkpoint."},
    {u"SavedMissingSpaceMsg", u"Your PC doesn't have enough free storage to save the checkpoints."},
};

// applied in order. {action} is its key, @ is press or click by the key after it. A section limits one to it:
// the game's own text has the two visions' directions swapped
struct Swap {
    std::string_view from, to, section;
};
constexpr Swap kSwaps[] = {
    {"press X to make noise or A to release gas", "@ {alt_fire} to make noise or {fire} to release gas"},
    {"press START to begin", "press (ENTER) to begin"},
    {"press A to continue", "press (ENTER) to continue"},
    {"trigger button", "trigger"},
    {"the left trigger", "left trigger"},
    {"the right trigger", "right trigger"},
    {"pull and hold left trigger", "@ and hold {alt_fire}"},
    {"pull left trigger", "@ {alt_fire}"},
    {"pull right trigger", "@ {fire}"},
    {"left trigger", "{alt_fire}"},
    {"right trigger", "{fire}"},
    {"hold the trigger", "hold {alt_fire}"},
    {"the triggers", "{alt_fire} and {fire}"},
    {"clicking on the left thumbstick", "@ing {back_to_wall}"},
    {"clicking the left thumbstick", "@ing {back_to_wall}"},
    {"click on the right thumbstick", "@ {zoom}"},
    {"click the right thumbstick", "@ {zoom}"},
    {"click right thumbstick", "@ {zoom}"},
    {"rotate the left thumbstick", "press {move}"},
    {"move the left thumbstick around its periphery", "press {move}"},
    {"left thumbstick", "{move}"},
    {"press left on the D-pad", "@ {next_vision}", "THERMO"},
    {"press right on the D-pad", "@ {previous_vision}", "NIGHTVISION"},
    {"press on the left D-pad", "@ {previous_vision}"},
    {"press up on the D-pad", "@ {goggles}"},
    {"press up on the directional pad", "@ {goggles}"},
    {"press on the Y button", "@ {jump}"},
    {"the BLACK button to whistle", "{whistle} to whistle"},
    {"the BLACK button", "{sidekick}"},
    {"the coop button", "{sidekick}"},
    {"coop button", "{sidekick}"},
    {"the WHITE button", "{quick_inventory}"},
    {"the START button", "{pause}"},
    {"the BACK button", "{opsat}"},
    {"the A button", "{interact}"},
    {"the action button", "{interact}"},
    {"the jump button", "{jump}"},
    {"the primary or secondary fire buttons", "{fire} or {alt_fire}"},
    {"press A", "press {interact}"},
    {"pressing A", "pressing {interact}"},
    {"hold B", "hold {crouch}"},
    {"press Y", "press {jump}"},
    {"X to equip", "{scope} to equip"},
};

bool Equals(uint32_t text, std::u16string_view want) {
    if (!text) return false;
    for (size_t i = 0; i <= want.size(); i++)
        if (R16(text + uint32_t(i) * 2) != (i < want.size() ? want[i] : 0)) return false;
    return true;
}

bool Letter(char16_t c) { return (c | 0x20) >= 'a' && (c | 0x20) <= 'z'; }

// empty when a key is unbound
std::u16string Expand(std::string_view to, bool upper) {
    std::u16string out;
    size_t verb = std::u16string::npos;
    for (size_t i = 0; i < to.size(); i++) {
        if (to[i] == '@') {
            verb = out.size();
        } else if (to[i] != '{') {
            out += char16_t(to[i]);
        } else {
            size_t end = to.find('}', i);
            std::string id(to.substr(i + 1, end - i - 1));
            std::string key = id == "move" ? game::KeyNames({"move_forward", "strafe_left", "move_back", "strafe_right"})
                                           : game::KeyNames({id.c_str()});
            if (key.empty()) return {};
            if (verb != std::u16string::npos) out.insert(verb, key.ends_with("MB") || key.starts_with("MOUSE") ? u"click" : u"press");
            verb = std::u16string::npos;
            out += u'(' + std::u16string(key.begin(), key.end()) + u')';
            i = end;
        }
    }
    if (upper && out[0] >= 'a' && out[0] <= 'z') out[0] -= 0x20;
    return out;
}

// whole words, the first letter in either case
std::u16string ToKeys(std::u16string text, std::u16string_view section) {
    for (const Swap& s : kSwaps) {
        if (!s.section.empty() && !std::equal(section.begin(), section.end(), s.section.begin(), s.section.end(),
                                              [](char16_t a, char b) { return (a | 0x20) == (b | 0x20); }))
            continue;
        for (size_t at = 0; at + s.from.size() <= text.size(); at++) {
            size_t end = at + s.from.size();
            if ((at && Letter(text[at - 1])) || (end < text.size() && Letter(text[end]))) continue;
            if ((text[at] | 0x20) != (s.from[0] | 0x20) || !std::equal(s.from.begin() + 1, s.from.end(), text.begin() + at + 1))
                continue;
            std::u16string to = Expand(s.to, text[at] < 'a');
            if (to.empty()) continue;
            text.replace(at, s.from.size(), to);
            at += to.size() - 1;
        }
    }
    return text;
}

std::u16string Read(uint32_t text) {
    std::u16string out;
    for (uint32_t i = 0; text && i < 0x3FF && R16(text + i * 2); i++) out += char16_t(R16(text + i * 2));
    return out;
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_00070FC9);
X86_FUNC(sub_00070FC9) {
    uint32_t key = R32(ctx.esp + 8);
    std::u16string section = Read(R32(ctx.esp + 4)), text;
    for (auto& t : kTexts)
        if (Equals(key, t.key)) text = t.text;
    __imp__sub_00070FC9(ctx, base);
    if (!ctx.eax) return;
    if (text.empty() && input::KeyboardActive()) {
        std::u16string was = Read(ctx.eax);
        text = ToKeys(was, section);
        if (text == was) return;
    }
    if (text.empty()) return;
    text.resize(std::min<size_t>(text.size(), 0x3FF));
    for (size_t i = 0; i <= text.size(); i++) W16(ctx.eax + uint32_t(i) * 2, i < text.size() ? text[i] : 0);
}
