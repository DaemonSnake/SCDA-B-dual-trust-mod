// Button legends on keyboard and mouse: the icon goes and the text leads with the key, "(ENTER) Select". Menus take
// Enter and Escape as A and B, play the bound keys. A change of device sets the legends on screen again. The in-game
// cinematic's prompts (Skip) are no legend: the game sets them every frame, so they are relabelled as they are
// drawn, "Skip (ESC)".
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>

#include "core/input.h"
#include "game/magma.h"
#include "xdk/hle.h"

using namespace kernel;

namespace game {
bool OnStack(uint32_t page);  // options_menu.cpp
}

namespace {

// SetNavigation (thiscall owner, page, const FString& "A,Common_A,B,Common_B", type 0 panel, 1 panel_right, 2 panel_bar)
constexpr uint32_t kSetNavigation = 0x2915C1;
constexpr uint32_t kFind = 0x17D0E8, kSetText = 0x183598;
constexpr uint32_t kPanels[] = {0x54362C, 0x543620, 0x565928};
constexpr uint32_t kBarButtons[] = {0x56591C, 0x565910, 0x565904, 0x5658F8};  // A_Button_1..4
constexpr uint32_t kBarText = 0x5658DC, kBarImage = 0x5658D4;
// element: +0x1C children, text +0x2C std::wstring (+4 buffer or pointer, +0x14 length, +0x18 capacity), image +0x3C material (+4 crc32 of its name); page +0x14 id, +0x1C layer
constexpr uint32_t kHudLayer = 100;
constexpr uint32_t kFindId = 0x2BE900;  // thiscall(page or children, id): the child instance
constexpr uint32_t kHud = game::magma::Id("P_hud");
// P_hud's scripted event bar's prompts, names not recovered: text and icon, the text right aligned against the icon
// on its right, or (before) the icon on the text's left
struct Prompt {
    uint32_t text, image;
    bool before;
};
constexpr Prompt kEventPrompts[] = {{0xDBC4BB81, 0x0E3CCC34, false}, {0x35CADAAD, 0xE032AD18, false},
                                    {0x42CDEA3B, 0x97359D8E, true}};

enum Code { A, B, X, Y, LT, RT, White, Black, Start, Back, Up, Down, Left, Right, Move, LeftClick, RightClick };
struct Icon {
    uint32_t id;
    Code code;
    constexpr Icon(std::string_view name, Code c) : id(game::magma::Id(name)), code(c) {}
};
// other languages name the sticks and triggers by their own left (G, I, S) and right (D)
constexpr Icon kIcons[] = {
    {"Button_A", A},         {"Button_B", B},           {"Button_X", X},           {"Button_Y", Y},
    {"Button_LT", LT},       {"Button_GT", LT},         {"Button_IT", LT},         {"Button_ST", LT},
    {"Button_RT", RT},       {"Button_DT", RT},         {"Button_white", White},   {"Button_black", Black},
    {"Button_start", Start}, {"button_start", Start},   {"Button_Start", Start},   {"Button_back", Back},
    {"button_back", Back},   {"Button_Back", Back},     {"Button_DU", Up},         {"Button_DD", Down},
    {"Button_DL", Left},     {"Button_DR", Right},      {"Button_AL_up", Move},    {"button_al_up", Move},
    {"Button_AG_up", Move},  {"Button_AI_up", Move},    {"Button_AS_up", Move},    {"Button_AL_clic", LeftClick},
    {"Button_AG_clic", LeftClick}, {"Button_AI_clic", LeftClick}, {"Button_AS_clic", LeftClick},
    {"Button_AR_clic", RightClick}, {"Button_AD_clic", RightClick},
};

uint32_t g_block = 0;  // +0 FString, +0x10 text, +0x600 slot names

std::string Keys(std::initializer_list<const char*> ids) {
    size_t count = 0;
    const input::Action* actions = input::Actions(&count);
    std::string out;
    bool single = true;
    std::string keys[4];
    size_t n = 0;
    for (const char* id : ids)
        for (size_t i = 0; i < count; i++)
            if (std::string_view(actions[i].id) == id) {
                std::string k = input::Binding(i);
                k = k.substr(0, k.find(','));
                if (k == "Escape") k = "Esc";
                for (char& c : k) c = char(std::toupper(uint8_t(c)));
                if (k.empty()) return {};
                single &= k.size() == 1;
                keys[n++] = k;
            }
    for (size_t i = 0; i < n; i++) out += (i && !single ? "/" : "") + keys[i];
    return out;
}

std::string Label(uint32_t material_id, bool menu, uint32_t page_id) {
    const Icon* icon = std::find_if(std::begin(kIcons), std::end(kIcons), [&](const Icon& i) { return i.id == material_id; });
    if (icon == std::end(kIcons)) return {};
    switch (icon->code) {
        case A: return menu ? "ENTER" : Keys({"interact"});
        case B: return !menu ? Keys({"crouch"}) : page_id == game::magma::Id("P_Pause") ? "BACKSPACE" : "ESC";
        case X: return Keys({"scope"});
        case Y: return Keys({"jump"});
        case LT: return Keys({"alt_fire"});
        case RT: return Keys({"fire"});
        case White: return Keys({"quick_inventory"});
        case Black: return Keys({"sidekick"});
        case Start: return page_id == game::magma::Id("P_Start") ? "ENTER" : Keys({"pause"});
        case Back: return Keys({"opsat"});
        case Up: return menu ? "UP" : Keys({"goggles"});
        case Down: return menu ? "DOWN" : Keys({"switch_camera"});
        case Left: return menu ? "LEFT" : Keys({"previous_vision"});
        case Right: return menu ? "RIGHT" : Keys({"next_vision"});
        case Move: return Keys({"move_forward", "strafe_left", "move_back", "strafe_right"});
        case LeftClick: return Keys({"back_to_wall"});
        case RightClick: return Keys({"zoom"});
    }
    return {};
}

bool Block() {
    if (!g_block) g_block = mem::AllocSystem(0x800);
    return g_block != 0;
}

std::u16string ReadText(uint32_t text) {
    uint32_t string = text + 0x2C, length = std::min<uint32_t>(R32(string + 0x14), 0x200);
    uint32_t data = R32(string + 0x18) >= 8 ? R32(string + 4) : string + 4;
    std::u16string out;
    for (uint32_t i = 0; data && i < length; i++) out += char16_t(R16(data + i * 2));
    return out;
}

void WriteText(X86Context& ctx, uint32_t text, const std::u16string& line) {
    size_t n = std::min<size_t>(line.size(), 0x2F0);
    for (size_t i = 0; i < n; i++) W16(g_block + 0x10 + uint32_t(i) * 2, uint16_t(line[i]));
    W16(g_block + 0x10 + uint32_t(n) * 2, 0);
    ctx.ecx = text + 0x2C;
    CallGuest(ctx, kSetText, {g_block + 0x10});
}

uint32_t Ansi(uint32_t at, std::string_view text) {
    for (size_t i = 0; i < text.size(); i++) G<char>(at + uint32_t(i)) = text[i];
    G<char>(at + uint32_t(text.size())) = 0;
    return at;
}

// false past the last slot
bool Slot(X86Context& ctx, uint32_t box, uint32_t text_name, uint32_t image_name, bool menu, uint32_t page_id) {
    uint32_t ti = CallGuest(ctx, kFind, {box, text_name}), ii = CallGuest(ctx, kFind, {box, image_name});
    if (!ii) return false;
    uint32_t text = ti ? R32(ti + 8) : 0, image = R32(ii + 8);
    uint32_t material = image ? R32(image + 0x3C) : 0;
    if (!text || !material) return true;
    std::string key = Label(R32(material + 4), menu, page_id);
    if (key.empty()) return true;
    std::u16string line = u"(";
    for (char c : key) line += char16_t(uint8_t(c));
    line += u") ";
    uint32_t string = text + 0x2C, length = std::min<uint32_t>(R32(string + 0x14), 0x200);
    uint32_t data = R32(string + 0x18) >= 8 ? R32(string + 4) : string + 4;
    for (uint32_t i = 0; data && i < length; i++) line += char16_t(R16(data + i * 2));
    size_t n = std::min<size_t>(line.size(), 0x2F0);
    for (size_t i = 0; i < n; i++) W16(g_block + 0x10 + uint32_t(i) * 2, uint16_t(line[i]));
    W16(g_block + 0x10 + uint32_t(n) * 2, 0);
    W32(image + 0x3C, 0);
    ctx.ecx = text + 0x2C;
    CallGuest(ctx, kSetText, {g_block + 0x10});
    return true;
}

void Relabel(X86Context& ctx, uint32_t page, uint32_t type) {
    if (type > 2 || !Block()) return;
    uint32_t instance = CallGuest(ctx, kFind, {page, kPanels[type]});
    uint32_t panel = instance ? R32(instance + 8) : 0;
    uint32_t box = panel ? R32(panel + 0x1C) : 0;
    if (!box) return;
    bool menu = R32(page + 0x1C) != kHudLayer;
    uint32_t page_id = R32(page + 0x14);
    if (type == 2) {
        for (uint32_t name : kBarButtons) {
            uint32_t button = CallGuest(ctx, kFind, {box, name});
            uint32_t element = button ? R32(button + 8) : 0;
            if (uint32_t inner = element ? R32(element + 0x1C) : 0) Slot(ctx, inner, kBarText, kBarImage, menu, page_id);
        }
        return;
    }
    for (int i = 0; i < 16; i++) {
        uint32_t text = Ansi(g_block + 0x600, std::to_string(i) + "_Text");
        uint32_t image = Ansi(g_block + 0x620, std::to_string(i) + "_Image");
        if (!Slot(ctx, box, text, image, menu, page_id)) break;
    }
}

// the last legends set, set again when the device changes
struct Bar {
    uint32_t owner = 0, page = 0, type = 0;
    std::u16string actions;
};
Bar g_bars[8];  // game thread
size_t g_next = 0;
int g_keyboard = -1;

void Remember(uint32_t owner, uint32_t page, uint32_t fstring, uint32_t type) {
    uint32_t data = R32(fstring), chars = std::min<uint32_t>(R32(fstring + 4), 0x100);
    std::u16string actions;
    for (uint32_t i = 0; data && i + 1 < chars; i++) actions += char16_t(R16(data + i * 2));
    for (Bar& bar : g_bars)
        if (bar.page == page && bar.type == type) {
            bar = {owner, page, type, actions};
            return;
        }
    g_bars[g_next++ % std::size(g_bars)] = {owner, page, type, actions};
}

uint32_t Child(X86Context& ctx, uint32_t parent, uint32_t id) {
    ctx.ecx = parent;
    return CallGuest(ctx, kFindId, {id});
}

// what a relabelled prompt was: its icon and the game's text, and the text put in its place
struct Held {
    uint32_t image = 0, material = 0;
    std::u16string text, shown;
};
Held g_held[std::size(kEventPrompts)];  // game thread

}  // namespace

