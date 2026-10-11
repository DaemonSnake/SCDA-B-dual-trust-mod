// Button prompts on keyboard and mouse. A pad icon with its caption beside it goes and the caption takes the key,
// "(SHIFT) Select"; an icon on its own gets the key's name drawn in its place. Nothing is kept between frames, so the
// icons are back as soon as the pad is. Texts with the pad's buttons inline ("Press \\2 to continue.") take the key
// when they are looked up, or when drawn for texts given at run time (the tips page's localized array).
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/input.h"
#include "core/log.h"
#include "game/magma.h"
#include "xdk/hle.h"

using namespace kernel;
using game::magma::Id;

namespace {

// Page: +0xC begin, +0x10 end of instance pointers. Instance: +4 shown, +5 hidden, +0xC keyframe (-1 before the
// first), +0x10 widget. Widget: +0 vtable, +4 frame (+0xC left, +0xE right, +0x10 top, +0x12 bottom, page units).
// Image: +8 material (+4 crc32 of its lowercase name), frame +0x28 colour. Text: +8 font, +0xC text, +0x10 align
// (1 centre), +0x14 flags, frame +0x14 colour, +0x18 shadow colour, +0x1C size (float), +0x20 shadow x and y, +0x24 size.
// Sub page: +0x10 page, +0x24 texts and +0x30 materials it gives its page's instances, +0x38 and +0x34 the defaults.
constexpr uint32_t kImage = 0x333E48, kText = 0x362980, kSubPage = 0x362C58, kTextFrame = 0x362B24;
constexpr uint32_t kBegin = 0x0C, kEnd = 0x10, kWidget = 0x10, kFrame = 0x04, kRect = 0x0C;
// drawer: +0xA4 text and +0xA8 material given to the instance being drawn
constexpr uint32_t kDrawer = 0x3D8D8C, kGivenText = 0xA4, kGivenMaterial = 0xA8;
constexpr uint32_t kGive = 0x269030;  // thiscall(sub page, instance): sets both
constexpr int kReach = 16;            // a caption starts or ends this close to its icon
constexpr int kMargin = 16;           // kept clear at a caption's end, the next legend starts about there
constexpr size_t kMaxText = 0x17F;

struct Icon {
    uint32_t id;
    const char* keys[4];  // actions, or the name itself in capitals
    bool split = false;
};
constexpr Icon kIcons[] = {
    {Id("button_a_color"), {"versus_interact"}},
    {Id("buttona"), {"versus_interact"}},
    {Id("button_b_color"), {"crouch"}},
    {Id("buttonb"), {"crouch"}},
    {Id("button_x_color"), {"versus_scope"}},
    {Id("buttonx"), {"versus_scope"}},
    {Id("button_y_color"), {"versus_jump"}},
    {Id("buttony"), {"versus_jump"}},
    {Id("buttonblack"), {"help"}},
    {Id("button_l"), {"alt_fire"}},
    {Id("button_lt"), {"alt_fire"}},
    {Id("button_r"), {"fire"}},
    {Id("button_rt"), {"fire"}},
    {Id("button_du"), {"laser"}},
    {Id("button_dd"), {"previous_camera"}},
    {Id("button_dl"), {"night_vision"}},
    {Id("button_dpad"), {"laser", "previous_camera"}, true},
    {Id("button_al"), {"move_forward", "strafe_left", "move_back", "strafe_right"}},
    {Id("button_ar"), {"MOUSE"}},
    {Id("button_ar_clic"), {"versus_zoom"}},
};

// an action's first key in capitals, or the name itself when it is in capitals; empty when unbound
std::string KeyOf(const char* key) {
    size_t count = 0;
    const input::Action* actions = input::Actions(&count);
    std::string k;
    if (std::isupper(uint8_t(key[0]))) k = key;
    for (size_t i = 0; i < count && k.empty(); i++)
        if (std::string_view(actions[i].id) == key) k = input::Binding(i);
    k = k.substr(0, k.find(','));
    if (k == "Escape") k = "Esc";
    for (char& c : k) c = char(std::toupper(uint8_t(c)));
    return k;
}

// empty when a key is missing: the icon stays
std::string Label(uint32_t material) {
    uint32_t id = material ? R32(material + 4) : 0;
    const Icon* icon = std::find_if(std::begin(kIcons), std::end(kIcons), [&](const Icon& i) { return i.id == id; });
    if (icon == std::end(kIcons)) return {};
    std::string out;
    for (const char* key : icon->keys) {
        if (!key) break;
        std::string k = KeyOf(key);
        if (k.empty()) return {};
        out += (out.empty() || (k.size() == 1 && !icon->split) ? "" : "/") + k;
    }
    return out;
}

// a text's inline pad buttons: a backslash (the table writes two) and the button's code; A, B and X as the menu
// legends have them (button_a / button_b / button_x above). \\9 (spy bullet listen) is not known
struct Inline {
    char16_t code;
    const char* key;
};
constexpr Inline kInline[] = {{u'2', "versus_interact"}, {u'4', "versus_scope"}, {u'6', "crouch"}};

struct Act {
    enum Kind { Hide, Name, Caption, Skip };
    uint32_t widget;
    Kind kind;
    std::u16string text;      // a caption's key, put before its text when after
    int left = 0, right = 0;  // added to the caption's sides
    bool after = true;
};
std::vector<Act> g_acts;  // of the pages being drawn, game thread
std::vector<size_t> g_pages;
uint32_t g_font = 0;
uint32_t g_block = 0;  // +0 text widget, +0x40 its frame, +0x80 text

const Act* Find(uint32_t widget) {
    for (size_t i = g_acts.size(); i--;)
        if (g_acts[i].widget == widget) return &g_acts[i];
    return nullptr;
}

struct Rect {
    int l, r, t, b;
};
Rect RectOf(uint32_t widget, int dx = 0, int dy = 0) {
    uint32_t f = R32(widget + kFrame) + kRect;
    return {int16_t(R16(f)) + dx, int16_t(R16(f + 2)) + dx, int16_t(R16(f + 4)) + dy, int16_t(R16(f + 6)) + dy};
}

bool Shown(uint32_t inst) { return G<uint8_t>(inst + 4) && !G<uint8_t>(inst + 5) && R32(inst + 0xC) != ~0u; }

// what the instance draws with: its own, or its sub page's
struct Given {
    uint32_t text, material;
};
Given GivenTo(X86Context& ctx, uint32_t sub, uint32_t inst) {
    uint32_t widget = R32(inst + kWidget);
    Given own{R32(widget + 0xC), R32(widget + 8)};
    if (!sub || (!R32(sub + 0x24) && !R32(sub + 0x30) && !R32(sub + 0x34) && !R32(sub + 0x38))) return own;
    uint32_t drawer = R32(kDrawer), text = R32(drawer + kGivenText), material = R32(drawer + kGivenMaterial);
    ctx.ecx = sub;
    CallGuest(ctx, kGive, {inst});
    Given given{R32(drawer + kGivenText), R32(drawer + kGivenMaterial)};
    W32(drawer + kGivenText, text);
    W32(drawer + kGivenMaterial, material);
    return {given.text ? given.text : own.text, given.material ? given.material : own.material};
}

std::u16string Read(uint32_t text) {
    std::u16string out;
    for (; text && out.size() < kMaxText && R16(text); text += 2) out += char16_t(R16(text));
    return out;
}

std::u16string Wide(std::string_view s) { return {s.begin(), s.end()}; }

bool Block() {
    if (!g_block) g_block = mem::AllocSystem(0x400);
    return g_block != 0;
}

uint32_t Write(const std::u16string& text) {
    size_t n = std::min(text.size(), kMaxText);
    for (size_t i = 0; i < n; i++) W16(g_block + 0x80 + uint32_t(i) * 2, uint16_t(text[i]));
    W16(g_block + 0x80 + uint32_t(n) * 2, 0);
    return g_block + 0x80;
}

// the text's width at the widget's size, page units
float Width(X86Context& ctx, uint32_t widget, uint32_t text) {
    uint32_t font = R32(widget + 8), ecx = ctx.ecx;
    if (!font || !G<uint8_t>(font + 0x49)) return 0.0f;
    ctx.ecx = font;
    float width = float(int(CallGuest(ctx, R32(R32(font) + 0x10), {text})));
    ctx.ecx = font;
    bool scales = CallGuest(ctx, R32(R32(font) + 0x14), {}) & 0xFF;
    ctx.ecx = ecx;
    return scales ? width * G<float>(R32(widget + kFrame) + 0x1C) / float(G<uint8_t>(font + 0x49)) : width;
}

struct Found {
    uint32_t widget;
    Rect rect;
    std::string key;
};

// the button icons a page shows; one alone in a sub page counts as its parent's, where its caption is
void Icons(X86Context& ctx, uint32_t page, uint32_t sub, int dx, int dy, bool nested, std::vector<Found>& out) {
    uint32_t begin = R32(page + kBegin), end = R32(page + kEnd);
    if (!begin || end < begin || end - begin > 4 * 256) return;
    for (uint32_t it = begin; it < end; it += 4) {
        uint32_t inst = R32(it), widget = inst ? R32(inst + kWidget) : 0;
        if (!widget || !Shown(inst)) continue;
        uint32_t type = R32(widget);
        if (type == kImage && (!nested || end - begin == 4)) {
            std::string key = Label(GivenTo(ctx, sub, inst).material);
            if (!key.empty() && !Find(widget)) out.push_back({widget, RectOf(widget, dx, dy), key});
        } else if (type == kSubPage && !nested && R32(widget + 0x10)) {
            Rect at = RectOf(widget);
            Icons(ctx, R32(widget + 0x10), widget, at.l, at.r, true, out);
        }
    }
}

}  // namespace

