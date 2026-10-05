// 2D is drawn 640x480 and shown as wide as the output. Canvas flushes get x' = x * narrow + offset * spare width:
// HUD elements are placed per page and element, menus and loading screens are centred 4:3. On 4:3 nothing moves.
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "app/app.h"
#include "game/loading.h"
#include "gpu/backend.h"
#include "xdk/hle.h"

using namespace kernel;

namespace game {
void EventPrompts(X86Context& ctx, uint32_t bar);                             // prompts.cpp
bool ComboElement(X86Context& ctx, uint8_t* base, uint32_t instance);  // prompts.cpp
bool HintElement(X86Context& ctx, uint8_t* base, uint32_t instance);   // prompts.cpp
bool WordingElement(X86Context& ctx, uint8_t* base, uint32_t instance);  // prompts.cpp
bool OptionsElement(X86Context& ctx, uint8_t* base, uint32_t instance);  // options_menu.cpp
}

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

constexpr uint32_t kEventBar = 0x467FE0A7;  // P_hud's scripted event bar, name not recovered
constexpr uint32_t kFileView = 0x4E6BBCC7;  // computer.mgm's open file page over P_Computer, name not recovered

// Magma names pages and elements by crc32 of their name. The goggles' vignette, lens ring and frame lines
// are arcs of one circle: only a uniform stretch keeps them on one curve
constexpr Rule kHud[] = {
    {"A_Goggles", Mode::Stretch},      {"A_HUD", Mode::Right},          {"A_Interact_NPC", Mode::Right},
    {"A_Interact_RTN", Mode::Right},    {"B_interact_NPC", Mode::Right}, {"B_interact_RTN", Mode::Right},
    {"List_interaction_NPC", Mode::Right}, {"List_interaction_RTN", Mode::Right}, {"A_Goals", Mode::Right},
    {"A_Notes", Mode::Right},           {"L_Alarms", Mode::Right},       {"A_icon_coopaction", Mode::Right},
    {"A_Info_teamate", Mode::Left},     {"I_Talking", Mode::Left},       {"A_Communications", Mode::Left},
    {"A_timer", Mode::Centre},          {"Sound_Meter", Mode::Right},
    {kEventBar, Mode::Open},  // scripted event bar: letterbox and subtitle stay, the Skip prompt goes to its side
};
constexpr Rule kOverlay[] = {
    {"A_noise", Mode::Stretch}, {"A_goggles", Mode::Stretch}, {"A_Goggles", Mode::Stretch},
    {"A_timer", Mode::Right},   {"A_Communications", Mode::Left},
    // the controller prompt pairs (system.mgm legend areas): Spread would move their icons away from their text
    {0xA2ADD30F, Mode::Centre}, {0x640C6602, Mode::Centre},
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
    constexpr Page(uint32_t i, std::span<const Rule> r, Mode m = Mode::Fill, bool h = false)
        : id(i), rules(r), others(m), half(h) {}
};

// pages not listed are menus: Fill. The scope's side strips hold the outer ring's left and right arcs, which
// stretching bulges: Extend repeats their outer edge instead. The open file page draws its own comms box and legend
// over P_Computer's, so it is placed the same way or both show
constexpr Page kPages[] = {
    {"P_hud", kHud, Mode::Fill, true}, {"P_EEV", kOverlay, Mode::Spread}, {"P_Sniper", kOverlay, Mode::Extend},
    {"P_Camera", kOverlay},            {"P_opticcable", kOverlay},        {"P_Computer", kOverlay},
    {kFileView, kOverlay},
};

constexpr int kSlack = 2, kKeep = 150, kMinWidth = 8;  // page units
// share of the width a 4:3 page leaves spare that goes left of it
constexpr float kLeft = 0.0f, kCentre = 0.5f, kRight = 1.0f, kNone = -1.0f;

float g_offset = kCentre;
Mode g_mode = Mode::Centre;
const Page* g_page = nullptr;
uint32_t g_drawing = 0;  // the Magma page drawing
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
    float k = app::Narrow(), w = float(int(R32(util + 0x28))), unit = w / sw, side = w * (1 - k) / (2 * k);
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
    float s = scale, m = move, k = app::Narrow();
    move = m + g_offset * (1 - k) * w * s;
    scale = s * k;
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
    float k = app::Narrow();
    auto narrow = [&](int x) { return int(std::lround(x * k + g_offset * (1 - k) * sw)); };
    W16(state + 8, uint16_t(int16_t(edges && l <= kSlack ? -1 : narrow(l))));
    W16(state + 0xA, uint16_t(int16_t(edges && r >= sw - kSlack ? sw + 1 : narrow(r))));
}

