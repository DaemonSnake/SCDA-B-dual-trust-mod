// RECOMP SETTINGS and MOUSE AND KEYBOARD under Shadow Calibration in the options menu. Each opens the Display page
// (P_Video) with the F10 panel's rows in its list until B: Left and Right change a value, A steps it or rebinds.
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "app/app.h"
#include "core/input.h"
#include "game/magma.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// COptionsMenu: +8 the listener its OnPress, OnBack and OnKey get, +0x14 owner, +0x28 P_Options, +0x2C its list,
// +0x44 P_Video, +0x48 its list. Owner +0x14 is set when a key was taken.
constexpr uint32_t kListener = 8, kOwner = 0x14, kOptionsList = 0x2C, kVideo = 0x44, kVideoList = 0x48, kTaken = 0x14;
constexpr uint32_t kOptionsName = 0x545D74, kVideoName = 0x545D4C, kListName = 0x545E04, kValidateName = 0x545E10;
constexpr uint32_t kTitleName = 0x5420F8, kSection = 0x546318, kVideoTitle = 0x546238, kPackage = 0x541544;
constexpr uint32_t kIsPage = 0x28FA86;      // thiscall owner (name): that page has the screen
constexpr uint32_t kIsElement = 0x1A6090;   // cdecl (instance, name)
constexpr uint32_t kFind = 0x17D0E8;        // cdecl (container, name): instance, its element at +8
constexpr uint32_t kSetTitle = 0x1A6235;    // cdecl (page, element, section, key, package)
constexpr uint32_t kFillVideo = 0x1A4012;   // thiscall menu
constexpr uint32_t kResetFocus = 0x2C0730;  // cdecl (page)
constexpr uint32_t kShow = 0x292318;        // thiscall owner (page, show, 1, 0)
constexpr uint32_t kSetText = 0x183598;     // thiscall FString (text)
constexpr uint32_t kSetEnabled = 0x2C87E0;  // thiscall instance (bool)
constexpr uint32_t kSoundPlayer = 0x1838BD, kSoundOwner = 0x17B720;
// list: thiscall Clear, AddItem(text, 0), SetCell(row, column, text), Layout, Select(row, 0, 1), ColumnCount on +0x6C;
// +0x60 items begin, +0x64 end (0x38 each), +0x9A rows shown, +0x9C selection
constexpr uint32_t kClear = 0x2C4D00, kAddItem = 0x2C5160, kSetCell = 0x2C40D0, kLayout = 0x2C1710, kSelect = 0x2C2040;
constexpr uint32_t kColumns = 0x18EBFD;
constexpr uint32_t kItems = 0x60, kRowsShown = 0x9A, kSelection = 0x9C, kItemSize = 0x38;
// instance: +0x18 bit 0 visible, +0x4C flags
constexpr uint32_t kVisible = 0x18, kFlags = 0x4C;
constexpr uint32_t kLeft = 0x25, kRight = 0x27, kEscape = 0x1B;
const char* const kCaptions[] = {"Recomp Settings", "Mouse and Keyboard"};

// the F10 panel's upper case in the game's title case: "DISPLAY MODE" as "Display Mode", "SPACE/ENTER" as "Space/Enter"
std::string Title(std::string_view text) {
    constexpr std::string_view kSmall[] = {"AND", "OR", "OF", "TO", "A"}, kKeep[] = {"FPS", "EEV", "EMF", "LMB", "RMB", "MMB", "IP", "OPSAT", "ESC"};
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        size_t end = text.find_first_of(" /()", i);
        if (end == std::string_view::npos) end = text.size();
        std::string word(text.substr(i, end - i));
        bool digit = word.find_first_of("0123456789") != std::string::npos;
        if (word == "HZ") word = "Hz";
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
};
Take g_take;  // game thread
int g_first = -1;          // P_Options row of RECOMP SETTINGS
uint8_t g_shown = 0;       // P_Video's own rows shown
uint8_t g_validate[2] = {};
bool g_validate_saved = false, g_restore = false;
int g_capture = -1;
bool g_armed = false;
uint32_t g_text = 0;

uint32_t Wide(std::string_view text) {
    if (!g_text) g_text = mem::AllocSystem(0x400);
    size_t n = std::min<size_t>(text.size(), 0x1FF);
    for (size_t i = 0; i < n; i++) W16(g_text + uint32_t(i) * 2, uint8_t(text[i]));
    W16(g_text + uint32_t(n) * 2, 0);
    return g_text;
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
        if (!r) continue;
        std::string value = Title(g_capture == int(i) ? std::string("PRESS A KEY") : app::rows::Value(*r));
        ctx.ecx = g_take.list;
        CallGuest(ctx, kSetCell, {uint32_t(i), two ? 1u : 0u, Wide(two ? value : Title(r->label) + "  " + value)});
    }
}

