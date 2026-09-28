// System Link is called LAN everywhere the game shows it: the co-op button, its page titles and the lobby all
// read [COMMON] MainTitleLan.
#include <string_view>

#include "xdk/hle.h"

using namespace kernel;

namespace {

// Localize(section, key, package, ...), cdecl, returns one of its own TCHAR buffers
constexpr std::u16string_view kKey = u"MainTitleLan", kText = u"LAN";

bool Equals(uint32_t text, std::u16string_view want) {
    if (!text) return false;
    for (size_t i = 0; i <= want.size(); i++)
        if (R16(text + uint32_t(i) * 2) != (i < want.size() ? want[i] : 0)) return false;
    return true;
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_00070FC9);
X86_FUNC(sub_00070FC9) {
    bool lan = Equals(R32(ctx.esp + 8), kKey);
    __imp__sub_00070FC9(ctx, base);
    if (!lan || !ctx.eax) return;
    for (size_t i = 0; i <= kText.size(); i++) W16(ctx.eax + uint32_t(i) * 2, i < kText.size() ? kText[i] : 0);
}