namespace {
bool g_esrb = false;     // the ESRB notice is being shown
bool g_loading = false;  // a loading picture is up, no Magma page drawn since
bool g_fade = false;     // ShowScreen fades between a 4:3 frame and a full width one
bool g_new_43 = false;   // the frame it fades to is 4:3
bool g_old_43 = false;   // the frame it fades from is 4:3, until the fade draws over its memory

// a full screen movie is fitted for a 16:9 frame (pixel aspect 4/3) about the centre: redone for the frame's shape
bool Refit(float& x0, float& y0, float& x1, float& y1, float mw, float mh) {
    float w = x0 + x1, h = y0 + y1, dw = x1 - x0, dh = y1 - y0;
    if (std::min(x0, y0) != 0 || dw <= 0 || dh <= 0 || std::fabs(mw * dh / (mh * dw) - 4.0f / 3.0f) > 0.02f) return false;
    float fit = mw / mh * app::Narrow() * h;
    dw = std::min(w, fit);
    dh = dw / fit * h;
    x0 = (w - dw) / 2, x1 = w - x0, y0 = (h - dh) / 2, y1 = h - y0;
    return true;
}
}  // namespace

// Render interface screen quad setup (sub_002E3011, thiscall(ri, x0, y0, x1, y1, u0, v0, u1, v1), ret 0x24): corners
// in viewport pixels (ri +0x4B84 width), turned into clip space for a textured quad, which the canvas flush never
// sees. Bink movies inside a Magma placeholder (training videos, PH_Video) come through it and are narrowed into the
// centred 4:3 like the page around it. A quad the full viewport wide (full screen movies, post effects) is left as
// it is, and so is the loading bar over a loading picture, which is pillarboxed with the frame. The ESRB notice keeps
// its 4:3 proportions: the spare width more on each side (clamped, so the edge columns reach the screen sides).
// The movie fits read ri +0x4D04, widescreen, set once at boot: kept to the output here.
X86_EXTERN_FUNC(__imp__sub_002E3011);
X86_FUNC(sub_002E3011) {
    uint32_t ri = R32(ctx.esp + 4);
    if (ri) G<uint8_t>(ri + 0x4D04) = app::Widescreen();
    if (ri && app::Widescreen()) {
        float w = float(int(R32(ri + 0x4B84))), k = app::Narrow();
        float& x0 = G<float>(ctx.esp + 8);
        float& x1 = G<float>(ctx.esp + 0x10);
        float& u0 = G<float>(ctx.esp + 0x18);
        float& u1 = G<float>(ctx.esp + 0x20);
        if (g_esrb) {
            float d = (u1 - u0) * (1 - k) / (2 * k);
            u0 -= d;
            u1 += d;
        } else if (game::loading::g_movie && !u0 && !GF(ctx.esp + 0x1C) &&
                   Refit(x0, G<float>(ctx.esp + 0xC), x1, G<float>(ctx.esp + 0x14), u1 + 1, GF(ctx.esp + 0x24) + 1)) {
        } else if (!g_loading && w > 0 && (std::min(x0, x1) > kSlack || std::max(x0, x1) < w - kSlack)) {
            x0 = x0 * k + kCentre * (1 - k) * w;
            x1 = x1 * k + kCentre * (1 - k) * w;
        }
    }
    __imp__sub_002E3011(ctx, base);
}

// Magma page draw (thiscall, no arguments); page +0x14 id, +0x1C layer. 100 is the default layer, the HUD's in a
// level but also the menus' lists, so the hide HUD key only acts while P_hud is up
X86_EXTERN_FUNC(__imp__sub_002BE990);
X86_FUNC(sub_002BE990) {
    uint32_t id = R32(ctx.ecx + 0x14);
    g_hud_seen |= id == Crc32("P_hud");
    g_loading = false;
    if (app::g_hide_hud && g_in_level && R32(ctx.ecx + 0x1C) == 100) {
        ctx.esp += 4;
        return;
    }
    const Page* saved = g_page;
    uint32_t drawing = std::exchange(g_drawing, ctx.ecx);
    g_page = nullptr;
    for (auto& p : kPages)
        if (p.id == id) g_page = &p;
    Draw(ctx, base, g_page ? Mode::Centre : Mode::Fill, __imp__sub_002BE990);
    g_page = saved;
    g_drawing = drawing;
    if (id == Crc32("P_OnlineOffline")) Draw(ctx, base, Mode::Right, [](X86Context& c, uint8_t*) { app::DrawWatermark(c); });
}

namespace game {
uint32_t DrawingPage() { return g_drawing; }
}

// Magma element draw, children come back through here: the first listed or placed element decides. On menu
// pages only the listed elements are placed, the rest stay with the page
X86_EXTERN_FUNC(__imp__sub_002C64E0);
X86_FUNC(sub_002C64E0) {
    uint32_t instance = ctx.ecx;
    if (game::OptionsElement(ctx, base, instance)) return;
    if (game::WordingElement(ctx, base, instance)) return;
    if (game::HintElement(ctx, base, instance)) return;
    // the game sets the cinematic's prompts every frame: relabelled for the keyboard just before they are drawn
    if (R32(instance + 4) == kEventBar && (G<uint8_t>(instance + 0x18) & 1)) {
        game::EventPrompts(ctx, instance);
        ctx.ecx = instance;
    }
    if (g_decided && game::ComboElement(ctx, base, instance)) return;
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
    if (app::Get().fps_counter) Draw(ctx, base, Mode::Left, [](X86Context& c, uint8_t*) { app::DrawFps(c); });
    g_offset = g_hud_seen ? kNone : kCentre;
    g_in_level = g_hud_seen;
    g_hud_seen = false;
}

