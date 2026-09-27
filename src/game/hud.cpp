// 2D is drawn 640x480 and shown 16:9. Canvas flushes get x' = x * 3/4 + offset * width: HUD elements are
// placed per page and element, menus and loading screens are centred 4:3.
#include <algorithm>
#include <bit>
#include <cctype>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

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
// Spread: centred, image edges further than kKeep from the middle move out to the screen sides
enum class Mode { Centre, Fill, Extend, Left, Right, Stretch, Spread };

struct Rule {
    uint32_t id;
    Mode mode;
    constexpr Rule(std::string_view name, Mode m) : id(Crc32(name)), mode(m) {}
};

// Magma names pages and elements by crc32 of their name
constexpr Rule kHud[] = {
    {"A_Goggles", Mode::Extend},       {"A_HUD", Mode::Right},          {"A_Interact_NPC", Mode::Right},
    {"A_Interact_RTN", Mode::Right},    {"B_interact_NPC", Mode::Right}, {"B_interact_RTN", Mode::Right},
    {"List_interaction_NPC", Mode::Right}, {"List_interaction_RTN", Mode::Right}, {"A_Goals", Mode::Right},
    {"A_Notes", Mode::Right},           {"L_Alarms", Mode::Right},       {"A_icon_coopaction", Mode::Right},
    {"A_Info_teamate", Mode::Right},    {"I_Talking", Mode::Right},      {"A_Communications", Mode::Left},
    {"A_timer", Mode::Left},            {"Sound_Meter", Mode::Right},
};
constexpr Rule kOverlay[] = {
    {"A_noise", Mode::Stretch}, {"A_goggles", Mode::Extend}, {"A_Goggles", Mode::Extend},
    {"A_timer", Mode::Left},    {"A_Communications", Mode::Left},
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

// pages not listed are menus: Fill
constexpr Page kPages[] = {
    {"P_hud", kHud, Mode::Fill, true}, {"P_EEV", kOverlay, Mode::Spread}, {"P_Sniper", kOverlay},
    {"P_Camera", kOverlay},            {"P_opticcable", kOverlay},        {"P_Computer", kOverlay},
};

constexpr int kSlack = 2, kKeep = 150, kMinWidth = 8;  // page units
constexpr float kLeft = 0.0f, kCentre = 0.125f, kRight = 0.25f, kNone = -1.0f;

float g_offset = kCentre;
Mode g_mode = Mode::Centre;
const Page* g_page = nullptr;
bool g_hud_seen = false, g_decided = false;

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
    if (!edges || sw <= 0 || R32(ctx.esp) != 0x2A1186) return __imp__sub_0028F090(ctx, base);
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
                if (left && x(i) == lo) x(i) = -side;
                if (right && x(i) == hi) x(i) = w + side;
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
    if (g_offset < 0 || w < 640 || h < 448) return __imp__sub_0028EDCC(ctx, base);
    float& scale = G<float>(util + 0x74);
    float& move = G<float>(util + 0xA4);
    float s = scale, m = move;
    move = m + g_offset * w * s;
    scale = s * 0.75f;
    __imp__sub_0028EDCC(ctx, base);
    G<float>(util + 0x74) = s;
    G<float>(util + 0xA4) = m;
}

// Magma page draw
X86_EXTERN_FUNC(__imp__sub_002BE990);
X86_FUNC(sub_002BE990) {
    uint32_t id = R32(ctx.ecx + 0x14);
    g_hud_seen |= id == Crc32("P_hud");
    const Page* saved = g_page;
    g_page = nullptr;
    for (auto& p : kPages)
        if (p.id == id) g_page = &p;
    Draw(ctx, base, g_page ? Mode::Centre : Mode::Fill, __imp__sub_002BE990);
    g_page = saved;
}

// Magma element draw, children come back through here: the first listed or placed element decides
X86_EXTERN_FUNC(__imp__sub_002C64E0);
X86_FUNC(sub_002C64E0) {
    uint32_t instance = ctx.ecx;
    if (!g_page || g_decided || !(G<uint8_t>(instance + 0x18) & 1)) return __imp__sub_002C64E0(ctx, base);
    uint32_t id = R32(instance + 4);
    auto rule = std::find_if(g_page->rules.begin(), g_page->rules.end(), [&](const Rule& r) { return r.id == id; });
    Mode mode = rule != g_page->rules.end() ? rule->mode : g_page->half ? ByHalf(ctx, instance) : g_page->others;
    g_decided = true;
    Draw(ctx, base, mode, __imp__sub_002C64E0);
    g_decided = false;
}

// Magma frame; canvas outside pages is full width in a level, 4:3 in menus
X86_EXTERN_FUNC(__imp__sub_002303E8);
X86_FUNC(sub_002303E8) {
    __imp__sub_002303E8(ctx, base);
    g_offset = g_hud_seen ? kNone : kCentre;
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
