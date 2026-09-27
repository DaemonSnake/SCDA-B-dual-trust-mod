// The mouse on the game side, after UXboxViewport::UpdateInput on the game thread.
// Play: raw counts go to UEngine::InputEvent as IK_MouseX and IK_MouseY axis events, as the PC viewport sends
// them; the game's ini binds them to aMouseX and aMouseY.
// Menus: the Magma page on top of the input stack. The cursor focuses the widget under it the way the neighbour
// navigation does and in a list selects the item under it; a click presses A, or scrolls on a list's arrows.
#include <algorithm>

#include "core/input.h"
#include "game/console.h"
#include "game/magma.h"
#include "xdk/hle.h"

using namespace kernel;

namespace game {
void UpdateView(uint32_t viewport);               // views.cpp
}

namespace {

constexpr uint32_t kMouseX = 0xE4, kMouseY = 0xE5, kJoyA = 0xCA, kAnalogUp = 0xC4, kAnalogDown = 0xC5;
constexpr uint32_t kPress = 1, kRelease = 3, kAxis = 4;

// manager: +0x3C contexts, +0x40 count; context: +0x14 draw stack, +0x18 input stack;
// stack: +4 begin, +8 end of {page, flags}; screen: +0x84 width, +0x86 height
constexpr uint32_t kMagma = 0x6A6638, kMagmaScreen = 0x6A6668;
// page: +8 instances begin, +0xC end, +0x14 id, +0x1C layer (100 HUD, the base default), +0x24 focused; instance: +8 widget, +0x4C flags (2 enabled, 8 focused),
// vtable +0xC Focus, +0x10 Unfocus; widget: vtable +0xC type, +0x10 GetBounds(short rect[4] left right top bottom)
// list: +0x3C up arrow, +0x50 down arrow, +0x7C up arrow rect, +0x8C down arrow rect, +0x9C selection
constexpr uint32_t kListType = 10;
constexpr uint32_t kHudLayer = 100;
constexpr uint32_t kListHitTest = 0x2C1190, kListSelect = 0x2C2040, kPageSetFocus = 0x2C5370;

struct Game {
    X86Context& ctx;
    uint32_t engine, viewport, scratch;  // scratch: point at +0, rect at +8
};

uint32_t Call(X86Context& ctx, uint32_t fn, uint32_t self, std::initializer_list<uint32_t> args) {
    ctx.ecx = self;
    return CallGuest(ctx, fn, args);
}

uint32_t Virtual(X86Context& ctx, uint32_t self, uint32_t offset, std::initializer_list<uint32_t> args = {}) {
    return Call(ctx, R32(R32(self) + offset), self, args);
}

void Event(Game& g, uint32_t key, uint32_t action, float delta) {
    uint32_t d;
    std::memcpy(&d, &delta, 4);
    Call(g.ctx, R32(R32(g.engine) + 0x78), g.engine, {g.viewport, key, action, d});
}

void Tap(Game& g, uint32_t key) {
    Event(g, key, kPress, 1.0f);
    Event(g, key, kRelease, 1.0f);
}

uint32_t InputPage() {
    uint32_t m = R32(kMagma);
    uint32_t list = m ? R32(m + 0x3C) : 0, count = m ? R32(m + 0x40) : 0;
    if (!list || count > 16) return 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t context = R32(list + i * 4);
        uint32_t stack = context ? R32(context + 0x18) : 0;
        uint32_t begin = stack ? R32(stack + 4) : 0, end = stack ? R32(stack + 8) : 0;
        if (begin && end > begin && end - begin <= 8 * 64) return R32(end - 8);
    }
    return 0;
}

bool Focusable(Game& g, uint32_t instance) {
    if (!(G<uint8_t>(instance + 0x4C) & 2) || !R32(instance + 8)) return false;
    switch (Virtual(g.ctx, R32(instance + 8), 0xC)) {
        case 2: case 3: case 4: case 9: case 10: case 12: return true;
        default: return false;
    }
}

bool Inside(Game& g, uint32_t rect) {
    int16_t x = int16_t(R16(g.scratch)), y = int16_t(R16(g.scratch + 2));
    int16_t l = int16_t(R16(rect)), r = int16_t(R16(rect + 2)), t = int16_t(R16(rect + 4)), b = int16_t(R16(rect + 6));
    return l < r && t < b && x >= l && x <= r && y >= t && y <= b;
}

uint32_t g_page = 0;
uint32_t g_clicks = 0;
uint32_t g_point = 0xFFFFFFFF;