namespace versus {

// a page or a sub page's page is about to draw
void EnterPage(X86Context& ctx, uint32_t page, uint32_t sub) {
    g_pages.push_back(g_acts.size());
    if (!page || !input::KeyboardActive() || !Block()) return;
    uint32_t ecx = ctx.ecx;
    std::vector<Found> icons;
    Icons(ctx, page, sub, 0, 0, false, icons);
    if (!icons.empty()) {
        std::vector<bool> paired(icons.size());
        for (uint32_t it = R32(page + kBegin), end = R32(page + kEnd); it < end; it += 4) {
            uint32_t inst = R32(it), widget = inst ? R32(inst + kWidget) : 0;
            if (!widget || R32(widget) != kText || !Shown(inst)) continue;
            g_font = R32(widget + 8);
            std::u16string text = Read(GivenTo(ctx, sub, inst).text);
            // a number is no caption: the duel's timer sits beside one of its buttons
            auto letter = [](char16_t c) { return ((c | 0x20) >= u'a' && (c | 0x20) <= u'z') || c > 0x7F; };
            if (std::none_of(text.begin(), text.end(), letter)) continue;
            Rect r = RectOf(widget);
            int x = (r.l + r.r) / 2, y = (r.t + r.b) / 2;
            for (size_t i = 0; i < icons.size(); i++) {
                const Rect& c = icons[i].rect;
                if (y < c.t || y > c.b) continue;
                // a stick's or trigger's letter over its icon
                if (text.size() <= 2 && x >= c.l && x <= c.r) {
                    g_acts.push_back({widget, Act::Skip});
                    break;
                }
                if (paired[i]) continue;
                bool after = r.l >= c.l && r.l <= c.r + kReach, before = r.r <= c.r && r.r >= c.l - kReach;
                if (!after && !before) continue;
                std::u16string key = u"(" + Wide(icons[i].key) + u")";
                paired[i] = true;
                g_acts.push_back({icons[i].widget, Act::Hide});
                g_acts.push_back({widget, Act::Caption, key, after ? c.l - r.l : 0, after ? 0 : c.r - r.r, after});
                break;
            }
        }
        for (size_t i = 0; i < icons.size(); i++)
            if (!paired[i]) g_acts.push_back({icons[i].widget, Act::Name, Wide(icons[i].key)});
    }
    ctx.ecx = ecx;
}

void LeavePage() {
    g_acts.resize(g_pages.back());
    g_pages.pop_back();
}

// xbox_live.cpp's text lookup, on what it found: with the keyboard in use, a copy with the inline A and B given their
// keys ("Press (SHIFT) to continue.", as the HUD writes its keys); 0 keeps the text
uint32_t KeyText(uint32_t text) {
    if (!text || !input::KeyboardActive()) return 0;
    std::u16string s = Read(text), out;
    if (s.size() >= kMaxText || s.find(u'\\') == std::u16string::npos) return 0;
    bool changed = false;
    for (size_t i = 0; i < s.size(); i++) {
        size_t j = i;
        while (j < s.size() && s[j] == u'\\') j++;
        const Inline* b = j > i && j < s.size()
                              ? std::find_if(std::begin(kInline), std::end(kInline), [&](const Inline& x) { return x.code == s[j]; })
                              : std::end(kInline);
        std::string key = b != std::end(kInline) ? KeyOf(b->key) : std::string();
        if (key.empty()) {
            out += s[i];
            continue;
        }
        out += Wide("(" + key + ")");
        i = j;
        changed = true;
    }
    if (!changed) return 0;
    static std::map<std::u16string, uint32_t> guest;  // Magma keeps the pointers it is given
    uint32_t& va = guest[out];
    if (!va) {
        va = mem::AllocSystem(uint32_t(out.size() + 1) * 2);
        for (size_t i = 0; i <= out.size(); i++) W16(va + uint32_t(i) * 2, i < out.size() ? uint16_t(out[i]) : 0);
    }
    return va;
}

}  // namespace versus

