// RECOMP SETTINGS, MOUSE AND KEYBOARD and CONTROLLER under Shadow Calibration in the options menu. Each opens the
// Display page (P_Video) with the F10 panel's rows in its list until B: A opens the list of a row's values (an on / off
// row flips) or rebinds; a range (no list) steps on Left, Right and A. RECOMP SETTINGS first lists its sections (the
// F10 headings named here) on P_Options itself, drawn as the stock two-bar pages (Solo > Load Game).
#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "app/app.h"
#include "core/input.h"
#include "game/magma.h"
#include "xdk/hle.h"

using namespace kernel;

namespace dispatch {
uint32_t HostThunk(X86Func* fn);
}  // namespace dispatch

namespace {

// COptionsMenu: +8 the listener its OnPress, OnBack and OnKey get, +0x14 owner, +0x28 P_Options, +0x2C its list,
// +0x44 P_Video, +0x48 its list. Owner +0x14 is set when a key was taken.
constexpr uint32_t kListener = 8, kOwner = 0x14, kOptionsList = 0x2C, kVideo = 0x44, kVideoList = 0x48, kTaken = 0x14;
constexpr uint32_t kOptionsName = 0x545D74, kVideoName = 0x545D4C, kListName = 0x545E04, kValidateName = 0x545E10;
constexpr uint32_t kPopupName = 0x54638C;
constexpr uint32_t kTitleName = 0x5420F8, kSection = 0x546318, kVideoTitle = 0x546238, kPackage = 0x541544;
constexpr uint32_t kIsPage = 0x28FA86;      // thiscall owner (name): that page has the screen
constexpr uint32_t kIsElement = 0x1A6090;   // cdecl (instance, name)
constexpr uint32_t kFind = 0x17D0E8;        // cdecl (container, name): instance, its element at +8
constexpr uint32_t kSetTitle = 0x1A6235;    // cdecl (page, element, section, key, package)
constexpr uint32_t kFillVideo = 0x1A4012;   // thiscall menu
constexpr uint32_t kFillOptions = 0x1A53B5; // thiscall menu
constexpr uint32_t kResetFocus = 0x2C0730;  // cdecl (page)
constexpr uint32_t kShow = 0x292318;        // thiscall owner (page, show, 1, 0)
constexpr uint32_t kSetText = 0x183598;     // thiscall FString (text)
constexpr uint32_t kSetEnabled = 0x2C87E0;  // thiscall instance (bool)
constexpr uint32_t kSoundPlayer = 0x1838BD, kSoundOwner = 0x17B720;
// the stock option popup as a Difficulty press opens it, through the Display options (menu +0x88) and their popup
// controller at +0x18. Open (thiscall options, FString array, row, option, ret 0xC; only the options' entry is a
// function) lists the array; the option's vtable is called back, +0 with the row picked and +4 on B (thiscall row,
// item, FString*, ret 0xC).
constexpr uint32_t kVideoOptions = 0x88, kPopupController = 0x18, kOpenPopup = 0x1BF28E;
constexpr uint32_t kPopupList = 0x88 + 8;  // controller: +0x88 the popup, its +8 list
constexpr uint32_t kPopupSize = 0x4000, kMaxChoices = 64;
// the options' base vtable (15 slots): the popup also calls +8 as its selection moves
constexpr uint32_t kOptionVtable = 0x547C24, kOptionSlots = 15;
// list: thiscall Clear, AddItem(text, 0), SetCell(row, column, text), Layout, Select(row, 0, 1), ColumnCount on +0x6C;
// +0x60 items begin, +0x64 end (0x38 each), +0x9A rows shown, +0x9C selection
constexpr uint32_t kClear = 0x2C4D00, kAddItem = 0x2C5160, kSetCell = 0x2C40D0, kLayout = 0x2C1710, kSelect = 0x2C2040;
constexpr uint32_t kColumns = 0x18EBFD;
constexpr uint32_t kItems = 0x60, kRowsShown = 0x9A, kSelection = 0x9C, kItemSize = 0x38;
// instance: +0x18 bit 0 visible, +0x4C flags
constexpr uint32_t kVisible = 0x18, kFlags = 0x4C;
constexpr uint32_t kLeft = 0x25, kRight = 0x27, kEscape = 0x1B;
const char* const kCaptions[] = {"Recomp Settings", "Mouse and Keyboard", "Controller"};
constexpr int kCaptionCount = int(std::size(kCaptions));
const std::string_view kSections[] = {"GENERAL", "DISPLAY", "GRAPHICS"};

// the F10 panel's upper case in the game's title case: "DISPLAY MODE" as "Display Mode", "SPACE/ENTER" as "Space/Enter"
std::string Title(std::string_view text) {
    constexpr std::string_view kSmall[] = {"AND", "OR", "OF", "TO", "A"};
    constexpr std::string_view kKeep[] = {"FPS", "EEV", "EMF", "LMB", "RMB", "MMB", "IP", "OPSAT", "ESC", "HDR", "MSAA", "SMAA",
                                          "LB", "RB", "LT", "RT", "LS", "RS"};
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        size_t end = text.find_first_of(" /()", i);
        if (end == std::string_view::npos) end = text.size();
        std::string word(text.substr(i, end - i));
        bool digit = word.find_first_of("0123456789") != std::string::npos;
        if (word == "HZ") word = "Hz";
        else if (word == "D-PAD") word = "D-Pad";
        else if (word == "MS") word = "ms";
        else if (!digit && std::find(std::begin(kKeep), std::end(kKeep), word) == std::end(kKeep)) {
            bool small = !out.empty() && std::find(std::begin(kSmall), std::end(kSmall), word) != std::end(kSmall);
            for (size_t k = 0; k < word.size(); k++)
                if (small || k) word[k] = char(std::tolower(uint8_t(word[k])));
        }
        out += word;
        if (end < text.size()) out += text[end];
        i = end + 1;
    }
    return out;
}