void Fill(X86Context& ctx) {
    size_t count = 0;
    const app::rows::Row* page = app::rows::Page(g_take.mode, &count);
    g_take.rows.clear();
    for (size_t i = 0; i < count; i++)
        if (page[i].kind == app::rows::Kind::Value || page[i].kind == app::rows::Kind::Bind) g_take.rows.push_back(&page[i]);
    g_take.rows.push_back(nullptr);
    uint32_t list = g_take.list;
    ctx.ecx = list;
    CallGuest(ctx, kClear, {});
    for (const app::rows::Row* r : g_take.rows) CallGuest(ctx, kAddItem, {Wide(Title(r ? r->label : "RESTORE DEFAULTS")), 0});
    Refresh(ctx);
    G<uint8_t>(list + kRowsShown) = uint8_t(std::min<size_t>(g_take.rows.size(), g_shown));
    ctx.ecx = list;
    CallGuest(ctx, kLayout, {});
    CallGuest(ctx, kSelect, {0, 0, 1});
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
    uint32_t validate = CallGuest(ctx, kFind, {page, kValidateName});
    if (validate && g_validate_saved) {
        G<uint8_t>(validate + kVisible) = g_validate[0];
        G<uint8_t>(validate + kFlags) = g_validate[1];
    }
    g_validate_saved = false;
}

void Enter(X86Context& ctx, uint32_t menu, int mode) {
    uint32_t owner = R32(menu + kOwner), page = R32(menu + kVideo), list = R32(menu + kVideoList);
    if (!owner || !page || !list) return;
    if (!g_shown) g_shown = std::max<uint8_t>(G<uint8_t>(list + kRowsShown), 4);
    g_take = {mode, menu, owner, page, list, false, false, {}};
    Fill(ctx);
    SetTitle(ctx, page, kCaptions[mode]);
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
    ctx.ecx = g_take.menu;
    CallGuest(ctx, kFillVideo, {});
    RestoreStock(ctx, g_take.menu);
    g_take = {};
}

// step 0 is A
void Act(X86Context& ctx, int step) {
    int row = int16_t(R16(g_take.list + kSelection));
    LOGI("options menu: row %d step %d", row, step);
    if (row < 0 || row >= int(g_take.rows.size())) return;
    const app::rows::Row* r = g_take.rows[row];
    if (!r) {
        if (step) return;
        app::rows::Defaults(g_take.mode);
    } else if (r->kind == app::rows::Kind::Value) {
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
    return g_take.mode >= 0 && !g_take.leaving && menu == g_take.menu && IsPage(ctx, g_take.owner, kVideoName);
}

}  // namespace

namespace game {

bool OnStack(uint32_t page) {
    uint32_t m = R32(0x6A6638);
    uint32_t list = m ? R32(m + 0x3C) : 0, count = m ? R32(m + 0x40) : 0;
    if (!page || !list || count > 16) return false;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t context = R32(list + i * 4);
        for (uint32_t offset : {0x14u, 0x18u}) {
            uint32_t stack = context ? R32(context + offset) : 0;
            uint32_t begin = stack ? R32(stack + 4) : 0, end = stack ? R32(stack + 8) : 0;
            if (!begin || end < begin || end - begin > 8 * 64) continue;
            for (uint32_t it = begin; it < end; it += 8)
                if (R32(it) == page) return true;
        }
    }
    return false;
}

// game thread, every input update
void MenuTick(X86Context& ctx) {
    if (g_take.mode < 0) return;
    // the stock rows go back only once the list's key handling is over, a refill under it leaves the page stale
    if (g_take.leaving) {
        Leave(ctx);
        LOGI("options menu: stock rows back");
        return;
    }
    if (OnStack(g_take.page)) {
        g_take.seen = true;
    } else if (g_take.seen) {
        // closed under us (the pause menu went): the stock rows come back with the next options page
        StopCapture();
        g_take = {};
        g_restore = true;
        return;
    }
    if (g_capture < 0) return;
    int vk = input::PressedKey();
    if (!g_armed) {
        g_armed = !vk;
        return;
    }
    if (!vk) return;
    const app::rows::Row* r = g_take.rows[size_t(g_capture)];
    if (vk != kEscape && r) input::Bind(size_t(r->id), vk);
    StopCapture();
    Refresh(ctx);
    Sound(ctx, g_take.owner);
}

}  // namespace game

// options list fill (thiscall menu): Controls, Display, Sound, Shadow Calibration, then ours
X86_EXTERN_FUNC(__imp__sub_001A53B5);
X86_FUNC(sub_001A53B5) {
    uint32_t menu = ctx.ecx;
    __imp__sub_001A53B5(ctx, base);
    uint32_t list = R32(menu + kOptionsList);
    int count = list ? Count(list) : 0;
    if (count < 2) return;  // split screen shows Controls only
    g_first = count;
    ctx.ecx = list;
    for (const char* caption : kCaptions) CallGuest(ctx, kAddItem, {Wide(caption), 0});
    G<uint8_t>(list + kRowsShown) = uint8_t(count + 2);
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
    if (ours_list && g_take.mode < 0 && g_first >= 0 && R32(instance + 8) == list && row >= g_first && row < g_first + 2 &&
        IsPage(ctx, owner, kOptionsName)) {
        Enter(ctx, menu, row - g_first);
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

// OnBack (thiscall listener)
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
        ctx.esp += 4;
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