namespace {

struct PageScope {
    PageScope(X86Context& ctx, uint32_t page, uint32_t sub) { versus::EnterPage(ctx, page, sub); }
    ~PageScope() { versus::LeavePage(); }
};

}  // namespace

// sub page widget draw (thiscall)
X86_EXTERN_FUNC(__imp__sub_002690E0);
X86_FUNC(sub_002690E0) {
    PageScope scope(ctx, R32(ctx.ecx + 0x10), ctx.ecx);
    __imp__sub_002690E0(ctx, base);
}

// text widget draw (thiscall)
X86_EXTERN_FUNC(__imp__sub_00263C30);
X86_FUNC(sub_00263C30) {
    const Act* act = g_acts.empty() ? nullptr : Find(ctx.ecx);
    if (!act || act->kind == Act::Hide || act->kind == Act::Name) {
        uint32_t drawer = R32(kDrawer), given = drawer ? R32(drawer + kGivenText) : 0;
        uint32_t keyed = drawer ? versus::KeyText(given ? given : R32(ctx.ecx + 0xC)) : 0;
        if (!keyed) return __imp__sub_00263C30(ctx, base);
        W32(drawer + kGivenText, keyed);
        __imp__sub_00263C30(ctx, base);
        W32(drawer + kGivenText, given);
        return;
    }
    if (act->kind == Act::Skip) {
        ctx.esp += 4;
        return;
    }
    uint32_t frame = R32(ctx.ecx + kFrame), rect = frame + kRect, drawer = R32(kDrawer);
    uint32_t sides = R32(rect), given = R32(drawer + kGivenText), size = R32(frame + 0x1C);
    int left = int16_t(sides) + act->left, right = int16_t(sides >> 16) + act->right;
    W16(rect, uint16_t(left));
    W16(rect + 2, uint16_t(right));
    // the text it draws now: the interaction prompt's is filled in after the page starts, over a placeholder
    std::u16string text = Read(given ? given : R32(ctx.ecx + 0xC));
    W32(drawer + kGivenText, Write(act->after ? act->text + u" " + text : text + u" " + act->text));
    // a caption too long for its place with the key in it shrinks to fit
    float width = Width(ctx, ctx.ecx, g_block + 0x80), room = float(right - left - kMargin);
    if (room > 0.0f && width > room) G<float>(frame + 0x1C) *= room / width;
    __imp__sub_00263C30(ctx, base);
    W32(frame + 0x1C, size);
    W32(drawer + kGivenText, given);
    W32(rect, sides);
}