struct Take {
    int mode = -1;
    uint32_t menu = 0, owner = 0, page = 0, list = 0;
    bool seen = false, leaving = false;
    std::vector<const app::rows::Row*> rows;  // per list row, null restores defaults
    int section = -1;                         // RECOMP SETTINGS: the heading shown
    bool refill = false;                      // a pick can change which rows show
    int last_row = 0;                         // the selection last tick, which way it moved onto a gap
};
Take g_take;  // game thread

// a blank row between the MOUSE and KEYBOARD rows: the lists have no divider of their own
const app::rows::Row kGap{app::rows::Kind::Heading, -1, ""};
int g_first = -1;          // P_Options row of RECOMP SETTINGS
uint8_t g_shown = 0;       // P_Video's own rows shown
constexpr size_t kRowsMax = 14;  // ours shown at once, from kListTop to the frame's bottom
// a list's place is its +0x10 x, +0x12 y (GetPosition `sub_002C1030` on its +8 part); ours starts right under the
// titles, where the stock page leaves room above its few rows
constexpr uint32_t kListY = 0x12;
constexpr int16_t kListTop = 116;
// P_Video's own (options.mgm): before that page has been shown the list's y is still 0, so it is not read back
constexpr int16_t kStockListY = 205;
// the long pages (MOUSE AND KEYBOARD, CONTROLLER) start under the titles; the RECOMP SETTINGS sections keep the stock place
int16_t ListTop() { return g_take.mode >= 1 ? kListTop : kStockListY; }
uint8_t g_validate[2] = {};
bool g_validate_saved = false, g_restore = false;
// P_Video's upper title (L_Text, "OPTIONS"): RECOMP SETTINGS shows its name there and the section's below
constexpr uint32_t kTopTextId = 0xC9E6E44B;
std::u16string g_top_text;
bool g_top_saved = false;
int g_capture = -1;
bool g_armed = false;
uint32_t g_text = 0;
uint32_t g_popup = 0;  // our option (+0) and its vtable (+0x20), the array (+0x60), its strings, their text
int g_popup_row = -1, g_popup_current = 0;

uint32_t Wide(std::string_view text) {
    if (!g_text) g_text = mem::AllocSystem(0x400);
    size_t n = std::min<size_t>(text.size(), 0x1FF);
    for (size_t i = 0; i < n; i++) W16(g_text + uint32_t(i) * 2, uint8_t(text[i]));
    W16(g_text + uint32_t(n) * 2, 0);
    return g_text;
}

uint32_t Wide16(const std::u16string& text) {
    if (!g_text) g_text = mem::AllocSystem(0x400);
    size_t n = std::min<size_t>(text.size(), 0x1FF);
    for (size_t i = 0; i < n; i++) W16(g_text + uint32_t(i) * 2, uint16_t(text[i]));
    W16(g_text + uint32_t(n) * 2, 0);
    return g_text;
}

