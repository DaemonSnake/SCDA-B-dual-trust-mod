// Xbox Live is gone from the versus menus: the sign-in status after the profile name, the X sign-in key and its
// legend on the main menu, the XBOX LIVE button, whose place goes to System Link, renamed LAN, and Appear Offline,
// which leaves the "System Link / Xbox Live" options page to voice alone, renamed Voice. The save box speaks of the PC.
// Deleting this file brings them back.
#include <string_view>

#include "xdk/hle.h"

using namespace kernel;

namespace versus {
uint32_t OptionsText(uint32_t section, uint32_t section2, uint32_t key);  // options_menu.cpp
}  // namespace versus

namespace {

// localization database find (thiscall(section, section2, key*, package), ret 0x10): returns the text in the table.
// Sections and keys are cooked name indices, or crc32 of the lowercase name for Magma pages and elements.
struct Text {
    uint32_t section, key;
    std::u16string_view text;
};
constexpr uint32_t kMagma = 0x178B;  // [Magma]
constexpr uint32_t kMainMenu = 0x8BBFE343, kSystemLinkPage = 0xE12C2CDF, kOptionsPage = 0xD7F513E8;
constexpr uint32_t kOnlineOptionsPage = 0xE95B9F21;
constexpr uint32_t kXboxLive = 0x44F5EDB2, kSystemLink = 0xB67F2AD0, kTutorials = 0xC21EFE28, kOptions = 0xD035FA87;
constexpr uint32_t kTitle = 0x2B36786B, kOnline = 0x9E32BEEA;
constexpr Text kTexts[] = {
    {kMagma, 0x5065, u""},  // Live_NoSignIn "Not signed In"
    {kMagma, 0x5064, u""},  // Live_FailedSignIn "Sign-In Failed"
    {kMagma, 0x4E83, u"Please don't close the game or turn off your PC."},  // PopUp_MC_Body, the save box
    {kMainMenu, kSystemLink, u"LAN"},
    {kSystemLinkPage, kTitle, u"LAN"},
    {kOptionsPage, kOnline, u"VOICE"},
    {kOnlineOptionsPage, kTitle, u"Voice Options"},
};

// Magma localize by [Magma] key (cdecl(key)); the main menu asks it for the X legend just before this return address
constexpr uint32_t kSignInLegend = 0x1A764E;
constexpr uint32_t kNullText = 0x4F05;

// Menu pages: +0x28 the Magma page; P_Options_Online +0xC8 CHK_Appear. Page: +0xC begin, +0x10 end of instance pointers, +0x24 focus.
// Button and checkbox instance: +8 id, +0x10 widget (+4 its current position frame), +0x18 begin, +0x1C end of keyframe pointers
// (keyframe +0x18 its position frame), +0xA8 up and +0xAC down neighbour instances, +0xB8 bit 0 default.
// Position frame (vtable 0x361FC8): +0xE y (short, page units).
constexpr uint32_t kPage = 0x28, kBegin = 0x0C, kEnd = 0x10, kFocus = 0x24;
constexpr uint32_t kId = 0x08, kWidget = 0x10, kKeyframes = 0x18, kKeyFrame = 0x18, kWidgetFrame = 0x04;
constexpr uint32_t kUp = 0xA8, kDown = 0xAC, kFlags = 0xB8, kDefault = 0x01;
constexpr uint32_t kPositionFrame = 0x361FC8, kY = 0x0E;
constexpr uint32_t kSetEnabled = 0x25CFB0;  // thiscall instance (bool)
constexpr uint32_t kResetFocus = 0x25C1D0;  // cdecl(page): the default enabled button takes the focus
constexpr int kOffPage = -1000;             // no hidden flag in this Magma: the button is moved off the canvas
constexpr uint32_t kAppear = 0xC8;

uint32_t Guest(std::u16string_view text) {
    uint32_t va = mem::AllocSystem(uint32_t(text.size() + 1) * 2);
    for (size_t i = 0; i <= text.size(); i++) W16(va + uint32_t(i) * 2, i < text.size() ? text[i] : 0);
    return va;
}

uint32_t Instance(uint32_t page, uint32_t id) {
    for (uint32_t it = R32(page + kBegin), end = R32(page + kEnd); it && it < end; it += 4)
        if (uint32_t inst = R32(it); inst && R32(inst + kId) == id) return inst;
    return 0;
}

template <typename F>
void EachFrame(uint32_t inst, F f) {
    uint32_t begin = R32(inst + kKeyframes), end = R32(inst + kKeyframes + 4);
    if (begin && end >= begin && end - begin <= 4 * 64)
        for (uint32_t it = begin; it < end; it += 4)
            if (uint32_t key = R32(it)) f(R32(key + kKeyFrame));
    if (uint32_t widget = R32(inst + kWidget)) f(R32(widget + kWidgetFrame));
}

int FirstY(uint32_t inst) {
    int y = 0x7FFF;
    EachFrame(inst, [&](uint32_t frame) {
        if (y == 0x7FFF && frame && R32(frame) == kPositionFrame) y = int16_t(R16(frame + kY));
    });
    return y;
}

void Move(uint32_t inst, int dy) {
    EachFrame(inst, [&](uint32_t frame) {
        if (frame && R32(frame) == kPositionFrame) W16(frame + kY, uint16_t(int16_t(R16(frame + kY)) + dy));
    });
}

// Takes a button out of its page: disabled and off the canvas, the rows under it up one, its neighbours linked past
// it, the default and the focus to the one below. Returns the row height, 0 when the page was done already.
int Remove(X86Context& ctx, uint32_t page, uint32_t gone, std::initializer_list<uint32_t> below) {
    uint32_t up = R32(gone + kUp), down = R32(gone + kDown);
    if (!up || !down || R32(down + kUp) != gone || !below.size()) return 0;
    int row = FirstY(*below.begin()) - FirstY(gone);
    if (row > 0 && row < 100)
        for (uint32_t inst : below) Move(inst, -row);
    Move(gone, kOffPage);
    ctx.ecx = gone;
    CallGuest(ctx, kSetEnabled, {0});
    if (G<uint8_t>(gone + kFlags) & kDefault) {
        G<uint8_t>(gone + kFlags) &= ~kDefault;
        G<uint8_t>(down + kFlags) |= kDefault;
    }
    W32(up + kDown, down);
    W32(down + kUp, up);
    W32(page + kFocus, 0);
    CallGuest(ctx, kResetFocus, {page});
    return row;
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_00157A90);
X86_FUNC(sub_00157A90) {
    uint32_t section = R32(ctx.esp + 4), section2 = R32(ctx.esp + 8), key = R32(ctx.esp + 0xC);
    uint32_t id = key ? R32(key) : 0;
    if (uint32_t text = versus::OptionsText(section, section2, id)) {
        ctx.eax = text;
        ctx.esp += 0x14;
        return;
    }
    for (auto& t : kTexts) {
        if ((section != t.section && section2 != t.section) || id != t.key) continue;
        static const auto guest = [] {
            std::array<uint32_t, std::size(kTexts)> va{};
            for (size_t i = 0; i < va.size(); i++) va[i] = Guest(kTexts[i].text);
            return va;
        }();
        ctx.eax = guest[&t - kTexts];
        ctx.esp += 0x14;
        return;
    }
    __imp__sub_00157A90(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_00192390);
X86_FUNC(sub_00192390) {
    if (R32(ctx.esp) == kSignInLegend) W32(ctx.esp + 4, kNullText);
    __imp__sub_00192390(ctx, base);
}

// main menu button keys (thiscall(?, event), ret 8): event 0x82 is X, which signs in or out
X86_EXTERN_FUNC(__imp__sub_001A71C0);
X86_FUNC(sub_001A71C0) {
    if (R32(R32(ctx.esp + 8)) != 0x82) return __imp__sub_001A71C0(ctx, base);
    ctx.eax = (ctx.eax & ~0xFFu) | 1;
    ctx.esp += 0xC;
}

// P_MainMenu open (thiscall, no arguments): XBOX LIVE goes, System Link, Tutorials and Options move up a row
X86_EXTERN_FUNC(__imp__sub_001A72F0);
X86_FUNC(sub_001A72F0) {
    uint32_t menu = ctx.ecx;
    __imp__sub_001A72F0(ctx, base);
    uint32_t page = R32(menu + kPage);
    if (!page) return;
    uint32_t live = Instance(page, kXboxLive), lan = Instance(page, kSystemLink);
    uint32_t tutorials = Instance(page, kTutorials), options = Instance(page, kOptions);
    if (!live || !lan || !tutorials || !options) return;
    if (int row = Remove(ctx, page, live, {lan, tutorials, options}))
        LOGI("versus: XBOX LIVE button removed, LAN / Tutorials / Options up %d", row);
}

// P_Options_Online open (thiscall, no arguments): Appear Offline goes, Voice On Speaker moves up a row
X86_EXTERN_FUNC(__imp__sub_001C4910);
X86_FUNC(sub_001C4910) {
    uint32_t menu = ctx.ecx;
    __imp__sub_001C4910(ctx, base);
    uint32_t page = R32(menu + kPage), appear = R32(menu + kAppear);
    if (!page || !appear) return;
    if (int row = Remove(ctx, page, appear, {R32(appear + kDown)}))
        LOGI("versus: Appear Offline removed, Voice On Speaker up %d", row);
}
