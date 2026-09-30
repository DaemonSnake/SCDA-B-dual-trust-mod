// Versus widescreen. The 3D view widens instead of stretching. Magma 2D keeps its 4:3 shapes: menu pages are centred
// with images touching a page side pulled out to the screen side, HUD page instances go to the side they sit on.
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <string_view>
#include <vector>

#include "app/app.h"
#include "core/log.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// Magma names pages by crc32 of the lowercase name
constexpr uint32_t Crc32(std::string_view text) {
    uint32_t crc = ~0u;
    for (char c : text) {
        crc ^= uint8_t(c);
        for (int i = 0; i < 8; i++) crc = crc >> 1 ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

constexpr uint32_t kHudPages[] = {Crc32("p_hudatt"), Crc32("p_interatt")};
constexpr uint32_t kMagma = 0x3D8D74;  // manager: +0x38 viewport (+0x9C width in pixels)
constexpr uint32_t kAddQuad = 0x1231E0;
constexpr float kSlack = 2.0f, kMinWidth = 8.0f;  // pixels

enum class Mode { Stretch, Left, Centre, Right, Fill };

struct Quad {
    uint32_t util;
    float corners[12];
    uint32_t args[12];
};

bool g_hud = false;      // a HUD page is being drawn
bool g_capture = false;  // a HUD instance is being drawn: its quads wait for its bounds
bool g_replay = false;
Mode g_mode = Mode::Centre;
std::vector<Quad> g_quads;

float Width() {
    uint32_t m = R32(kMagma), vp = m ? R32(m + 0x38) : 0;
    return vp ? float(int(R32(vp + 0x9C))) : 0.0f;
}

// corners: 4 x {x, y, z}
void Place(float* c, Mode mode, float w) {
    float lo = std::min({c[0], c[3], c[6], c[9]}), hi = std::max({c[0], c[3], c[6], c[9]});
    bool wide = hi - lo >= kMinWidth;
    for (int i = 0; i < 12; i += 3) {
        float& x = c[i];
        if (mode == Mode::Stretch) continue;
        if (mode == Mode::Fill && wide && x <= kSlack) x = -1.0f;
        else if (mode == Mode::Fill && wide && x >= w - kSlack) x = w + 1.0f;
        else x = x * 0.75f + w * (mode == Mode::Left ? 0.0f : mode == Mode::Right ? 0.25f : 0.125f);
    }
}

}  // namespace

// FPerspectiveMatrix(fov x, fov y, x scale, y scale, near, far), thiscall ret 0x18: M[0][0] = x scale / tan(fov x).
// The camera passes its viewport's 640 / 480 as y scale; a 16:9 frame takes 3/4 of the x scale.
X86_EXTERN_FUNC(__imp__sub_0011B720);
X86_FUNC(sub_0011B720) {
    float& x = G<float>(ctx.esp + 0xC);
    if (app::Widescreen() && std::fabs(G<float>(ctx.esp + 0x10) - 4.0f / 3.0f) < 0.01f) x *= 0.75f;
    __imp__sub_0011B720(ctx, base);
}

// Magma page draw (thiscall, +4 id), also the vtable entry the frame calls for every page
X86_EXTERN_FUNC(__imp__sub_0025A3F0);
X86_FUNC(sub_0025A3F0) {
    uint32_t id = R32(ctx.ecx + 4);
    bool saved = g_hud;
    g_hud = std::find(std::begin(kHudPages), std::end(kHudPages), id) != std::end(kHudPages);
    static std::vector<uint32_t> seen;
    if (std::find(seen.begin(), seen.end(), id) == seen.end() && seen.size() < 64) {
        seen.push_back(id);
        LOGI("versus widescreen: page %08X %s", id, g_hud ? "hud" : "menu");
    }
    __imp__sub_0025A3F0(ctx, base);
    g_hud = saved;
}

// Magma instance draw (thiscall, +8 id). On a HUD page each instance is drawn into a list, then placed by the
// half of the screen it covers and replayed; children come back through here inside their parent's list.
X86_EXTERN_FUNC(__imp__sub_002620A0);
X86_FUNC(sub_002620A0) {
    if (!g_hud || g_capture || !app::Widescreen()) return __imp__sub_002620A0(ctx, base);
    uint32_t id = R32(ctx.ecx + 8);
    g_capture = true;
    __imp__sub_002620A0(ctx, base);
    g_capture = false;
    std::vector<Quad> quads;
    quads.swap(g_quads);
    float w = Width();
    if (quads.empty() || w <= 0.0f) return;
    float lo = w, hi = 0.0f;
    for (auto& q : quads)
        for (int i = 0; i < 12; i += 3) lo = std::min(lo, q.corners[i]), hi = std::max(hi, q.corners[i]);
    Mode mode = lo <= kSlack && hi >= w - kSlack ? Mode::Stretch
                : hi <= w / 2 + kSlack           ? Mode::Left
                : lo >= w / 2 - kSlack           ? Mode::Right
                                                 : Mode::Centre;
    LOG_FIRST(100, "versus widescreen: instance %08X x %.0f..%.0f mode %d", id, lo, hi, int(mode));
    uint32_t esp = ctx.esp, corners = (esp - 0x40) & ~0xFu;
    ctx.esp = corners;
    g_replay = true;
    g_mode = mode;
    for (auto& q : quads) {
        std::memcpy(&G<float>(corners), q.corners, sizeof q.corners);
        ctx.ecx = q.util;
        const uint32_t* a = q.args;
        CallGuest(ctx, kAddQuad, {corners, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11]});
    }
    g_replay = false;
    ctx.esp = esp;
}

// canvas quad add for Magma text and images (thiscall(util, corners, 12 more), ret 0x34), in viewport pixels
X86_EXTERN_FUNC(__imp__sub_001231E0);
X86_FUNC(sub_001231E0) {
    float w = Width();
    if (!app::Widescreen() || w <= 0.0f) return __imp__sub_001231E0(ctx, base);
    uint32_t corners = R32(ctx.esp + 4);
    float c[12];
    std::memcpy(c, &G<float>(corners), sizeof c);
    if (g_capture) {
        Quad q{ctx.ecx};
        std::memcpy(q.corners, c, sizeof c);
        for (int i = 0; i < 12; i++) q.args[i] = R32(ctx.esp + 8 + i * 4);
        g_quads.push_back(q);
        ctx.esp += 0x38;
        return;
    }
    float placed[12];
    std::memcpy(placed, c, sizeof c);
    Place(placed, g_replay ? g_mode : Mode::Fill, w);
    std::memcpy(&G<float>(corners), placed, sizeof placed);
    __imp__sub_001231E0(ctx, base);
    std::memcpy(&G<float>(corners), c, sizeof c);
}