// a text widget's wstring at +0x2C: +4 its characters (a pointer from 8 on), +0x14 length, +0x18 capacity
std::u16string ReadWide(uint32_t text) {
    uint32_t string = text + 0x2C, length = std::min<uint32_t>(R32(string + 0x14), 0x100);
    uint32_t data = R32(string + 0x18) >= 8 ? R32(string + 4) : string + 4;
    std::u16string out;
    for (uint32_t i = 0; i < length; i++) out += char16_t(R16(data + i * 2));
    return out;
}

uint32_t PageChild(uint32_t page, uint32_t id) {
    uint32_t begin = page ? R32(page + 8) : 0, end = page ? R32(page + 0xC) : 0;
    if (!begin || end < begin || end - begin > 4 * 512) return 0;
    for (uint32_t it = begin; it < end; it += 4)
        if (uint32_t instance = R32(it); instance && R32(instance + 4) == id) return instance;
    return 0;
}

int Count(uint32_t list) {
    uint32_t begin = R32(list + kItems), end = R32(list + kItems + 4);
    return begin && end > begin ? int((end - begin) / kItemSize) : 0;
}

bool IsPage(X86Context& ctx, uint32_t owner, uint32_t name) {
    ctx.ecx = owner;
    return CallGuest(ctx, kIsPage, {name}) & 0xFF;
}

void Sound(X86Context& ctx, uint32_t owner) {
    ctx.ecx = owner;
    uint32_t player = CallGuest(ctx, kSoundPlayer, {});
    if (!player) return;
    uint32_t sounds = CallGuest(ctx, kSoundOwner, {});
    ctx.ecx = player;
    CallGuest(ctx, R32(R32(player) + 0x78), {R32(sounds + 0x2AC), R32(R32(owner + 4) + 0x5C), 3, 0, 0, 0, 0});
}

void Unblock() { input::SetBlocked(app::MenuOpen() || app::ConsoleOpen()); }

void Refresh(X86Context& ctx) {
    ctx.ecx = g_take.list + 0x6C;
    bool two = int8_t(CallGuest(ctx, kColumns, {})) > 1;
    for (size_t i = 0; i < g_take.rows.size(); i++) {
        const app::rows::Row* r = g_take.rows[i];
        std::string label = Title(r ? r->label : "RESTORE DEFAULTS"), value;
        const char* prompt = r && r->kind == app::rows::Kind::PadBind ? "PRESS A BUTTON" : "PRESS A KEY";
        if (r) value = Title(g_capture == int(i) ? std::string(prompt) : app::rows::Value(*r));
        ctx.ecx = g_take.list;
        if (!two) {
            CallGuest(ctx, kSetCell, {uint32_t(i), 0, Wide(r ? label + "  " + value : label)});
            continue;
        }
        // as the stock rows: the item's own text is not drawn
        CallGuest(ctx, kSetCell, {uint32_t(i), 0, Wide(label)});
        ctx.ecx = g_take.list;
        CallGuest(ctx, kSetCell, {uint32_t(i), 1, Wide(value)});
    }
}

void Picked(X86Context& __restrict ctx, uint8_t*) {
    int pick = int(R32(ctx.esp + 4));
    const app::rows::Row* r = g_take.mode >= 0 && g_popup_row >= 0 && g_popup_row < int(g_take.rows.size())
                                  ? g_take.rows[size_t(g_popup_row)] : nullptr;
    // the value it has would be applied again
    if (r && pick != g_popup_current) {
        app::rows::Choose(*r, pick);
        Refresh(ctx);
        g_take.refill = true;
    }
    g_popup_row = -1;
    ctx.esp += 16;
}

void Cancelled(X86Context& __restrict ctx, uint8_t*) {
    g_popup_row = -1;
    ctx.esp += 16;
}

