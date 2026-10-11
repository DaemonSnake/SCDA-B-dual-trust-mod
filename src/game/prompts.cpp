// Button legends on keyboard and mouse: the icon goes and the text leads with the key, "(ENTER) Select". Menus take
// Enter and Escape as A and B, play the bound keys. A change of device sets the legends on screen again. The in-game
// cinematic's prompts (Skip) are no legend: the game sets them every frame, so they are relabelled as they are
// drawn, "Skip (ESC)". The combo escape's buttons (knock, grab) have no text at all: their key's name is drawn in
// each icon's place with the row name's text.
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "core/input.h"
#include "game/magma.h"
#include "xdk/hle.h"

using namespace kernel;

namespace game {
bool OnStack(uint32_t page);  // options_menu.cpp
uint32_t StackOwner(uint32_t page);  // options_menu.cpp
uint32_t DrawingPage();  // hud.cpp
uint32_t KeyboardPlayer();  // mouse.cpp
std::u16string KeyWording(std::u16string text);  // text.cpp
}

X86_EXTERN_FUNC(__imp__sub_002C64E0);  // Magma element draw (thiscall instance), hooked in hud.cpp
X86_EXTERN_FUNC(sub_002C64E0);

namespace {

// SetNavigation (thiscall owner, page, const FString& "A,Common_A,B,Common_B", type 0 panel, 1 panel_right, 2 panel_bar)
constexpr uint32_t kSetNavigation = 0x2915C1;
constexpr uint32_t kFind = 0x17D0E8, kSetText = 0x183598;
// the panels' names, by SetNavigation's type
constexpr uint32_t kPanels[] = {0x54362C, 0x543620, 0x565928};
constexpr uint32_t kBarButtons[] = {0x56591C, 0x565910, 0x565904, 0x5658F8};  // A_Button_1..4
constexpr uint32_t kBarText = 0x5658DC, kBarImage = 0x5658D4;
// element: +0x1C children, text +0x2C std::wstring (+4 buffer or pointer, +0x14 length, +0x18 capacity), image +0x3C material (+4 crc32 of its name); page +0x14 id, +0x1C layer
constexpr uint32_t kHudLayer = 100;
constexpr uint32_t kFindId = 0x2BE900;  // thiscall(page or children, id): the child instance
constexpr uint32_t kHud = game::magma::Id("P_hud");
// name entry (name_entry.cpp): the keys type, Backspace deletes (X); the switch to the digits (Y) goes
constexpr uint32_t kNameEntry = game::magma::Id("P_VirtualKeyboard");
// P_hud's scripted event bar's prompts, names not recovered: text and icon, the text right aligned against the icon
// on its right, or (before) the icon on the text's left
struct Prompt {
    uint32_t text, image;
    bool before;
};
constexpr Prompt kEventPrompts[] = {{0xDBC4BB81, 0x0E3CCC34, false}, {0x35CADAAD, 0xE032AD18, false},
                                    {0x42CDEA3B, 0x97359D8E, true}};
// combo escape rows (hud.mgm, names not recovered): the two rows, each of up to five button slots and four arrows
// placed left to right, and the row's name ("Knock", Title_18); a slot holds its button's icon
constexpr uint32_t kComboRows[] = {0x0CBEB816, 0x95B7E9AC};
constexpr uint32_t kComboArrows[] = {0x93BEFD95, 0x0AB7AC2F, 0x7DB09CB9, 0xE3D4091A};
constexpr uint32_t kComboIcon = 0xAAA53D32, kComboGlow = 0xEA7CB115, kComboName = 0xCF667FEC;
// widget: +0x10 left, +0x12 right, +0x14 top, +0x16 bottom (page units, in its area), an area instance's +0x10 x,
// +0x1C its children; vtable +8 Draw
constexpr uint32_t kRect = 0x10, kChildren = 0x1C, kDraw = 0x08;
constexpr uint32_t kTextWidget = 0x568290;  // vtable
// Magma screen: +0x7C the text given to the text being drawn in place of its own (a list row's)
constexpr uint32_t kScreen = 0x6A6668, kGivenText = 0x7C, kMaxWording = 0x300;
constexpr int kCharWidth = 10;  // Title_18 capitals, page units
// after a name wider than its icon, so the next arrow sits midway; names sit this much higher to line up with the arrows
constexpr int kNameGap = 12, kNameRaise = 4;

enum Code { A, B, X, Y, LT, RT, White, Black, Start, Back, Up, Down, Left, Right, Move, Look, LeftClick, RightClick };
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
    {"Button_AG_up", Move},  {"Button_AI_up", Move},    {"Button_AS_up", Move},    {"Button_AL", Move},
    {"button_al", Move},     {"button_ag", Move},       {"button_ai", Move},       {"button_as", Move},
    {"Button_AR", Look},     {"button_ar", Look},       {"button_ad", Look},       {"Button_AL_clic", LeftClick},
    {"Button_AG_clic", LeftClick}, {"Button_AI_clic", LeftClick}, {"Button_AS_clic", LeftClick},
    {"Button_AR_clic", RightClick}, {"Button_AD_clic", RightClick},
};