// image widget draw (thiscall)
X86_EXTERN_FUNC(__imp__sub_0014C190);
X86_FUNC(sub_0014C190) {
    const Act* act = g_acts.empty() ? nullptr : Find(ctx.ecx);
    if (!act || (act->kind == Act::Name && !g_font)) return __imp__sub_0014C190(ctx, base);
    if (act->kind != Act::Name) {
        ctx.esp += 4;
        return;
    }
    // the key's name over the icon's place, in a text widget of our own
    uint32_t image = R32(ctx.ecx + kFrame), widget = g_block, frame = g_block + 0x40, drawer = R32(kDrawer);
    Rect r = RectOf(ctx.ecx);
    int size = std::clamp((r.b - r.t) / 2, 10, 16), x = (r.l + r.r) / 2, y = (r.t + r.b) / 2 - size / 2 - 2;
    for (uint32_t i = 0; i < 0x80; i += 4) W32(g_block + i, 0);
    W32(widget, kText);
    W32(widget + kFrame, frame);
    W32(widget + 8, g_font);
    W32(widget + 0x10, 1);
    W32(widget + 0x14, 0x200);
    W32(frame, kTextFrame);
    W32(frame + 8, ~0u);
    W16(frame + kRect, uint16_t(int16_t(x - 100)));
    W16(frame + kRect + 2, uint16_t(int16_t(x + 100)));
    W16(frame + kRect + 4, uint16_t(int16_t(y)));
    W16(frame + kRect + 6, uint16_t(int16_t(y + size)));
    // the icon's own colour: the duel dims the buttons not to press
    uint32_t colour = R32(image + 0x28);
    W32(frame + 0x14, colour);
    W32(frame + 0x18, colour & 0xFF000000);
    G<float>(frame + 0x1C) = float(size);
    W32(frame + 0x24, uint32_t(size));
    uint32_t given = R32(drawer + kGivenText);
    W32(drawer + kGivenText, Write(act->text));
    ctx.ecx = widget;
    __imp__sub_00263C30(ctx, base);
    W32(drawer + kGivenText, given);
}