void Popup(X86Context& ctx, int row, const std::vector<std::string>& choices, int current) {
    if (!R32(g_take.menu + kVideoOptions + kPopupController)) {
        LOGI("options menu: no popup controller");
        return;
    }
    // laid out as a stock option (+4 its options, +0x10 its value) for the base methods
    if (!g_popup) {
        g_popup = mem::AllocSystem(kPopupSize);
        uint32_t vtable = g_popup + 0x20;
        for (uint32_t i = 0; i < kOptionSlots; i++) W32(vtable + i * 4, R32(kOptionVtable + i * 4));
        W32(vtable, dispatch::HostThunk(Picked));
        W32(vtable + 4, dispatch::HostThunk(Cancelled));
        W32(g_popup, vtable);
    }
    W32(g_popup + 4, g_take.menu + kVideoOptions);
    W32(g_popup + 0x10, uint32_t(current));
    uint32_t array = g_popup + 0x60, strings = array + 0xC, text = strings + kMaxChoices * 0xC, n = 0;
    for (const std::string& choice : choices) {
        std::string t = Title(choice);
        uint32_t length = uint32_t(t.size()) + 1;  // FString counts its terminator
        if (n == kMaxChoices || text + length * 2 > g_popup + kPopupSize) break;
        for (uint32_t i = 0; i < length; i++) W16(text + i * 2, i < t.size() ? uint8_t(t[i]) : 0);
        W32(strings + n * 0xC, text);
        W32(strings + n * 0xC + 4, length);
        W32(strings + n * 0xC + 8, length);
        text += length * 2;
        n++;
    }
    W32(array, strings);
    W32(array + 4, n);
    W32(array + 8, n);
    g_popup_row = row;
    g_popup_current = current;
    ctx.ecx = g_take.menu + kVideoOptions;
    CallGuest(ctx, kOpenPopup, {array, uint32_t(current), g_popup});
    // opening resets the page's focus, which puts its list back on the first row
    uint32_t list = R32(R32(g_take.menu + kVideoOptions + kPopupController) + kPopupList);
    if (list) {
        ctx.ecx = list;
        CallGuest(ctx, kSelect, {uint32_t(current), 0, 1});
    }
    LOGI("options menu: popup of %u for row %d", n, row);
}

void Fill(X86Context& ctx, int select = 0) {
    using app::rows::Kind;
    size_t count = 0;
    const app::rows::Row* page = app::rows::Page(g_take.mode, &count);
    g_take.rows.clear();
    if (g_take.mode == 0) {
        for (size_t i = size_t(g_take.section) + 1; i < count && page[i].kind != Kind::Heading; i++)
            if (page[i].kind == Kind::Value && app::rows::Shown(page[i])) g_take.rows.push_back(&page[i]);
    } else {
        for (size_t i = 0; i < count; i++) {
            if (page[i].kind == Kind::Heading && !g_take.rows.empty()) g_take.rows.push_back(&kGap);
            if (page[i].kind == Kind::Value || page[i].kind == Kind::Bind || page[i].kind == Kind::PadBind)
                g_take.rows.push_back(&page[i]);
        }
        g_take.rows.push_back(&kGap);
        g_take.rows.push_back(nullptr);
    }
    uint32_t list = g_take.list;
    ctx.ecx = list;
    CallGuest(ctx, kClear, {});
    for (const app::rows::Row* r : g_take.rows) CallGuest(ctx, kAddItem, {Wide(Title(r ? r->label : "RESTORE DEFAULTS")), 0});
    Refresh(ctx);
    G<uint8_t>(list + kRowsShown) = uint8_t(std::min<size_t>(g_take.rows.size(), kRowsMax));
    ctx.ecx = list;
    CallGuest(ctx, kLayout, {});
    CallGuest(ctx, kSelect, {uint32_t(select), 0, 1});
}

// as 1A6235: text elements (type 8) keep it at +0x2C, buttons (2, 3, 4) at +0x28
void SetTitle(X86Context& ctx, uint32_t page, std::string_view text) {
    uint32_t instance = CallGuest(ctx, kFind, {page, kTitleName});
    uint32_t element = instance ? R32(instance + 8) : 0;
    if (!element) return;
    ctx.ecx = element;
    uint32_t type = CallGuest(ctx, R32(R32(element) + 0xC), {});
    if (type != 8 && (type < 2 || type > 4)) return;
    ctx.ecx = element + (type == 8 ? 0x2C : 0x28);
    CallGuest(ctx, kSetText, {Wide(text)});
}

// the in-game Confirm Changes button
void RestoreStock(X86Context& ctx, uint32_t menu) {
    uint32_t page = R32(menu + kVideo);
    if (!page) return;
    CallGuest(ctx, kSetTitle, {page, kTitleName, kSection, kVideoTitle, kPackage});
    uint32_t top = g_top_saved ? PageChild(page, kTopTextId) : 0;
    if (top && R32(top + 8)) {
        ctx.ecx = R32(top + 8) + 0x2C;
        CallGuest(ctx, kSetText, {Wide16(g_top_text)});
    }
    g_top_saved = false;
    uint32_t validate = CallGuest(ctx, kFind, {page, kValidateName});
    if (validate && g_validate_saved) {
        G<uint8_t>(validate + kVisible) = g_validate[0];
        G<uint8_t>(validate + kFlags) = g_validate[1];
    }
    g_validate_saved = false;
}

