// 2D is drawn 640x480 and shown 16:9. Canvas flushes get x' = x * 3/4 + offset * width: HUD elements are
// placed per page and element, menus and loading screens are centred 4:3. A 4:3 output is shown 4:3: nothing moves.
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

#include "app/app.h"
#include "gpu/backend.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

constexpr uint32_t Crc32(std::string_view text) {
    uint32_t crc = ~0u;
    for (char c : text) {
        crc ^= uint8_t(c);
        for (int i = 0; i < 8; i++) crc = crc >> 1 ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
static_assert(Crc32("P_hud") == 0x330C9548);

constexpr uint32_t kMagma = 0x6A6638, kMagmaScreen = 0x6A6668, kFlush = 0x28EDCC;

// Centre: 4:3 in the middle; Fill: the same, images touching a page side are stretched out to the screen
// side; Extend: the same, but a strip repeating the edge texels is drawn instead, so shapes stay round;
// Left, Right: 4:3 against that side; Stretch: page width over screen width;
// Spread: centred, image edges further than kKeep from the middle move out to the screen sides;
// Open: not decided here, its children are placed one by one
enum class Mode { Centre, Fill, Extend, Left, Right, Stretch, Spread, Open };

struct Rule {
    uint32_t id;
    Mode mode;
    constexpr Rule(std::string_view name, Mode m) : id(Crc32(name)), mode(m) {}
    constexpr Rule(uint32_t i, Mode m) : id(i), mode(m) {}
};

// Magma names pages and elements by crc32 of their name. The goggles' vignette, lens ring and frame lines
// are arcs of one circle: only a uniform stretch keeps them on one curve
constexpr Rule kHud[] = {
    {"A_Goggles", Mode::Stretch},      {"A_HUD", Mode::Right},          {"A_Interact_NPC", Mode::Right},
    {"A_Interact_RTN", Mode::Right},    {"B_interact_NPC", Mode::Right}, {"B_interact_RTN", Mode::Right},
    {"List_interaction_NPC", Mode::Right}, {"List_interaction_RTN", Mode::Right}, {"A_Goals", Mode::Right},
    {"A_Notes", Mode::Right},           {"L_Alarms", Mode::Right},       {"A_icon_coopaction", Mode::Right},
    {"A_Info_teamate", Mode::Left},     {"I_Talking", Mode::Left},       {"A_Communications", Mode::Left},
    {"A_timer", Mode::Centre},          {"Sound_Meter", Mode::Right},
    {0x467FE0A7, Mode::Open},  // scripted event bar: letterbox and subtitle stay, the Skip prompt goes to its side
};
constexpr Rule kOverlay[] = {
    {"A_noise", Mode::Stretch}, {"A_goggles", Mode::Stretch}, {"A_Goggles", Mode::Stretch},
    {"A_timer", Mode::Right},   {"A_Communications", Mode::Left},
    {0x478A341D, Mode::Left},  // P_Sniper's instance of the comms area, its name not recovered
};

// every menu page's button legend (A, B, X, Y), an instance of system.mgm's area, name not recovered
constexpr Rule kMenu[] = {
    {0xCF490E0F, Mode::Left},
};

// half: an unlisted element sitting in one half of the page goes to that side
struct Page {
    uint32_t id;
    std::span<const Rule> rules;
    Mode others;
    bool half;
    constexpr Page(std::string_view name, std::span<const Rule> r, Mode m = Mode::Fill, bool h = false)
        : id(Crc32(name)), rules(r), others(m), half(h) {}
};

// pages not listed are menus: Fill. The scope's side strips hold the outer ring's left and right arcs, which
// stretching bulges: Extend repeats their outer edge instead
constexpr Page kPages[] = {
    {"P_hud", kHud, Mode::Fill, true}, {"P_EEV", kOverlay, Mode::Spread}, {"P_Sniper", kOverlay, Mode::Extend},
    {"P_Camera", kOverlay},            {"P_opticcable", kOverlay},        {"P_Computer", kOverlay},
};

constexpr int kSlack = 2, kKeep = 150, kMinWidth = 8;  // page units
constexpr float kLeft = 0.0f, kCentre = 0.125f, kRight = 0.25f, kNone = -1.0f;

float g_offset = kCentre;
Mode g_mode = Mode::Centre;
const Page* g_page = nullptr;
bool g_hud_seen = false, g_decided = false;
bool g_in_level = false;  // P_hud was drawn last frame
bool g_sniper_seen = false, g_scope = false;  // P_Sniper drawn this frame, and last frame

// magma manager +0x34/+0x38 viewport, viewport +0x5C canvas, canvas +0x84 its FCanvasUtil
void FlushCanvas(X86Context& ctx) {
    uint32_t m = R32(kMagma);
    uint32_t vp = m ? R32(m + 0x34) : 0;
    if (vp && R32(m + 0x38)) vp = R32(m + 0x38);
    uint32_t canvas = vp ? R32(vp + 0x5C) : 0, util = canvas ? R32(canvas + 0x84) : 0;
    if (!util) return;
    uint32_t ecx = ctx.ecx;
    ctx.ecx = util;
    CallGuest(ctx, kFlush, {1});
    ctx.ecx = ecx;
}

template <typename F>
void Draw(X86Context& ctx, uint8_t* base, Mode mode, F original) {
    FlushCanvas(ctx);
    float offset = g_offset;
    Mode saved = g_mode;
    g_mode = mode;
    g_offset = mode == Mode::Stretch ? kNone : mode == Mode::Left ? kLeft : mode == Mode::Right ? kRight : kCentre;
    original(ctx, base);
    FlushCanvas(ctx);
    g_offset = offset;
    g_mode = saved;
}

// page rect of an unlisted element; GetBounds clears the screen's per child text and font, kept here
Mode ByHalf(X86Context& ctx, uint32_t instance) {
    uint32_t widget = R32(instance + 8), screen = R32(kMagmaScreen);
    if (!widget || !screen) return g_page->others;
    int sw = int16_t(R16(screen + 0x84));
    uint32_t esp = ctx.esp, rect = (esp - 0x40) & ~0xFu, text = R32(screen + 0x7C), font = R32(screen + 0x80);
    W32(rect, 0x80007FFF);
    ctx.esp = rect - 0x10;
    ctx.ecx = widget;
    CallGuest(ctx, R32(R32(widget) + 0x10), {rect});
    ctx.esp = esp;
    ctx.ecx = instance;
    W32(screen + 0x7C, text);
    W32(screen + 0x80, font);
    int l = int16_t(R16(rect)), r = int16_t(R16(rect + 2));
    Mode mode = l > r ? g_page->others : r <= sw / 2 + kSlack ? Mode::Left : l >= sw / 2 - kSlack ? Mode::Right : g_page->others;
    LOG_FIRST(100, "hud: page %08X element %08X x %d..%d mode %d", g_page->id, R32(instance + 4), l, r, int(mode));
    return mode;
}

}  // namespace

// FCanvasUtil::DrawQuad(4 x {x, y, u, v, colour}, z, material, rotation), stdcall 0x5C. Magma images only
// (called from 0x2A1181), text keeps its glyphs together. Spread and Fill move edges; Extend draws the quad,
// then a strip from each page side it touches out to the screen side with that edge's texels.
X86_EXTERN_FUNC(__imp__sub_0028F090);
X86_FUNC(sub_0028F090) {
    uint32_t screen = R32(kMagmaScreen);
    int sw = screen ? int16_t(R16(screen + 0x84)) : 0;
    bool edges = g_mode == Mode::Fill || g_mode == Mode::Extend || g_mode == Mode::Spread;
    if (!edges || !app::Widescreen() || sw <= 0 || R32(ctx.esp) != 0x2A1186) return __imp__sub_0028F090(ctx, base);
    uint32_t esp = ctx.esp, util = ctx.ecx, args = esp + 4;
    float w = float(int(R32(util + 0x28))), unit = w / sw, side = w / 6;
    auto x = [&](int i) -> float& { return G<float>(args + i * 0x14); };
    if (g_mode == Mode::Spread) {
        for (int i = 0; i < 4; i++) x(i) += x(i) < w / 2 - kKeep * unit ? -side : x(i) > w / 2 + kKeep * unit ? side : 0;
        return __imp__sub_0028F090(ctx, base);
    }
    float lo = std::min({x(0), x(1), x(2), x(3)}), hi = std::max({x(0), x(1), x(2), x(3)});
    bool left = lo <= kSlack * unit, right = hi >= w - kSlack * unit;
    if (g_mode == Mode::Fill) {
        if (hi - lo >= kMinWidth * unit)
            for (int i = 0; i < 4; i++) {
                // one page unit past the screen side: exactly on it, the last pixel column is left uncovered
                if (left && x(i) == lo) x(i) = -side - unit;
                if (right && x(i) == hi) x(i) = w + side + unit;
            }
        return __imp__sub_0028F090(ctx, base);
    }
    uint32_t saved[23];
    std::memcpy(saved, &G<uint32_t>(args), sizeof saved);
    auto was = [&](int i, int field) { return std::bit_cast<float>(saved[i * 5 + field]); };
    int at_lo = 0;
    for (int i = 0; i < 4; i++) at_lo += x(i) == lo;
    __imp__sub_0028F090(ctx, base);
    if (at_lo != 2 || hi - lo < kMinWidth * unit) return;
    for (float from : {lo, hi}) {
        if (from == lo ? !left : !right) continue;
        std::memcpy(&G<uint32_t>(args), saved, sizeof saved);
        for (int i = 0; i < 4; i++) {
            if (was(i, 0) == from) {
                x(i) = from == lo ? -side : w + side;
                continue;
            }
            // the inner vertex takes its edge partner's texel, so the strip repeats the edge column
            for (int j = 0; j < 4; j++)
                if (was(j, 0) == from && was(j, 1) == was(i, 1)) {
                    G<float>(args + i * 0x14 + 8) = was(j, 2);
                    G<float>(args + i * 0x14 + 12) = was(j, 3);
                }
            x(i) = from;
        }
        ctx.esp = esp;
        ctx.ecx = util;
        __imp__sub_0028F090(ctx, base);
    }
}

// FCanvasUtil::Flush: +0x28 width, +0x2C height, +0x74 screen to clip matrix, x row 0 and translation row 3
X86_EXTERN_FUNC(__imp__sub_0028EDCC);
X86_FUNC(sub_0028EDCC) {
    uint32_t util = ctx.ecx;
    int w = int(R32(util + 0x28)), h = int(R32(util + 0x2C));
    if (g_offset < 0 || w < 640 || h < 448 || !app::Widescreen()) return __imp__sub_0028EDCC(ctx, base);
    float& scale = G<float>(util + 0x74);
    float& move = G<float>(util + 0xA4);
    float s = scale, m = move;
    move = m + g_offset * w * s;
    scale = s * 0.75f;
    __imp__sub_0028EDCC(ctx, base);
    G<float>(util + 0x74) = s;
    G<float>(util + 0xA4) = m;
}

// Screen state of an element (thiscall(state, element), ret 8; state +8 left, +0xA right, page units). Rect shapes
// (sub_002BD93C, called back at 0x2BD973) are drawn from it straight to the render interface in clip space, past the
// canvas flush, so their x is narrowed here the way the flush narrows everything else; on edge-pulling pages a side
// touching the page's side goes past the screen's, as images do (cinematic letterbox, fades).
X86_EXTERN_FUNC(__imp__sub_002CC260);
X86_FUNC(sub_002CC260) {
    uint32_t ret = R32(ctx.esp), state = R32(ctx.esp + 4), screen = R32(kMagmaScreen);
    __imp__sub_002CC260(ctx, base);
    int sw = screen ? int16_t(R16(screen + 0x84)) : 0;
    if (ret != 0x2BD973 || g_offset < 0 || sw <= 0 || !app::Widescreen()) return;
    int l = int16_t(R16(state + 8)), r = int16_t(R16(state + 0xA));
    bool edges = g_mode == Mode::Fill || g_mode == Mode::Extend || g_mode == Mode::Spread;
    auto narrow = [&](int x) { return int(std::lround(x * 0.75f + g_offset * sw)); };
    W16(state + 8, uint16_t(int16_t(edges && l <= kSlack ? -1 : narrow(l))));
    W16(state + 0xA, uint16_t(int16_t(edges && r >= sw - kSlack ? sw + 1 : narrow(r))));
}

// Render interface screen quad setup (sub_002E3011, thiscall(ri, x0, y0, x1, y1, u0, v0, u1, v1), ret 0x24): corners
// in viewport pixels (ri +0x4B84 width), turned into clip space for a textured quad, which the canvas flush never
// sees. Bink movies inside a Magma placeholder (training videos, PH_Video) come through it and are narrowed into the
// centred 4:3 like the page around it. A quad the full viewport wide (full screen movies, post effects) is left as
// it is.
X86_EXTERN_FUNC(__imp__sub_002E3011);
X86_FUNC(sub_002E3011) {
    uint32_t ri = R32(ctx.esp + 4);
    if (ri && app::Widescreen()) {
        float w = float(int(R32(ri + 0x4B84)));
        float& x0 = G<float>(ctx.esp + 8);
        float& x1 = G<float>(ctx.esp + 0x10);
        if (w > 0 && (std::min(x0, x1) > kSlack || std::max(x0, x1) < w - kSlack)) {
            x0 = x0 * 0.75f + kCentre * w;
            x1 = x1 * 0.75f + kCentre * w;
        }
    }
    __imp__sub_002E3011(ctx, base);
}

// Magma page draw (thiscall, no arguments); page +0x14 id, +0x1C layer. 100 is the default layer, the HUD's in a
// level but also the menus' lists, so HIDE HUD only acts while P_hud is up
X86_EXTERN_FUNC(__imp__sub_002BE990);
X86_FUNC(sub_002BE990) {
    uint32_t id = R32(ctx.ecx + 0x14);
    g_hud_seen |= id == Crc32("P_hud");
    if (app::Get().hide_hud && g_in_level && R32(ctx.ecx + 0x1C) == 100) {
        ctx.esp += 4;
        return;
    }
    const Page* saved = g_page;
    g_page = nullptr;
    for (auto& p : kPages)
        if (p.id == id) g_page = &p;
    Draw(ctx, base, g_page ? Mode::Centre : Mode::Fill, __imp__sub_002BE990);
    g_page = saved;
}

// Magma element draw, children come back through here: the first listed or placed element decides. On menu
// pages only the listed elements are placed, the rest stay with the page
X86_EXTERN_FUNC(__imp__sub_002C64E0);
X86_FUNC(sub_002C64E0) {
    uint32_t instance = ctx.ecx;
    if (g_decided || !(G<uint8_t>(instance + 0x18) & 1)) return __imp__sub_002C64E0(ctx, base);
    uint32_t id = R32(instance + 4);
    std::span<const Rule> rules = g_page ? g_page->rules : std::span<const Rule>(kMenu);
    auto rule = std::find_if(rules.begin(), rules.end(), [&](const Rule& r) { return r.id == id; });
    if (rule == rules.end() ? !g_page : rule->mode == Mode::Open) return __imp__sub_002C64E0(ctx, base);
    Mode mode = rule != rules.end() ? rule->mode : g_page->half ? ByHalf(ctx, instance) : g_page->others;
    g_decided = true;
    Draw(ctx, base, mode, __imp__sub_002C64E0);
    g_decided = false;
}

// Magma frame; canvas outside pages is full width in a level, 4:3 in menus
X86_EXTERN_FUNC(__imp__sub_002303E8);
X86_FUNC(sub_002303E8) {
    __imp__sub_002303E8(ctx, base);
    g_offset = g_hud_seen ? kNone : kCentre;
    g_in_level = g_hud_seen;
    g_hud_seen = false;
}

// render device splash(path, ...): draws a tga over the frame buffer and presents it; the ESRB notice
// has its side columns carried out to the screen edges
X86_EXTERN_FUNC(__imp__sub_002E5F95);
X86_FUNC(sub_002E5F95) {
    std::string path;
    uint32_t p = R32(ctx.esp + 4), step = p && !G<uint8_t>(p + 1) ? 2 : 1;  // TCHAR or char
    for (uint32_t i = 0; p && i < 256 && G<uint8_t>(p + i * step); i++) path += char(std::tolower(G<uint8_t>(p + i * step)));
    gpu::g_splash_fill = path.find("esrb") != std::string::npos;
    gpu::g_splash = gpu::g_in_splash = true;
    __imp__sub_002E5F95(ctx, base);
    gpu::g_in_splash = false;
}