namespace game {

// hud.cpp, the bar's element draw
void EventPrompts(X86Context& ctx, uint32_t bar) {
    bool keyboard = input::KeyboardActive();
    uint32_t element = bar ? R32(bar + 8) : 0;
    uint32_t box = element ? R32(element + 0x1C) : 0;
    for (size_t i = 0; i < std::size(kEventPrompts); i++) {
        const Prompt& p = kEventPrompts[i];
        Held& h = g_held[i];
        uint32_t ti = box ? Child(ctx, box, p.text) : 0, ii = box ? Child(ctx, box, p.image) : 0;
        uint32_t text = ti ? R32(ti + 8) : 0, image = ii ? R32(ii + 8) : 0;
        if (!text || !image || !Block()) {
            h = {};
            continue;
        }
        if (image != h.image) h = {image};
        std::u16string now = ReadText(text);
        uint32_t material = R32(image + 0x3C);
        if (!keyboard) {
            // back on the pad: the icon and the game's text return
            if (!h.material) continue;
            if (!material) W32(image + 0x3C, h.material);
            if (now == h.shown) WriteText(ctx, text, h.text);
            h.material = 0;
            continue;
        }
        if (!material && (!h.material || now == h.shown)) continue;
        // a new icon or a new text from the game is labelled again
        uint32_t icon = material ? material : h.material;
        std::string key = Label(R32(icon + 4), false, kHud);
        if (key.empty()) continue;
        std::u16string label = u"(";
        for (char c : key) label += char16_t(uint8_t(c));
        label += u")";
        h.text = h.material && now == h.shown ? h.text : now;
        h.shown = p.before ? label + u" " + h.text : h.text + u" " + label;
        h.material = icon;
        W32(image + 0x3C, 0);
        WriteText(ctx, text, h.shown);
    }
}

// game thread, every input update
void PromptsTick(X86Context& ctx) {
    int keyboard = input::KeyboardActive();
    if (keyboard == g_keyboard) return;
    bool first = g_keyboard < 0;
    g_keyboard = keyboard;
    if (first || !Block()) return;
    for (const Bar& bar : g_bars) {
        if (!bar.page || !OnStack(bar.page)) continue;
        size_t n = std::min<size_t>(bar.actions.size(), 0x2F0);
        for (size_t i = 0; i < n; i++) W16(g_block + 0x10 + uint32_t(i) * 2, uint16_t(bar.actions[i]));
        W16(g_block + 0x10 + uint32_t(n) * 2, 0);
        W32(g_block, g_block + 0x10);
        W32(g_block + 4, uint32_t(n + 1));
        W32(g_block + 8, uint32_t(n + 1));
        ctx.ecx = bar.owner;
        CallGuest(ctx, kSetNavigation, {bar.page, g_block, bar.type});
    }
}

}  // namespace game

X86_EXTERN_FUNC(__imp__sub_002915C1);
X86_FUNC(sub_002915C1) {
    uint32_t owner = ctx.ecx, page = R32(ctx.esp + 4), actions = R32(ctx.esp + 8), type = R32(ctx.esp + 12);
    if (page && actions && actions != g_block) Remember(owner, page, actions, type);
    __imp__sub_002915C1(ctx, base);
    if (!page || !input::KeyboardActive()) return;
    uint32_t eax = ctx.eax;
    Relabel(ctx, page, type);
    ctx.eax = eax;
}