void Enter(X86Context& ctx, uint32_t menu, int mode, int section = -1) {
    uint32_t owner = R32(menu + kOwner), page = R32(menu + kVideo), list = R32(menu + kVideoList);
    if (!owner || !page || !list || (mode == 0 && section < 0)) return;
    if (!g_shown) g_shown = std::max<uint8_t>(G<uint8_t>(list + kRowsShown), 4);
    g_take = {mode, menu, owner, page, list, false, false, {}, section};
    W16(list + kListY, uint16_t(ListTop()));
    Fill(ctx);
    size_t count = 0;
    const app::rows::Row* rows = app::rows::Page(mode, &count);
    SetTitle(ctx, page, section < 0 ? std::string(kCaptions[mode]) : Title(rows[section].label));
    uint32_t top = section < 0 ? 0 : PageChild(page, kTopTextId), top_text = top ? R32(top + 8) : 0;
    if (top_text) {
        if (!g_top_saved) g_top_text = ReadWide(top_text);
        g_top_saved = true;
        ctx.ecx = top_text + 0x2C;
        CallGuest(ctx, kSetText, {Wide(kCaptions[mode])});
    }
    if (uint32_t validate = CallGuest(ctx, kFind, {page, kValidateName})) {
        g_validate[0] = G<uint8_t>(validate + kVisible);
        g_validate[1] = G<uint8_t>(validate + kFlags);
        g_validate_saved = true;
        ctx.ecx = validate;
        CallGuest(ctx, kSetEnabled, {0});
        G<uint8_t>(validate + kVisible) &= ~1;
    }
    CallGuest(ctx, kResetFocus, {page});
    ctx.ecx = owner;
    CallGuest(ctx, kShow, {page, 1, 1, 0});
    Sound(ctx, owner);
    LOGI("options menu: %s, %zu rows, %u shown", kCaptions[mode], g_take.rows.size(), g_shown);
}

void StopCapture() {
    if (g_capture < 0) return;
    g_capture = -1;
    Unblock();
}

void Leave(X86Context& ctx) {
    StopCapture();
    W16(g_take.list + kListY, uint16_t(kStockListY));
    ctx.ecx = g_take.menu;
    CallGuest(ctx, kFillVideo, {});
    ctx.ecx = g_take.list;
    CallGuest(ctx, kLayout, {});
    RestoreStock(ctx, g_take.menu);
    g_take = {};
}

// step 0 is A
void Act(X86Context& ctx, int step) {
    int row = int16_t(R16(g_take.list + kSelection));
    LOGI("options menu: row %d step %d", row, step);
    if (row < 0 || row >= int(g_take.rows.size())) return;
    const app::rows::Row* r = g_take.rows[row];
    if (r == &kGap) return;
    if (!r) {
        if (step) return;
        app::rows::Defaults(g_take.mode);
    } else if (r->kind == app::rows::Kind::Value) {
        int current = 0;
        std::vector<std::string> choices = app::rows::Choices(*r, &current);
        // a row with a list changes only through the popup, an on / off one flips on A
        bool flip = choices.size() == 2 && choices[0] == "OFF" && choices[1] == "ON";
        if (choices.size() > 1 && (!flip || step)) {
            if (!step) Popup(ctx, row, choices, current);
            return;
        }
        app::rows::Step(*r, step ? step : 1);
    } else {
        if (step) return;
        g_capture = row;
        g_armed = false;
        input::SetBlocked(true);
    }
    Refresh(ctx);
    Sound(ctx, g_take.owner);
}

bool OurPage(X86Context& ctx, uint32_t menu) {
    // the popup over it keeps its own keys
    return g_take.mode >= 0 && !g_take.leaving && menu == g_take.menu && IsPage(ctx, g_take.owner, kVideoName) &&
           !IsPage(ctx, g_take.owner, kPopupName);
}


