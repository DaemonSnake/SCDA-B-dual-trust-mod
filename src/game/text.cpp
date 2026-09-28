// Game text replaced by key: Xbox wording for the PC, and System Link called LAN everywhere the game shows it (the
// co-op button, its page titles and the lobby all read [COMMON] MainTitleLan). Every language gets the English text.
// The storage messages never show while the hard disk reports its fixed free space; their "Select OK to free %i
// blocks" tail went to the dashboard, so it is dropped.
#include <string_view>

#include "xdk/hle.h"

using namespace kernel;

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

bool Equals(uint32_t text, std::u16string_view want) {
    if (!text) return false;
    for (size_t i = 0; i <= want.size(); i++)
        if (R16(text + uint32_t(i) * 2) != (i < want.size() ? want[i] : 0)) return false;
    return true;
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_00070FC9);
X86_FUNC(sub_00070FC9) {
    uint32_t key = R32(ctx.esp + 8);
    const Text* found = nullptr;
    for (auto& t : kTexts)
        if (Equals(key, t.key)) found = &t;
    __imp__sub_00070FC9(ctx, base);
    if (!found || !ctx.eax) return;
    std::u16string_view text = found->text;
    for (size_t i = 0; i <= text.size(); i++) W16(ctx.eax + uint32_t(i) * 2, i < text.size() ? text[i] : 0);
}