// window cursor to page space: the frame is drawn 16:9 in the middle of the window, menu pages 4:3 in its middle
bool Point(uint32_t& point) {
    int x, y, w, h;
    if (!input::Cursor(x, y, w, h) || w <= 0 || h <= 0) return false;
    uint32_t screen = R32(kMagmaScreen);
    float sw = screen ? float(int16_t(R16(screen + 0x84))) : 0, sh = screen ? float(int16_t(R16(screen + 0x86))) : 0;
    if (sw <= 0 || sh <= 0) sw = 640, sh = 480;
    float vw = std::min(float(w), h * 16.0f / 9.0f), vh = vw * 9.0f / 16.0f;
    float fx = ((x - (w - vw) / 2) / vw - 0.125f) / 0.75f;
    int px = int(fx * sw), py = int((y - (h - vh) / 2) / vh * sh);
    point = uint32_t(uint16_t(int16_t(px))) | uint32_t(uint16_t(int16_t(py))) << 16;
    LOG_FIRST(1, "mouse: menu space %gx%g", sw, sh);
    return true;
}

// true while a menu takes the input
bool Menu(Game& g) {
    uint32_t top = InputPage();
    // the HUD stays on the input stack through play with nothing focused, its widgets are not a menu
    uint32_t layer = top ? R32(top + 0x1C) : 0, focus = top ? R32(top + 0x24) : 0;
    uint32_t page = layer == kHudLayer || !focus ? 0 : top;
    uint32_t begin = page ? R32(page + 8) : 0, end = page ? R32(page + 0xC) : 0;
    if (end < begin || end - begin > 4 * 512) begin = end = 0;
    uint32_t focusable = 0, hovered = 0, focused = 0;
    uint32_t point = g_point, clicks = input::Clicks();
    // a new page starts from where the cursor rests, only movement or a click after it acts
    bool fresh = top != g_page;
    bool moved = Point(point) && point != g_point && !fresh, clicked = clicks != g_clicks && !fresh;
    g_point = point;
    g_clicks = clicks;
    W32(g.scratch, point);
    for (uint32_t it = begin; it < end; it += 4) {
        uint32_t instance = R32(it);
        if (!instance || !Focusable(g, instance)) continue;
        focusable++;
        if (G<uint8_t>(instance + 0x4C) & 8) focused = instance;
        if (!moved && !clicked) continue;
        Virtual(g.ctx, R32(instance + 8), 0x10, {g.scratch + 8});
        if (Inside(g, g.scratch + 8) && (!hovered || (G<uint8_t>(instance + 0x4C) & 8))) hovered = instance;
    }
    if (fresh) {
        g_page = top;
        LOGI("mouse: input page %08X id %08X layer %u focus %08X, %u focusable, %s", top, top ? R32(top + 0x14) : 0,
             layer, focus, focusable, page ? "menu" : "not a menu");
    }
    if (!focusable) return false;
    if (!hovered) return true;
    if (hovered != focused) Call(g.ctx, kPageSetFocus, page, {hovered});
    uint32_t widget = R32(hovered + 8);
    if (Virtual(g.ctx, widget, 0xC) != kListType) {
        if (clicked) Tap(g, kJoyA);
        return true;
    }
    int16_t hit = int16_t(Call(g.ctx, kListHitTest, widget, {g.scratch}));
    if (hit >= 0 && hit != int16_t(R16(widget + 0x9C))) {
        Call(g.ctx, kListSelect, widget, {uint32_t(hit), 0, 1});
        Virtual(g.ctx, hovered, 0xC);
    }
    if (!clicked) return true;
    if (R32(widget + 0x3C) && Inside(g, widget + 0x7C)) Tap(g, kAnalogUp);
    else if (R32(widget + 0x50) && Inside(g, widget + 0x8C)) Tap(g, kAnalogDown);
    else if (hit >= 0) Tap(g, kJoyA);
    return true;
}

}  // namespace

// viewport: +0x18 client; client: +0x2C engine, +0x30 viewports; engine vtable +0x78 InputEvent(viewport, key, action, delta)
X86_EXTERN_FUNC(__imp__sub_0004B525);
X86_FUNC(sub_0004B525) {
    uint32_t viewport = ctx.ecx;
    __imp__sub_0004B525(ctx, base);
    uint32_t client = R32(viewport + 0x18);
    uint32_t engine = client ? R32(client + 0x2C) : 0;
    if (!engine || R32(R32(client + 0x30)) != viewport) return;
    // guest calls go below a scratch block under the caller's stack
    uint32_t esp = ctx.esp;
    Game g{ctx, engine, viewport, (esp - 0x40) & ~0xFu};
    ctx.esp = g.scratch - 0x10;
    bool menu = Menu(g);
    input::SetMenu(menu);
    uint32_t top = InputPage();
    input::SetTitleScreen(top && R32(top + 0x14) == game::magma::Id("P_Start"));
    game::UpdateView(viewport);
    float dx, dy;
    // the axis command stores its value, so each axis gets one summed event per update
    if (!menu && input::TakeMouse(dx, dy)) {
        if (dx != 0.0f) Event(g, kMouseX, kAxis, dx);
        if (dy != 0.0f) Event(g, kMouseY, kAxis, dy);
    }
    game::RunCommands(ctx, engine, viewport);
    ctx.esp = esp;
}