// RECOMP SETTINGS lists its sections in P_Options' own list (the gray buttons), under the two-bar title of the stock
// pages like Solo > Load Game (system.mgm area 768229913, the one-bar 1515491561 with a second bar 30 lower): the
// bar and the title text draw a bar higher as well, the lower one with our caption, so the list stays put under them
// (a list's place comes from its instance, not the widget rect).
constexpr uint32_t kOptionsPage = 0x28, kBarId = 0x32542A69, kTitleId = 0xCFCADA23;
constexpr uint32_t kRect = 0x10, kChildren = 0x1C;  // widget: +0x10 left, +0x12 right, +0x14 top, +0x16 bottom
constexpr int16_t kBarStep = 30;
constexpr uint32_t kElementDraw = 0x2C64E0;  // thiscall instance
struct Sections {
    bool on = false;
    int pending = 0;  // 1 shows them, -1 brings the options back, once the list's key handling is over
    uint32_t menu = 0, page = 0, list = 0, bar = 0, title = 0;
};
Sections g_sections;  // game thread
bool g_drawing = false;

int Heading(std::string_view name) {
    size_t count = 0;
    const app::rows::Row* rows = app::rows::Page(0, &count);
    for (size_t i = 0; i < count; i++)
        if (rows[i].kind == app::rows::Kind::Heading && rows[i].label == name) return int(i);
    return -1;
}

bool OnSections(X86Context& ctx, uint32_t menu) {
    return g_sections.on && g_take.mode < 0 && menu == g_sections.menu && IsPage(ctx, R32(menu + kOwner), kOptionsName) &&
           !IsPage(ctx, R32(menu + kOwner), kPopupName);
}

void EndSections() { g_sections.on = false; }

// shown again, so its opening animation plays as on a stock page change
void Replay(X86Context& ctx, int row) {
    Sections& s = g_sections;
    uint32_t owner = R32(s.menu + kOwner);
    ctx.ecx = owner;
    CallGuest(ctx, kShow, {s.page, 0, 1, 0});
    CallGuest(ctx, kResetFocus, {s.page});
    ctx.ecx = owner;
    CallGuest(ctx, kShow, {s.page, 1, 1, 0});
    ctx.ecx = s.list;
    CallGuest(ctx, kSelect, {uint32_t(row), 0, 1});
}

void SectionsTick(X86Context& ctx) {
    Sections& s = g_sections;
    int pending = s.pending;
    s.pending = 0;
    if (pending > 0 && s.list) {
        s.on = true;
        uint32_t list = s.list;
        ctx.ecx = list;
        CallGuest(ctx, kClear, {});
        for (std::string_view name : kSections) {
            ctx.ecx = list;
            CallGuest(ctx, kAddItem, {Wide(Title(name)), 0});
        }
        G<uint8_t>(list + kRowsShown) = uint8_t(std::size(kSections));
        ctx.ecx = list;
        CallGuest(ctx, kLayout, {});
        Replay(ctx, 0);
        LOGI("options menu: sections, bar %08X title %08X", s.bar, s.title);
    } else if (pending < 0) {
        EndSections();
        ctx.ecx = s.menu;
        CallGuest(ctx, kFillOptions, {});
        Replay(ctx, g_first);
    }
}

// an area's images carry their own page place, so the bar moves by its children (area instance +0x1C, a box of them)
void Raise(uint32_t widget, bool area, int dy) {
    auto move = [dy](uint32_t w) {
        W16(w + kRect + 4, uint16_t(int16_t(R16(w + kRect + 4)) + dy));
        W16(w + kRect + 6, uint16_t(int16_t(R16(w + kRect + 6)) + dy));
    };
    if (!area) return move(widget);
    uint32_t box = R32(widget + kChildren);
    uint32_t begin = box ? R32(box + 8) : 0, end = box ? R32(box + 0xC) : 0;
    if (!begin || end < begin || end - begin > 4 * 64) return;
    for (uint32_t it = begin; it < end; it += 4)
        if (uint32_t child = R32(it); child && R32(child + 8)) move(R32(child + 8));
}

}  // namespace

X86_FUNC(sub_002C64E0);