// ShowScreen's fade draw (thiscall(alpha, blend, r, g, b, 1), ret 0x18): one quad, stage 0 the picture, stage 1 the
// frame on screen (ri +0x5C64, the front buffer's memory, read back once a fade frame is drawn over it). The quad
// (+4: c67 position, c68 its scale, c70 the uv offset both stages share, c71 / c72 each stage's uv scale) gives both
// one shape, so a 4:3 picture squeezed a full width frame, and a full width one stretched a 4:3 frame. Such a fade
// is shown full width, drawn as two halves from the centre, where both u meet: each layer keeps its own width.
X86_EXTERN_FUNC(__imp__sub_002E5F1F);
X86_FUNC(sub_002E5F1F) {
    uint32_t quad = ctx.ecx, ri = R32(quad), old = R32(ri + 0x5C64);
    float* f = &G<float>(quad + 4);
    float alpha = GF(ctx.esp + 4);
    if (!g_fade || !old || f[0] != -1 || f[4] != 2) return __imp__sub_002E5F1F(ctx, base);
    gpu::g_splash = alpha == 1 && g_new_43;
    if (alpha == 1) return __imp__sub_002E5F1F(ctx, base);
    float saved[24];
    std::memcpy(saved, f, sizeof saved);
    float half = float(int(R32(ri + 0x4B84))) / 2, wide = half / app::Narrow();
    float now = g_new_43 || g_esrb ? wide : half, was = g_old_43 ? wide : half;
    if (R32(old + 4) == R32(R32(ri + 0x4B74) + 4)) g_old_43 = false;
    f[0] = 0;
    f[12] = half;
    for (float side : {1.0f, -1.0f}) {
        f[4] = side;
        f[16] = side * now;
        f[20] = side * was;
        CallGuest(ctx, 0x2E4D05, {0x2E, R32(ctx.esp + 4), R32(ctx.esp + 0xC), R32(ctx.esp + 0x10), R32(ctx.esp + 0x14)});
    }
    std::memcpy(f, saved, sizeof saved);
    ctx.esp += 4 + 0x18;
}

// render device ShowScreen(path, persist, fade, clear) (thiscall on the render device): reads the picture's index
// bytes into the device's cached back buffer (+0x4B74), then fades to it from the front buffer through Swaps of its
// own. The engine refreshes that cache only after its own Swap, so after an earlier ShowScreen it can be the front
// buffer, and the index bytes land in the picture being faded from. It is pointed at the current back buffer first.
// Without a picture the frame on screen stays what it was (faded out, or put in both buffers), 4:3 if it was.
X86_EXTERN_FUNC(__imp__sub_002E5F95);
X86_FUNC(sub_002E5F95) {
    game::loading::BeforeShowScreen(ctx);
    std::string path;
    uint32_t ri = ctx.ecx, p = R32(ctx.esp + 4), step = p && !G<uint8_t>(p + 1) ? 2 : 1;  // TCHAR or char
    for (uint32_t i = 0; p && i < 256 && G<uint8_t>(p + i * step); i++) path += char(std::tolower(G<uint8_t>(p + i * step)));
    uint32_t cached = R32(ri + 0x4B74), back = CallGuest(ctx, SYM(D3DDevice_GetBackBuffer2), {0});
    if (cached) W32(cached, R32(cached) - 1);  // the cache's reference moves to the back buffer
    W32(ri + 0x4B74, back);
    if (!p && !SavedDataAddress() && !G<uint8_t>(ctx.esp + 0xC)) return game::loading::BothBuffers(ctx, ri);
    g_esrb = path.find("esrb") != std::string::npos;
    g_loading |= p != 0 || SavedDataAddress();  // no picture: it fades from the display kept through a reboot
    bool splash = gpu::g_splash;
    // a picture is shown 4:3 by Present, a fade to black as the frame on screen was, anything else as it was drawn
    g_old_43 = gpu::g_shown_splash;
    g_new_43 = p ? !g_esrb : G<uint8_t>(ctx.esp + 0x10) ? g_old_43 : splash;
    gpu::g_splash = g_new_43;
    g_fade = G<uint8_t>(ctx.esp + 0xC) && !SavedDataAddress() && app::Widescreen() && (g_esrb || g_new_43 != g_old_43);
    gpu::g_in_splash++;
    __imp__sub_002E5F95(ctx, base);
    gpu::g_in_splash--;
    g_fade = false;
    if (!(ctx.eax & 0xFF)) gpu::g_splash = splash;  // picture not found, nothing drawn
    g_esrb = false;
    game::loading::AfterShowScreen(ctx);
}