uint32_t g_block = 0;  // +0 FString, +0x10 text, +0x600 slot names

// keys of one letter run together ("WASD") unless split
std::string Keys(std::initializer_list<const char*> ids, bool split = false) {
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
    for (size_t i = 0; i < n; i++) out += (i && (split || !single) ? "/" : "") + keys[i];
    return out;
}

std::string Label(uint32_t material_id, bool menu, uint32_t page_id) {
    const Icon* icon = std::find_if(std::begin(kIcons), std::end(kIcons), [&](const Icon& i) { return i.id == material_id; });
    if (icon == std::end(kIcons)) return {};
    // the sticky camera's page: gas on fire, noise on alt fire, strafing switches cameras (input.cpp's camera remap),
    // forward and back zoom
    bool camera = page_id == game::magma::Id("P_Camera");
    // the OPSAT map: strafing steps zones (input.cpp's map remap), forward and back zoom
    bool map = page_id == game::magma::Id("P_Map");
    switch (icon->code) {
        case A: return menu ? "ENTER" : Keys({camera ? "fire" : "interact"});
        case B: return !menu ? Keys({"crouch"}) : page_id == game::magma::Id("P_Pause") ? "BACKSPACE" : "ESC";
        case X: return page_id == kNameEntry ? "BACKSPACE" : Keys({camera ? "alt_fire" : "scope"});
        case Y: return Keys({"jump"});
        case LT: return Keys({camera || map ? "strafe_left" : "alt_fire"});
        case RT: return Keys({camera || map ? "strafe_right" : "fire"});
        case White: return Keys({"quick_inventory"});
        case Black: return Keys({"sidekick"});
        case Start: return page_id == game::magma::Id("P_Start") ? "ENTER" : Keys({"pause"});
        case Back: return Keys({"opsat"});
        case Up: return menu ? "UP" : Keys({"goggles"});
        case Down: return menu ? "DOWN" : Keys({"switch_camera"});
        case Left: return menu ? "LEFT" : Keys({"previous_vision"});
        case Right: return menu ? "RIGHT" : Keys({"next_vision"});
        case Move:
            return camera || map ? Keys({"move_forward", "move_back"}, true)
                          : Keys({"move_forward", "strafe_left", "move_back", "strafe_right"});
        case Look: return "MOUSE";
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

// the actions without Y's pair, in the block's FString
uint32_t WithoutY(uint32_t fstring) {
    uint32_t data = R32(fstring), chars = std::min<uint32_t>(R32(fstring + 4), 0x100);
    std::u16string actions, out;
    for (uint32_t i = 0; data && i + 1 < chars; i++) actions += char16_t(R16(data + i * 2));
    for (size_t at = 0; at < actions.size();) {
        size_t comma = std::min(actions.find(u',', at), actions.size());
        size_t end = std::min(actions.find(u',', comma + 1), actions.size());
        if (actions.compare(at, comma - at, u"Y") != 0) out += (out.empty() ? u"" : u",") + actions.substr(at, end - at);
        at = end + 1;
    }
    size_t n = std::min<size_t>(out.size(), 0x2F0);
    for (size_t i = 0; i < n; i++) W16(g_block + 0x10 + uint32_t(i) * 2, uint16_t(out[i]));
    W16(g_block + 0x10 + uint32_t(n) * 2, 0);
    W32(g_block, g_block + 0x10);
    W32(g_block + 4, uint32_t(n + 1));
    W32(g_block + 8, uint32_t(n + 1));
    return g_block;
}

uint32_t Child(X86Context& ctx, uint32_t parent, uint32_t id) {
    ctx.ecx = parent;
    return CallGuest(ctx, kFindId, {id});
}

uint32_t g_combo_name = 0;  // the last combo row name drawn, its text widget (game thread)
// the row being drawn: how far right each slot and arrow goes, past the key names wider than their icon before it,
// and a wide name's glow (image: both its sides), to its middle
struct Shift {
    uint32_t instance;
    int dx;
    bool image;
};
Shift g_shifts[16];
size_t g_shift_count = 0;

std::string ComboKey(uint32_t icon) {
    uint32_t material = R32(icon + 0x3C);
    return material ? Label(R32(material + 4), false, kHud) : std::string();
}

void ComboLayout(X86Context& ctx, uint32_t row) {
    g_shift_count = 0;
    uint32_t box = R32(row + kChildren);
    uint32_t begin = box ? R32(box + 8) : 0, end = box ? R32(box + 0xC) : 0;
    if (!begin || end < begin || end - begin > 4 * 16) return;
    struct Item {
        uint32_t instance;
        int x, overflow;
    };
    Item items[16];
    size_t n = 0;
    for (uint32_t it = begin; it < end; it += 4) {
        uint32_t instance = R32(it), widget = instance ? R32(instance + 8) : 0;
        uint32_t id = instance ? R32(instance + 4) : 0;
        if (!widget || !(G<uint8_t>(instance + 0x18) & 1) || id == kComboName) continue;
        int overflow = 0;
        if (std::find(std::begin(kComboArrows), std::end(kComboArrows), id) == std::end(kComboArrows)) {
            uint32_t slot = R32(widget + kChildren);
            uint32_t icon_instance = slot ? Child(ctx, slot, kComboIcon) : 0;
            uint32_t icon = icon_instance ? R32(icon_instance + 8) : 0;
            if (icon) {
                int width = int16_t(R16(icon + kRect + 2)) - int16_t(R16(icon + kRect));
                int wide = int(ComboKey(icon).size()) * kCharWidth - width;
                uint32_t glow = wide > 0 ? Child(ctx, slot, kComboGlow) : 0;
                if (glow && g_shift_count < std::size(g_shifts)) g_shifts[g_shift_count++] = {glow, wide / 2, true};
                if (wide > 0) overflow = wide + kNameGap;
            }
        }
        items[n++] = {instance, int16_t(R16(widget + kRect)), overflow};
    }
    std::sort(items, items + n, [](const Item& a, const Item& b) { return a.x < b.x; });
    int dx = 0;
    for (size_t i = 0; i < n; i++) {
        if (dx && g_shift_count < std::size(g_shifts)) g_shifts[g_shift_count++] = {items[i].instance, dx, false};
        dx += items[i].overflow;
    }
}

// what a relabelled prompt was: its icon and the game's text, and the text put in its place
struct Held {
    uint32_t image = 0, material = 0;
    std::u16string text, shown;
};
Held g_held[std::size(kEventPrompts)];  // game thread

// a caption right aligned against its icon, some with more text after the icon: the message box and the mission's
// end "Press (A) To Continue", the training video's "Skip Video (start)" and its "Press (button)"
struct Hint {
    uint32_t text, image, after;
};
constexpr Hint kHints[] = {
    {game::magma::Id("L_Press"), game::magma::Id("I_Button"), game::magma::Id("L_Continue")},
    {game::magma::Id("text_skip"), game::magma::Id("bouton_skip"), 0},
    {game::magma::Id("text"), game::magma::Id("bouton"), 0},
};
// the icon as it last came to draw: its key and sides
struct Seen {
    uint32_t tick = 0;
    std::string key;
    int l = 0, r = 0;
};
Seen g_seen[std::size(kHints)];  // game thread
uint32_t g_tick = 3;  // past a hint never seen
bool g_hinting = false;

// keyboard legends while the keyboard is in use; in split screen only in the keyboard's player's menus and HUD
bool Keyboard(uint32_t page) {
    uint32_t player = game::KeyboardPlayer(), owner = player && page ? game::StackOwner(page) : 0;
    return input::KeyboardActive() && (!owner || owner == player);
}
constexpr int kTextChar2 = 15;           // Text_16, twice a character's width in page units
constexpr int kIconGap = 4, kWordGap = 7;  // a caption's end to its icon, between words

}  // namespace

namespace game {

// text.cpp
std::string KeyNames(std::initializer_list<const char*> ids) { return Keys(ids); }

// hud.cpp, the bar's element draw
void EventPrompts(X86Context& ctx, uint32_t bar) {
    bool keyboard = Keyboard(game::DrawingPage());
    uint32_t element = bar ? R32(bar + 8) : 0;
    uint32_t box = element ? R32(element + 0x1C) : 0;
    uint32_t skip = box ? Child(ctx, box, kEventPrompts[0].text) : 0;
    uint32_t skip_text = skip ? R32(skip + 8) : 0;
    bool skippable = skip_text && (G<uint8_t>(skip + 0x18) & 1) && !ReadText(skip_text).empty();
    if (skippable) input::SkipPromptShown();
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

// hud.cpp, an element about to draw inside a placed area. On the keyboard a combo icon draws without its image and
// its key's name goes in its place, drawn by the row name's widget moved into the icon's box; a name wider than the
// icon starts at its left and moves the rest of the row right: true when drawn here.
bool ComboElement(X86Context& ctx, uint8_t* base, uint32_t instance) {
    uint32_t id = R32(instance + 4), widget = R32(instance + 8);
    if (id == kComboName) g_combo_name = widget;
    if (std::find(std::begin(kComboRows), std::end(kComboRows), id) != std::end(kComboRows)) {
        g_shift_count = 0;
        if (widget && Keyboard(game::DrawingPage())) ComboLayout(ctx, widget);
        ctx.ecx = instance;  // the row still draws, and the calls above left their own this
        return false;
    }
    for (size_t i = 0; i < g_shift_count && widget; i++) {
        if (g_shifts[i].instance != instance) continue;
        uint16_t x = R16(widget + kRect), right = R16(widget + kRect + 2);
        W16(widget + kRect, uint16_t(int16_t(x) + g_shifts[i].dx));
        if (g_shifts[i].image) W16(widget + kRect + 2, uint16_t(int16_t(right) + g_shifts[i].dx));
        __imp__sub_002C64E0(ctx, base);
        W16(widget + kRect, x);
        W16(widget + kRect + 2, right);
        return true;
    }
    if (id != kComboIcon || !widget || !(G<uint8_t>(instance + 0x18) & 1) || !g_combo_name ||
        !Keyboard(game::DrawingPage()) || !Block())
        return false;
    uint32_t material = R32(widget + 0x3C);
    std::string key = ComboKey(widget);
    if (key.empty()) return false;
    W32(widget + 0x3C, 0);
    __imp__sub_002C64E0(ctx, base);
    W32(widget + 0x3C, material);
    uint32_t name = g_combo_name;
    uint32_t rect[2] = {R32(name + kRect), R32(name + kRect + 4)};
    int l = int16_t(R16(widget + kRect)), r = int16_t(R16(widget + kRect + 2));
    int t = int16_t(R16(widget + kRect + 4)), b = int16_t(R16(widget + kRect + 6));
    int w = int(key.size()) * kCharWidth, h = int16_t(R16(name + kRect + 6)) - int16_t(R16(name + kRect + 4));
    int x = w > r - l ? l : (l + r - w) / 2, y = (t + b - h) / 2 - kNameRaise;
    W16(name + kRect, uint16_t(int16_t(x)));
    W16(name + kRect + 2, uint16_t(int16_t(x + w)));
    W16(name + kRect + 4, uint16_t(int16_t(y)));
    W16(name + kRect + 6, uint16_t(int16_t(y + h)));
    std::u16string text = ReadText(name), line;
    for (char c : key) line += char16_t(uint8_t(c));
    WriteText(ctx, name, line);
    ctx.ecx = name;
    CallGuest(ctx, R32(R32(name) + kDraw), {});
    WriteText(ctx, name, text);
    W32(name + kRect, rect[0]);
    W32(name + kRect + 4, rect[1]);
    return true;
}

// hud.cpp, any element about to draw. On the keyboard a text naming the pad's buttons draws with the bound keys
// in their place, given to the draw the way a list gives a row its text, so what is kept follows the device: true
// when drawn here.
bool WordingElement(X86Context& ctx, uint8_t* base, uint32_t instance) {
    static bool drawing = false;
    static uint32_t buffer = 0;
    uint32_t widget = R32(instance + 8), screen = R32(kScreen);
    if (drawing || !widget || !screen || R32(widget) != kTextWidget || !(G<uint8_t>(instance + 0x18) & 1) ||
        !Keyboard(game::DrawingPage()))
        return false;
    uint32_t given = R32(screen + kGivenText);
    if (!given && R32(widget + 0x2C + 0x14) > 0x200) return false;
    std::u16string text = given ? std::u16string() : ReadText(widget);
    for (uint32_t i = 0; given && R16(given + i * 2); i++) {
        if (i == kMaxWording) return false;
        text += char16_t(R16(given + i * 2));
    }
    std::u16string keys = KeyWording(text);
    if (keys == text || keys.size() > kMaxWording) return false;
    if (!buffer) buffer = mem::AllocSystem((kMaxWording + 1) * 2);
    if (!buffer) return false;
    for (size_t i = 0; i <= keys.size(); i++) W16(buffer + uint32_t(i) * 2, i < keys.size() ? uint16_t(keys[i]) : 0);
    W32(screen + kGivenText, buffer);
    drawing = true;
    ctx.ecx = instance;
    sub_002C64E0(ctx, base);
    drawing = false;
    W32(screen + kGivenText, given);
    return true;
}

// hud.cpp, any element about to draw. On the keyboard a hint's icon goes and its key joins the caption, which
// ends where the icon did; with text after the icon the two close up around the key, centred as before: true when
// handled here.
bool HintElement(X86Context& ctx, uint8_t* base, uint32_t instance) {
    uint32_t id = R32(instance + 4), widget = R32(instance + 8);
    if (g_hinting || !widget || !(G<uint8_t>(instance + 0x18) & 1) || !Keyboard(game::DrawingPage())) return false;
    for (size_t i = 0; i < std::size(kHints); i++) {
        const Hint& h = kHints[i];
        Seen& s = g_seen[i];
        if (id == h.image) {
            uint32_t material = R32(widget + 0x3C);
            std::string key = material ? Label(R32(material + 4), input::InMenu(), 0) : std::string();
            if (key.empty()) return false;
            s = {g_tick, key, int16_t(R16(widget + kRect)), int16_t(R16(widget + kRect + 2))};
            ctx.esp += 4;  // the icon is not drawn
            return true;
        }
        bool after = h.after && id == h.after;
        if ((id != h.text && !after) || g_tick - s.tick > 2 || !Block()) continue;
        std::u16string text = ReadText(widget);
        if (text.empty()) return false;
        int w = int(s.key.size() + 3) * kTextChar2 / 2, grow = h.after ? (w - (s.r - s.l)) / 2 : 0;
        int end = h.after ? s.l - kIconGap - grow + w : s.r;  // of the caption with its key
        int l = int16_t(R16(widget + kRect)), r = int16_t(R16(widget + kRect + 2));
        uint32_t sides = R32(widget + kRect);
        W16(widget + kRect, uint16_t(int16_t(after ? end + kWordGap : l - w)));
        W16(widget + kRect + 2, uint16_t(int16_t(after ? r + end + kWordGap - l : end)));
        if (!after) {
            std::u16string line = text + u" (";
            for (char c : s.key) line += char16_t(uint8_t(c));
            WriteText(ctx, widget, line + u")");
        }
        g_hinting = true;
        ctx.ecx = instance;
        sub_002C64E0(ctx, base);
        g_hinting = false;
        if (!after) WriteText(ctx, widget, text);
        W32(widget + kRect, sides);
        return true;
    }
    return false;
}

// game thread, every input update
void PromptsTick(X86Context& ctx) {
    g_tick++;
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

// SetNavigation: remembered for a device change, and relabelled for the keyboard
X86_EXTERN_FUNC(__imp__sub_002915C1);
X86_FUNC(sub_002915C1) {
    uint32_t owner = ctx.ecx, page = R32(ctx.esp + 4), actions = R32(ctx.esp + 8), type = R32(ctx.esp + 12);
    if (page && actions && actions != g_block) Remember(owner, page, actions, type);
    if (page && actions && R32(page + 0x14) == kNameEntry && Keyboard(page) && Block())
        W32(ctx.esp + 8, WithoutY(actions));
    __imp__sub_002915C1(ctx, base);
    if (!page || !Keyboard(page)) return;
    uint32_t eax = ctx.eax;
    Relabel(ctx, page, type);
    ctx.eax = eax;
}