namespace game {

// hud.cpp's element draw: true when it drew the instance
bool OptionsElement(X86Context& ctx, uint8_t* base, uint32_t instance) {
    Sections& s = g_sections;
    if (!s.on || g_drawing) return false;
    uint32_t widget = R32(instance + 8);
    if (!widget || (instance != s.bar && instance != s.title)) return false;
    g_drawing = true;
    Raise(widget, instance == s.bar, -kBarStep);
    // through CallGuest, which pushes its own return: a hook called directly returns past its caller's
    ctx.ecx = instance;
    CallGuest(ctx, kElementDraw, {});
    Raise(widget, instance == s.bar, kBarStep);
    std::u16string text;
    if (instance == s.title) {
        text = ReadWide(widget);
        ctx.ecx = widget + 0x2C;
        CallGuest(ctx, kSetText, {Wide(kCaptions[0])});
    }
    ctx.ecx = instance;
    sub_002C64E0(ctx, base);
    if (instance == s.title) {
        ctx.ecx = widget + 0x2C;
        CallGuest(ctx, kSetText, {Wide16(text)});
    }
    g_drawing = false;
    ctx.ecx = instance;
    return true;
}

// the id of the context whose draw or input stack holds the page (a player's menus: its player number), 0 for none
uint32_t StackOwner(uint32_t page) {
    uint32_t m = R32(0x6A6638);
    uint32_t list = m ? R32(m + 0x3C) : 0, count = m ? R32(m + 0x40) : 0;
    if (!page || !list || count > 16) return 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t context = R32(list + i * 4);
        for (uint32_t offset : {0x14u, 0x18u}) {
            uint32_t stack = context ? R32(context + offset) : 0;
            uint32_t begin = stack ? R32(stack + 4) : 0, end = stack ? R32(stack + 8) : 0;
            if (!begin || end < begin || end - begin > 8 * 64) continue;
            for (uint32_t it = begin; it < end; it += 8)
                if (R32(it) == page) return R32(context + 0x1C);
        }
    }
    return 0;
}

bool OnStack(uint32_t page) { return StackOwner(page) != 0; }

// mouse.cpp: the cursor leaves the selection alone over our blank row, else the pass over it flickers
bool BlankRow(uint32_t list, int row) {
    return g_take.mode >= 0 && list == g_take.list && row >= 0 && row < int(g_take.rows.size()) && g_take.rows[size_t(row)] == &kGap;
}

// the page's keyframes put the list back every tick: mouse.cpp calls this before its hit test (a list's rect comes
// from its place), MenuTick again
void MenuPlace(X86Context& ctx) {
    if (g_take.mode < 0 || g_take.leaving || int16_t(R16(g_take.list + kListY)) == ListTop()) return;
    W16(g_take.list + kListY, uint16_t(ListTop()));
    ctx.ecx = g_take.list;
    CallGuest(ctx, kLayout, {});
}

// game thread, every input update
void MenuTick(X86Context& ctx) {
    SectionsTick(ctx);
    if (g_take.mode < 0) return;
    // the stock rows go back only once the list's key handling is over, a refill under it leaves the page stale
    if (g_take.leaving) {
        Leave(ctx);
        LOGI("options menu: stock rows back");
        return;
    }
    if (g_take.refill) {
        g_take.refill = false;
        Fill(ctx, std::max(0, int(int16_t(R16(g_take.list + kSelection)))));
    }
    if (OnStack(g_take.page)) {
        g_take.seen = true;
    } else if (g_take.seen) {
        // closed under us (the pause menu went): the stock rows come back with the next options page
        StopCapture();
        W16(g_take.list + kListY, uint16_t(kStockListY));
        g_take = {};
        g_restore = true;
        return;
    }
    MenuPlace(ctx);
    // the gap between headings is passed over, on in the direction the selection came from
    int row = int16_t(R16(g_take.list + kSelection)), rows = int(g_take.rows.size());
    if (row >= 0 && row < rows && g_take.rows[size_t(row)] == &kGap) {
        row = std::clamp(row + (g_take.last_row < row ? 1 : -1), 0, rows - 1);
        ctx.ecx = g_take.list;
        CallGuest(ctx, kSelect, {uint32_t(row), 0, 1});
    }
    g_take.last_row = row;
    if (g_capture < 0) return;
    // a controller row takes any controller button, Escape cancels either
    const app::rows::Row* r = g_take.rows[size_t(g_capture)];
    bool pad = r && r->kind == app::rows::Kind::PadBind;
    int vk = input::PressedKey(), button = pad ? input::PressedPadButton() : -1;
    if (!g_armed) {
        g_armed = !vk && button < 0;
        return;
    }
    if (button >= 0) input::BindPad(size_t(r->id), button);
    else if (vk && !pad && vk != kEscape && r) input::Bind(size_t(r->id), vk);
    else if (vk != kEscape) return;
    StopCapture();
    Refresh(ctx);
    Sound(ctx, g_take.owner);
}

}  // namespace game

// options list fill (thiscall menu): Controls, Display, Sound, Shadow Calibration, then ours
X86_EXTERN_FUNC(__imp__sub_001A53B5);
X86_FUNC(sub_001A53B5) {
    uint32_t menu = ctx.ecx;
    EndSections();
    __imp__sub_001A53B5(ctx, base);
    uint32_t list = R32(menu + kOptionsList);
    int count = list ? Count(list) : 0;
    if (count < 2) return;  // split screen shows Controls only
    g_first = count;
    ctx.ecx = list;
    for (const char* caption : kCaptions) CallGuest(ctx, kAddItem, {Wide(caption), 0});
    G<uint8_t>(list + kRowsShown) = uint8_t(count + kCaptionCount);
    CallGuest(ctx, kLayout, {});
    if (g_restore) RestoreStock(ctx, menu);
    g_restore = false;
}

// OnPress (thiscall listener, instance, ret 4)
X86_EXTERN_FUNC(__imp__sub_001A4161);
X86_FUNC(sub_001A4161) {
    uint32_t menu = ctx.ecx - kListener, instance = R32(ctx.esp + 4), owner = R32(menu + kOwner);
    uint32_t list = R32(menu + kOptionsList);
    bool ours_list = instance && (CallGuest(ctx, kIsElement, {instance, kListName}) & 0xFF);
    int row = ours_list ? int16_t(R16(R32(instance + 8) + kSelection)) : -1;
    if (ours_list && R32(instance + 8) == list && OnSections(ctx, menu)) {
        if (row >= 0 && row < int(std::size(kSections))) Enter(ctx, menu, 0, Heading(kSections[row]));
        ctx.ecx = menu + kListener;
        G<uint8_t>(owner + kTaken) = 1;
        ctx.eax = 0;
        ctx.esp += 8;
        return;
    }
    if (ours_list && g_take.mode < 0 && g_first >= 0 && R32(instance + 8) == list && row >= g_first && row < g_first + kCaptionCount &&
        IsPage(ctx, owner, kOptionsName)) {
        if (row == g_first) {
            uint32_t page = R32(menu + kOptionsPage);
            g_sections.pending = 1;
            g_sections.menu = menu;
            g_sections.page = page;
            g_sections.list = list;
            g_sections.bar = PageChild(page, kBarId);
            g_sections.title = PageChild(page, kTitleId);
            Sound(ctx, owner);
        } else {
            Enter(ctx, menu, row - g_first);
        }
        ctx.ecx = menu + kListener;
        G<uint8_t>(owner + kTaken) = 1;
        ctx.eax = 0;
        ctx.esp += 8;
        return;
    }
    if (ours_list && OurPage(ctx, menu)) {
        if (g_capture < 0) Act(ctx, 0);
        G<uint8_t>(owner + kTaken) = 1;
        ctx.eax = 1;
        ctx.esp += 8;
        return;
    }
    ctx.ecx = menu + kListener;
    __imp__sub_001A4161(ctx, base);
}

// OnBack (thiscall listener, one argument, ret 4)
X86_EXTERN_FUNC(__imp__sub_001A5C41);
X86_FUNC(sub_001A5C41) {
    uint32_t menu = ctx.ecx - kListener;
    if (OurPage(ctx, menu)) {
        LOGI("options menu: back");
        uint32_t owner = g_take.owner;
        ctx.ecx = owner;
        CallGuest(ctx, kShow, {g_take.page, 0, 1, 0});
        StopCapture();
        g_take.leaving = true;
        G<uint8_t>(owner + kTaken) = 0;
        ctx.eax = 0;
        ctx.esp += 8;
        return;
    }
    if (OnSections(ctx, menu)) {
        g_sections.pending = -1;
        Sound(ctx, R32(menu + kOwner));
        G<uint8_t>(R32(menu + kOwner) + kTaken) = 0;
        ctx.eax = 0;
        ctx.esp += 8;
        return;
    }
    ctx.ecx = menu + kListener;
    __imp__sub_001A5C41(ctx, base);
}

// OnKey (thiscall listener, instance, int* key, ret 8): arrow key codes
X86_EXTERN_FUNC(__imp__sub_001A5786);
X86_FUNC(sub_001A5786) {
    uint32_t menu = ctx.ecx - kListener, key_at = R32(ctx.esp + 8);
    uint32_t key = key_at ? R32(key_at) : 0;
    if ((key == kLeft || key == kRight) && OurPage(ctx, menu)) {
        if (g_capture < 0) Act(ctx, key == kLeft ? -1 : 1);
        G<uint8_t>(g_take.owner + kTaken) = 1;
        ctx.eax = 1;
        ctx.esp += 12;
        return;
    }
    ctx.ecx = menu + kListener;
    __imp__sub_001A5786(ctx, base);
}
