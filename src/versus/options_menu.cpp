// RECOMP SETTINGS, MOUSE AND KEYBOARD and CONTROLLER under VOICE in the versus Options page. RECOMP SETTINGS lists
// its sections (as story's: General, Display, Graphics) on P_Options' own rows; a section, MOUSE AND KEYBOARD and
// CONTROLLER open a page of the F10 panel's rows in P_Options_Game's frame. Magma 5.0 loads its packages from XML
// text, so what the stock packages lack is spliced into that text before it is parsed and the game builds it like
// its own: the three buttons (copies of the last one, Online, shown as VOICE, a row lower each, in the up / down loop
// between it and Game) and the two pages in Options, the rows page's row (the option rows' CHK_Default as a button:
// its bar and label, and a value on the right) in common, and the popup listing a row's values (a copy of
// P_TeamsActionPopup, on that package's own B_OptionItem rows, at the place of the map list's popup) in xboxlive.
// Labels and titles come through the text lookup (xbox_live.cpp) or are set when a page opens. As story's options
// menu: A flips an on / off row, opens the popup on a row with a list and steps the others, Left / Right step those.
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "app/app.h"
#include "app/translations.h"
#include "xdk/hle.h"

using namespace kernel;

namespace dispatch {
uint32_t HostThunk(X86Func* fn);
}  // namespace dispatch

namespace {

constexpr uint32_t kMalloc = 0x149DF0;  // cdecl (size, tag)
constexpr uint32_t kFree = 0x11230;     // cdecl (pointer)
constexpr uint32_t kOptionsPage = 0xD7F513E8;
constexpr int kRowStep = 24;  // the stock rows' spacing, y 250 to 322

// menus: the base constructor (thiscall menu, FString* package, FString* page, ret 8) keeps the names the push
// (cdecl (menu, 1, 1)) finds the page by, then calls the open (vtable slot 6, thiscall, no arguments)
constexpr uint32_t kMenuSize = 0x10C, kMenuCtor = 0x1AA210, kMenuVtable = 0x338BD8, kMenuSlots = 13, kOpenSlot = 6;
constexpr uint32_t kBaseOpen = 0x1AA0D0, kPush = 0x193950;
// slot 1 runs as a menu comes on top again (after the one over it went): the base's does nothing, P_Options' and
// P_Options_Game's set the legend, (B) Back (A) Ok, which the page over it changed
constexpr uint32_t kLegendSlot = 1, kOptionsLegend = 0x1C44B0;
constexpr uint32_t kFString = 0x16540;      // thiscall (FString, const char*): {wide text, count, capacity}
constexpr uint32_t kPage = 0x28;             // menu: its Magma page
// an instance's handler (bound by thiscall menu (handler, instance id), ret 8): a refcounted object {vtable,
// refcount, ..} whose vtable +0x10 is the press (thiscall (one argument), ret 4) and +0x18 the other events
// (thiscall (?, event*), ret 8), both true when handled; the Game button's vtable
constexpr uint32_t kBind = 0x193A20, kHandlerVtable = 0x338E48, kHandlerSlots = 8, kPressSlot = 4, kEventSlot = 6;
constexpr uint32_t kHandlerTail = 0x3380E0;
// B as P_Options takes it, in its menu's key slot 10 (thiscall (key, action, ?), ret 0xC, true when handled): on
// B's release (IK_Joy2 0xC9, IST_Release 3) the menu is popped (cdecl, no arguments). Taken on the press (handler
// event 0x81) instead, the release would pop the menu under it as well.
constexpr uint32_t kKeySlot = 10, kJoyB = 0xC9, kRelease = 3, kPopMenu = 0x193E40;
constexpr uint32_t kSoundPlayer = 0x3D8D78, kBackSound = 2;  // the sound player's vtable +8
constexpr uint32_t kLeft = 0x25, kUp = 0x26, kRight = 0x27, kDown = 0x28;  // events: the directions' virtual keys
constexpr uint32_t kSelectSound = 1;             // as a stock page's X press plays
// a popup pushes over the page under it (cdecl (menu, 0, 1)), a page in its place (menu, 1, 1)
constexpr uint32_t kSelect = 0x25EE00;  // list widget: thiscall (row, notify), ret 8
constexpr uint32_t kFind = 0x25A320;  // thiscall page (instance id)
// list widget (the instance's +0x10): Clear, AddItem (text, data) -> row, Part (element id) -> column,
// SetText (row, column, text), which keeps the pointer
constexpr uint32_t kWidget = 0x10, kClear = 0x260B40, kAddItem = 0x261AB0, kPart = 0x260540, kSetText = 0x2605F0;
constexpr uint32_t kEmpty = 0x31BA80;  // L""
constexpr uint32_t kTextString = 0x0C;  // text widget: its string
constexpr uint32_t kSelection = 0x54;   // list widget: the row selected

// our pages, each a stock page's frame with a list in place of its own rows
struct List {
    std::string_view package, page, list, item, position;
    int rows;
};
// in P_Options' frame on its own rows (B_MainItem)
constexpr List kSectionList{"Options", "P_Options_RecompSections", "SectionList", "B_MainItem", "158 250", 3};
// in P_Options_Game's on ours, where its option rows were: 21 high from y 176, down to the frame's bottom
constexpr std::string_view kItem = "B_RecompItem";
constexpr List kRowList{"Options", "P_Options_Recomp", "RecompList", kItem, "50 176", 10};
// the popup over the rows: P_TeamsActionPopup's list at the map list popup's place (P_createMatchParamsPopup in
// entry, whose B_OptionItem xboxlive has too), on a background as that one's
constexpr List kChoiceList{"xboxlive", "P_Options_RecompPopup", "ChoiceList", "B_OptionItem", "50 267", 4};
constexpr std::string_view kPopupBackground[] = {"246", "404"};  // top, bottom
// the RECOMP SETTINGS sections: F10 headings, as story's
constexpr std::string_view kSections[] = {"DISPLAY", "GRAPHICS", "GENERAL"};
// our menus keep their page (0 sections, 1 rows, 2 popup), the button's mode and the section heading's row past the
// base menu's fields (P_Options_Game's own start at +0xC8)
constexpr uint32_t kMenuKind = 0x100, kMenuMode = 0x104, kMenuSection = 0x108;

struct Button {
    std::string_view name, label, title;
};
// labels in capitals as the stock rows, the page title as Options' own
constexpr Button kButtons[] = {{"RecompSettings", "RECOMP SETTINGS", "Recomp Settings"},
                               {"MouseKeyboard", "MOUSE AND KEYBOARD", "Mouse and Keyboard"},
                               {"Controller", "CONTROLLER", "Controller"}};

// Magma ids: crc32 of the lowercase name
constexpr uint32_t Id(std::string_view name) {
    uint32_t crc = ~0u;
    for (char c : name) {
        crc ^= uint8_t(c >= 'A' && c <= 'Z' ? c + 32 : c);
        for (int i = 0; i < 8; i++) crc = crc >> 1 ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
static_assert(Id("Online") == 0x9E32BEEA && Id("B_MainItem") == 3405425947u);

// game thread
uint32_t g_sections = 0;  // the sections menu open, for its list's press
struct Rows {
    uint32_t menu = 0, list = 0, value = 0;  // the rows menu open, its list widget and value column
    int mode = 0;
    std::vector<const app::rows::Row*> rows;  // per list row, kGap a blank row, null restores defaults
};
// a blank row between headings and before RESTORE DEFAULTS, as story's: the lists have no divider of their own
const app::rows::Row kGap{app::rows::Kind::Heading, -1, ""};
Rows g_rows;
struct Popup {
    uint32_t menu = 0;
    const app::rows::Row* row = nullptr;  // the row whose values it lists
    std::vector<std::string> choices;
    int current = 0;
};
Popup g_popup;

// ---- the package text

// the text between <tag> and </tag>, the first from `from`; false when there is none
bool SetTag(std::string& xml, std::string_view tag, std::string_view value, size_t from = 0) {
    std::string open = "<" + std::string(tag) + ">", close = "</" + std::string(tag) + ">";
    size_t a = xml.find(open, from);
    size_t b = a == std::string::npos ? a : xml.find(close, a);
    if (b == std::string::npos) return false;
    xml.replace(a + open.size(), b - a - open.size(), value);
    return true;
}

void SetEvery(std::string& xml, std::string_view tag, std::string_view value) {
    for (size_t at = 0; (at = xml.find("<" + std::string(tag) + ">", at)) != std::string::npos; at++) SetTag(xml, tag, value, at);
}

// every keyframe's POSITION "x y" moved down
void MoveDown(std::string& xml, int dy) {
    for (size_t at = xml.find("<POSITION>"); at != std::string::npos; at = xml.find("<POSITION>", at + 1)) {
        size_t start = at + 10, end = xml.find("</POSITION>", start), space = xml.find(' ', start);
        if (end == std::string::npos || space > end) return;
        int y = std::atoi(xml.c_str() + space + 1);
        xml.replace(space + 1, end - space - 1, std::to_string(y + dy));
    }
}

// the <tag> block holding <NAME>name</NAME>, from `from`: [begin, end)
bool Block(const std::string& xml, std::string_view tag, std::string_view name, size_t from, size_t& begin, size_t& end) {
    std::string open = "<" + std::string(tag) + ">", close = "</" + std::string(tag) + ">";
    size_t at = xml.find("<NAME>" + std::string(name) + "</NAME>", from);
    if (at == std::string::npos) return false;
    begin = xml.rfind(open, at);
    end = xml.find(close, at);
    if (begin == std::string::npos || begin < from || end == std::string::npos) return false;
    end += close.size();
    return true;
}

std::string_view NewLine(const std::string& xml) { return xml.find("\r\n") != std::string::npos ? "\r\n" : "\n"; }

// the keyframes of an element block whose IDX is one of `keep`
std::string KeepFrames(std::string block, std::initializer_list<int> keep) {
    for (size_t at = block.find("<KEYFRAME>"); at != std::string::npos;) {
        size_t end = block.find("</KEYFRAME>", at), idx = block.find("<IDX>", at);
        if (end == std::string::npos || idx == std::string::npos) break;
        end += std::strlen("</KEYFRAME>");
        int frame = std::atoi(block.c_str() + idx + 5);
        if (std::find(keep.begin(), keep.end(), frame) != keep.end()) {
            at = block.find("<KEYFRAME>", end);
            continue;
        }
        while (end < block.size() && (block[end] == '\r' || block[end] == '\n')) end++;
        block.erase(at, end - at);
        at = block.find("<KEYFRAME>", at);
    }
    return block;
}

// common: B_RecompItem, a button made of the option rows' CHK_Default (frames 0 enabled, 20 disabled, 30 selected,
// which are a button's too): its bar `back`, its label LBL_Text as ItemText and a copy of it on the right, ItemValue
bool SpliceItem(std::string& xml) {
    size_t begin = 0, end = 0, b0 = 0, b1 = 0, t0 = 0, t1 = 0;
    if (xml.find("<NAME>" + std::string(kItem) + "</NAME>") != std::string::npos || !Block(xml, "CheckBox", "CHK_Default", 0, begin, end))
        return false;
    std::string check = xml.substr(begin, end - begin);
    size_t children = check.find("<CHILDREN>");
    if (children == std::string::npos || !Block(check, "IMAGE", "back", 0, b0, b1) || !Block(check, "TEXT", "LBL_Text", 0, t0, t1))
        return false;
    std::string_view nl = NewLine(xml);
    std::string head = "<Button>" + check.substr(std::strlen("<CheckBox>"), children + std::strlen("<CHILDREN>") - std::strlen("<CheckBox>"));
    std::string back = KeepFrames(check.substr(b0, b1 - b0), {0, 20, 30});
    std::string label = KeepFrames(check.substr(t0, t1 - t0), {0, 20, 30}), value = label;
    if (!SetTag(head, "ID", std::to_string(Id(kItem))) || !SetTag(head, "NAME", kItem)) return false;
    SetTag(label, "ID", std::to_string(Id("ItemText")));
    SetTag(label, "NAME", "ItemText");
    SetEvery(label, "RIGHT", "400");
    SetTag(value, "ID", std::to_string(Id("ItemValue")));
    SetTag(value, "NAME", "ItemValue");
    SetEvery(value, "LEFT", "260");
    SetEvery(value, "RIGHT", "525");
    SetEvery(value, "ALIGNMENT", "RIGHT");
    std::string n(nl), item = head + n + back + n + label + n + value + n + "</CHILDREN>" + n + "<TIMINGS>" + n +
                            "<ENABLED>0</ENABLED>" + n + "<PRESSED>10</PRESSED>" + n + "<DISABLED>20</DISABLED>" + n +
                            "<SELECTED>30</SELECTED>" + n + "<OVER>40</OVER>" + n + "</TIMINGS>" + n + "</Button>";
    xml.insert(end, n + item);
    return true;
}

std::string ListXml(const List& l, std::string_view nl) {
    std::string keyframes;
    for (auto [idx, alpha] : {std::pair{0, 0}, std::pair{3, 255}})
        keyframes += "<KEYFRAME>\n<IDX>" + std::to_string(idx) + "</IDX>\n<INTERPOLATION>" + (idx ? "Linear" : "None") +
                     "</INTERPOLATION>\n<POSITION>" + std::string(l.position) + "</POSITION>\n<ROTATION>0</ROTATION>\n<STATECOLOR>255 255 255 " +
                     std::to_string(alpha) + "</STATECOLOR>\n<ACTIONS/>\n</KEYFRAME>\n";
    std::string list = "<LIST>\n<ID>" + std::to_string(Id(l.list)) + "</ID>\n<NAME>" + std::string(l.list) +
                       "</NAME>\n<HIDDEN>0</HIDDEN>\n<LOCKED>0</LOCKED>\n<MASKMODE>NOMASK</MASKMODE>\n<KEYFRAMES>\n" + keyframes +
                       "</KEYFRAMES>\n<KEYFRAMESOPT>1</KEYFRAMESOPT>\n<BUTTONCOUNT>" + std::to_string(l.rows) +
                       "</BUTTONCOUNT>\n<HEADERFOOTERPOS>Top and Bottom</HEADERFOOTERPOS>\n<ITEMLINK>\n<PACKAGE>common</PACKAGE>\n<AREA>" +
                       std::to_string(Id(l.item)) + "</AREA>\n</ITEMLINK>\n<HEADERLINK/>\n<FOOTERLINK/>\n<NEIGHBORS>\n<UP>NULL</UP>\n"
                       "<DOWN>NULL</DOWN>\n<LEFT>NULL</LEFT>\n<RIGHT>NULL</RIGHT>\n</NEIGHBORS>\n<DEFAULT>1</DEFAULT>\n</LIST>\n";
    std::string out;
    for (char c : list) {
        if (c == '\n') out += nl;
        else out += c;
    }
    return out;
}

// a copy of page `source` after it, without its option rows (buttons, checkboxes, the aiming slider with its label,
// value and bar) and with list `l` instead
bool AddPage(std::string& xml, std::string_view source, const List& l, std::string_view nl) {
    size_t at = xml.find("<NAME>" + std::string(source) + "</NAME>"), begin = 0, end = 0;
    size_t page_begin = at == std::string::npos ? at : xml.rfind("<Page>", at);
    size_t page_end = at == std::string::npos ? at : xml.find("</Page>", at);
    if (page_begin == std::string::npos || page_end == std::string::npos) return false;
    page_end += 7;
    std::string ours = xml.substr(page_begin, page_end - page_begin);
    auto drop = [&](std::string_view tag, std::string_view name) {
        while (name.empty() ? (begin = ours.find("<" + std::string(tag) + ">")) != std::string::npos : Block(ours, tag, name, 0, begin, end)) {
            end = ours.find("</" + std::string(tag) + ">", begin) + tag.size() + 3;
            if (ours.compare(end, nl.size(), nl) == 0) end += nl.size();
            ours.erase(begin, end - begin);
        }
    };
    drop("BUTTONINST", "");
    drop("CHECKBOXINST", "");
    drop("SLIDER", "");
    drop("TEXT", "Lbl_Aiming");
    drop("TEXT", "Lbl_AimingValue");
    drop("IMAGE", "back");
    if (!SetTag(ours, "ID", std::to_string(Id(l.page))) || !SetTag(ours, "NAME", l.page)) return false;
    size_t children = ours.rfind("</CHILDREN>");
    if (children == std::string::npos) return false;
    ours.insert(children, ListXml(l, nl));
    xml.insert(page_end, std::string(nl) + ours);
    return true;
}

// Options: our buttons after Online, and our pages
bool SpliceOptions(std::string& xml) {
    size_t page = xml.find("<NAME>P_Options</NAME>"), begin = 0, end = 0;
    if (page == std::string::npos || xml.find("<NAME>" + std::string(kRowList.page) + "</NAME>") != std::string::npos ||
        !Block(xml, "BUTTONINST", "Online", page, begin, end))
        return false;
    std::string online = xml.substr(begin, end - begin), added;
    std::string_view up = "Online";
    for (size_t i = 0; i < std::size(kButtons); i++) {
        std::string b = online;
        std::string_view down = i + 1 < std::size(kButtons) ? kButtons[i + 1].name : "Game";
        if (!SetTag(b, "ID", std::to_string(Id(kButtons[i].name))) || !SetTag(b, "NAME", kButtons[i].name) ||
            !SetTag(b, "LABEL", kButtons[i].label) || !SetTag(b, "UP", up) || !SetTag(b, "DOWN", down))
            return false;
        MoveDown(b, kRowStep * int(i + 1));
        added += b;
        up = kButtons[i].name;
    }
    if (!SetTag(online, "DOWN", kButtons[0].name)) return false;
    xml.replace(begin, end - begin, online + added);
    if (!Block(xml, "BUTTONINST", "Game", page, begin, end) || !SetTag(xml, "UP", kButtons[std::size(kButtons) - 1].name, begin))
        return false;

    std::string_view nl = NewLine(xml);
    if (!AddPage(xml, "P_Options_Game", kRowList, nl) || !AddPage(xml, "P_Options", kSectionList, nl)) return false;
    return true;
}

// xboxlive: our popup, a copy of P_TeamsActionPopup with its list where the map list's popup has it
bool SplicePopup(std::string& xml) {
    size_t at = xml.find("<NAME>P_TeamsActionPopup</NAME>"), begin = 0, end = 0;
    size_t page_begin = at == std::string::npos ? at : xml.rfind("<Page>", at);
    size_t page_end = at == std::string::npos ? at : xml.find("</Page>", at);
    if (page_begin == std::string::npos || page_end == std::string::npos ||
        xml.find("<NAME>" + std::string(kChoiceList.page) + "</NAME>") != std::string::npos)
        return false;
    page_end += 7;
    std::string ours = xml.substr(page_begin, page_end - page_begin);
    if (!SetTag(ours, "ID", std::to_string(Id(kChoiceList.page))) || !SetTag(ours, "NAME", kChoiceList.page) ||
        !Block(ours, "IMAGE", "fond", 0, begin, end))
        return false;
    std::string fond = ours.substr(begin, end - begin);
    SetEvery(fond, "TOP", kPopupBackground[0]);
    SetEvery(fond, "BOTTOM", kPopupBackground[1]);
    ours.replace(begin, end - begin, fond);
    if (!Block(ours, "LIST", "list", 0, begin, end)) return false;
    std::string list = ours.substr(begin, end - begin);
    if (!SetTag(list, "ID", std::to_string(Id(kChoiceList.list))) || !SetTag(list, "NAME", kChoiceList.list) ||
        !SetTag(list, "BUTTONCOUNT", std::to_string(kChoiceList.rows)))
        return false;
    SetEvery(list, "POSITION", kChoiceList.position);
    ours.replace(begin, end - begin, list);
    xml.insert(page_end, std::string(NewLine(xml)) + ours);
    return true;
}

// every <tag> closed by its </tag> in order (<tag/> stands alone): a splice gone wrong would send the game's
// parser past its first node
bool Balanced(std::string_view xml) {
    std::vector<std::string_view> open;
    for (size_t at = xml.find('<'); at != std::string_view::npos; at = xml.find('<', at + 1)) {
        size_t end = xml.find('>', at);
        if (end == std::string_view::npos) return false;
        std::string_view tag = xml.substr(at + 1, end - at - 1);
        if (tag.empty() || tag.back() == '/') continue;
        if (tag[0] != '/') {
            open.push_back(tag);
        } else {
            if (open.empty() || open.back() != tag.substr(1)) return false;
            open.pop_back();
        }
    }
    return open.empty();
}

// ---- text

std::u16string Caps(std::u16string text) {
    for (char16_t& c : text)
        if ((c >= u'a' && c <= u'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7)) c -= 0x20;
    return text;
}

std::u16string Translated(std::string_view text) {
    return app::Translate(text, [](std::string_view rest) { return std::string(rest); });
}

// a row's text in the game's language, in its title case
std::u16string Rows(std::string_view text) { return app::Translate(text, app::TitleCase); }

// a lasting guest copy: Magma keeps the pointers it is given, so each text is made once
uint32_t Guest(const std::u16string& text) {
    static std::map<std::u16string, uint32_t> guest;
    uint32_t& va = guest[text];
    if (!va) {
        va = mem::AllocSystem(uint32_t(text.size() + 1) * 2);
        for (size_t i = 0; i <= text.size(); i++) W16(va + uint32_t(i) * 2, i < text.size() ? uint16_t(text[i]) : 0);
    }
    return va;
}

// ---- the pages

std::u16string Titled(std::string_view text) {
    const char16_t* t = app::Translation(text);
    if (t) return t;
    std::string titled = app::TitleCase(text);
    return std::u16string(titled.begin(), titled.end());
}

// the row of heading `name` on the RECOMP SETTINGS rows
int Heading(std::string_view name) {
    size_t count = 0;
    const app::rows::Row* rows = app::rows::Page(0, &count);
    for (size_t i = 0; i < count; i++)
        if (rows[i].kind == app::rows::Kind::Heading && rows[i].label == name) return int(i);
    return -1;
}

uint32_t ListOf(X86Context& ctx, uint32_t page, std::string_view name) {
    ctx.ecx = page;
    uint32_t inst = page ? CallGuest(ctx, kFind, {Id(name)}) : 0;
    return inst ? R32(inst + kWidget) : 0;
}

// the page is built once with its package: the title is set for what opened it
void SetTitle(X86Context& ctx, uint32_t page, const std::u16string& text) {
    ctx.ecx = page;
    if (uint32_t title = page ? CallGuest(ctx, kFind, {Id("Title")}) : 0; title && R32(title + kWidget))
        W32(R32(title + kWidget) + kTextString, Guest(text));
}

void FillSections(X86Context& ctx, uint32_t menu) {
    uint32_t page = R32(menu + kPage), list = ListOf(ctx, page, kSectionList.list);
    SetTitle(ctx, page, Titled(kButtons[0].label));
    if (!list) {
        LOGI("versus options: no list on %s", std::string(kSectionList.page).c_str());
        return;
    }
    ctx.ecx = list;
    uint32_t label = CallGuest(ctx, kPart, {Id("ItemText")});
    ctx.ecx = list;
    CallGuest(ctx, kClear, {});
    // in capitals as the Options rows
    for (std::string_view name : kSections) {
        ctx.ecx = list;
        uint32_t row = CallGuest(ctx, kAddItem, {kEmpty, 0});
        ctx.ecx = list;
        CallGuest(ctx, kSetText, {row, label, Guest(Caps(Translated(name)))});
    }
    g_sections = menu;
}

// the rows' values, as P_Options_Game writes its rows; key and button names are not translated: the controller's
// BACK is no Back
void Values(X86Context& ctx) {
    using app::rows::Kind;
    if (!g_rows.list || g_rows.value >= 16) return;
    for (size_t i = 0; i < g_rows.rows.size(); i++) {
        if (!g_rows.rows[i] || g_rows.rows[i] == &kGap) {
            ctx.ecx = g_rows.list;
            CallGuest(ctx, kSetText, {uint32_t(i), g_rows.value, kEmpty});
            continue;
        }
        const app::rows::Row& r = *g_rows.rows[i];
        std::string v = app::rows::Value(r), key = app::TitleCase(v);
        std::u16string text = r.kind == Kind::Value || v == "UNBOUND" ? Rows(v) : std::u16string(key.begin(), key.end());
        ctx.ecx = g_rows.list;
        CallGuest(ctx, kSetText, {uint32_t(i), g_rows.value, Guest(text)});
    }
}

void FillRows(X86Context& ctx, uint32_t menu) {
    using app::rows::Kind;
    int mode = int(R32(menu + kMenuMode)), section = int(R32(menu + kMenuSection));
    uint32_t page = R32(menu + kPage), list = ListOf(ctx, page, kRowList.list);
    size_t count = 0, shown = 0;
    const app::rows::Row* rows = app::rows::Page(mode, &count);
    bool in_section = mode == 0 && section >= 0 && size_t(section) < count;
    SetTitle(ctx, page, Titled(in_section ? rows[section].label : kButtons[mode].label));
    if (!list) {
        LOGI("versus options: no list on %s", std::string(kRowList.page).c_str());
        return;
    }
    ctx.ecx = list;
    uint32_t label = CallGuest(ctx, kPart, {Id("ItemText")});
    ctx.ecx = list;
    uint32_t value = CallGuest(ctx, kPart, {Id("ItemValue")});
    ctx.ecx = list;
    CallGuest(ctx, kClear, {});
    g_rows = {menu, list, value, mode, {}};
    // a section's value rows up to the next heading; MOUSE AND KEYBOARD and CONTROLLER with a blank row for each
    // heading but the first, then a blank row and RESTORE DEFAULTS
    for (size_t i = in_section ? size_t(section) + 1 : 0; i < count; i++) {
        const app::rows::Row& r = rows[i];
        if (r.kind == Kind::Heading && in_section) break;
        if (r.kind == Kind::Heading && !g_rows.rows.empty()) g_rows.rows.push_back(&kGap);
        if (r.kind == Kind::Heading || r.kind == Kind::Text || (r.kind == Kind::Value && !app::rows::Shown(r))) continue;
        g_rows.rows.push_back(&r);
    }
    if (!in_section) {
        g_rows.rows.push_back(&kGap);
        g_rows.rows.push_back(nullptr);
    }
    for (const app::rows::Row* r : g_rows.rows) {
        ctx.ecx = list;
        uint32_t row = CallGuest(ctx, kAddItem, {kEmpty, 0});
        ctx.ecx = list;
        CallGuest(ctx, kSetText, {row, label, r == &kGap ? kEmpty : Guest(Rows(r ? r->label : "RESTORE DEFAULTS"))});
        shown++;
    }
    Values(ctx);
    LOGI("versus options: %s, %zu rows", in_section ? rows[section].label : std::string(kButtons[mode].label).c_str(), shown);
}

void FillPopup(X86Context& ctx, uint32_t menu) {
    g_popup.menu = menu;
    uint32_t list = ListOf(ctx, R32(menu + kPage), kChoiceList.list);
    if (!list) {
        LOGI("versus options: no list on %s", std::string(kChoiceList.page).c_str());
        return;
    }
    ctx.ecx = list;
    // xboxlive's B_OptionItem names its text ItemText (entry's, label)
    uint32_t label = CallGuest(ctx, kPart, {Id("ItemText")});
    ctx.ecx = list;
    CallGuest(ctx, kClear, {});
    // in capitals as the map list's popup
    for (const std::string& choice : g_popup.choices) {
        ctx.ecx = list;
        uint32_t row = CallGuest(ctx, kAddItem, {kEmpty, 0});
        ctx.ecx = list;
        CallGuest(ctx, kSetText, {row, label, Guest(Caps(Translated(choice)))});
    }
    ctx.ecx = list;
    CallGuest(ctx, kSelect, {uint32_t(g_popup.current), 1});
}

uint32_t Handler(int kind);

// our pages' open (thiscall menu): the base one, then the list and its handler
void MenuOpen(X86Context& __restrict ctx, uint8_t*) {
    uint32_t menu = ctx.ecx;
    CallGuest(ctx, kBaseOpen, {});
    uint32_t kind = R32(menu + kMenuKind);
    if (kind == 0) FillSections(ctx, menu);
    else if (kind == 1) FillRows(ctx, menu);
    else FillPopup(ctx, menu);
    uint32_t h = Handler(int(kind) + 3);
    W32(h + 4, R32(h + 4) + 1);  // the bind takes a counted reference
    ctx.ecx = menu;
    CallGuest(ctx, kBind, {h, Id(kind == 0 ? kSectionList.list : kind == 1 ? kRowList.list : kChoiceList.list)});
    ctx.esp += 4;
}

void ClosePopup(X86Context& ctx, uint32_t sound);
void Sound(X86Context& ctx, uint32_t sound);

// our menus' keys (thiscall menu (key, action, ?), ret 0xC): B goes back a page, as P_Options' does
void MenuKey(X86Context& __restrict ctx, uint8_t*) {
    uint32_t menu = ctx.ecx, key = R32(ctx.esp + 4), action = R32(ctx.esp + 8), handled = 0;
    if (key == kJoyB && action == kRelease) {
        if (R32(menu + kMenuKind) == 2) {
            ClosePopup(ctx, kBackSound);
        } else {
            Sound(ctx, kBackSound);
            CallGuest(ctx, kPopMenu, {});
        }
        handled = 1;
    }
    ctx.eax = handled;
    ctx.esp += 16;
}

uint32_t MenuVtable() {
    static uint32_t vtable = 0;
    if (!vtable) {
        vtable = mem::AllocSystem(kMenuSlots * 4);
        for (uint32_t i = 0; i < kMenuSlots; i++) W32(vtable + i * 4, R32(kMenuVtable + i * 4));
        W32(vtable + kOpenSlot * 4, dispatch::HostThunk(MenuOpen));
        W32(vtable + kLegendSlot * 4, kOptionsLegend);
        W32(vtable + kKeySlot * 4, dispatch::HostThunk(MenuKey));
    }
    return vtable;
}

// a guest copy of a page or package name, for its FString
uint32_t Name(std::string_view name) {
    static std::map<std::string_view, uint32_t> names;
    uint32_t& va = names[name];
    if (!va) {
        va = mem::AllocSystem(uint32_t(name.size()) + 1);
        for (size_t i = 0; i <= name.size(); i++) G<uint8_t>(va + uint32_t(i)) = i < name.size() ? uint8_t(name[i]) : 0;
    }
    return va;
}

// a base menu on one of our pages, pushed as the stock buttons push theirs
void OpenPage(X86Context& ctx, const List& l, int mode, int section) {
    static uint32_t names = mem::AllocSystem(0x18);  // the package and page FStrings
    uint32_t menu = CallGuest(ctx, kMalloc, {kMenuSize, 0});
    if (!menu) return;
    std::memset(HostPtr(menu), 0, kMenuSize);
    ctx.ecx = names;
    CallGuest(ctx, kFString, {Name(l.package)});
    ctx.ecx = names + 0xC;
    CallGuest(ctx, kFString, {Name(l.page)});
    ctx.ecx = menu;
    CallGuest(ctx, kMenuCtor, {names, names + 0xC});
    for (uint32_t s : {names, names + 0xC})
        if (uint32_t text = R32(s)) CallGuest(ctx, kFree, {text});
    W32(menu, MenuVtable());
    bool popup = &l == &kChoiceList;
    W32(menu + kMenuKind, &l == &kSectionList ? 0 : popup ? 2 : 1);
    W32(menu + kMenuMode, uint32_t(mode));
    W32(menu + kMenuSection, uint32_t(section));
    CallGuest(ctx, kPush, {menu, popup ? 0u : 1u, 1});
}

void Sound(X86Context& ctx, uint32_t sound) {
    if (uint32_t player = R32(kSoundPlayer)) {
        ctx.ecx = player;
        CallGuest(ctx, R32(R32(player) + 8), {sound});
    }
}

int Selected(uint32_t list) { return list ? int(R32(list + kSelection)) : -1; }

// A popup closing does not bring the page under it on top again as a page closing does (the page was never taken
// off), and its open hid the profile name and changed the legend: the rows menu's legend slot puts both back.
void ClosePopup(X86Context& ctx, uint32_t sound) {
    g_popup.row = nullptr;
    Sound(ctx, sound);
    CallGuest(ctx, kPopMenu, {});
    if (g_rows.menu) {
        ctx.ecx = g_rows.menu;
        CallGuest(ctx, kOptionsLegend, {0});
    }
}

// a row of the rows page, as story's: step 0 is A, which flips an on / off row, opens the popup on a row with a list
// and steps the others; Left / Right (step -1 / 1) step those
bool Act(X86Context& ctx, int step) {
    using app::rows::Kind;
    int row = Selected(g_rows.list);
    if (row < 0 || row >= int(g_rows.rows.size()) || g_rows.rows[size_t(row)] == &kGap) return false;
    if (!g_rows.rows[size_t(row)]) {
        if (step) return false;
        app::rows::Defaults(g_rows.mode);
        Values(ctx);
        Sound(ctx, kSelectSound);
        return true;
    }
    const app::rows::Row& r = *g_rows.rows[size_t(row)];
    if (r.kind != Kind::Value) return false;  // binding comes later
    int current = 0;
    std::vector<std::string> choices = app::rows::Choices(r, &current);
    bool flip = choices.size() == 2 && choices[0] == "OFF" && choices[1] == "ON";
    if (choices.size() > 1 && (!flip || step)) {
        if (step) return false;
        g_popup = {0, &r, std::move(choices), current};
        OpenPage(ctx, kChoiceList, 0, -1);
        return true;
    }
    app::rows::Step(r, step ? step : 1);
    Values(ctx);
    Sound(ctx, kSelectSound);
    return true;
}

// a press (thiscall handler, one argument, ret 4): the handler's +0x10 is what it opens, 0..2 our buttons, 3 the
// section picked on the sections list, 4 a row, 5 the value picked in the popup
void Press(X86Context& __restrict ctx, uint8_t*) {
    int kind = int(R32(ctx.ecx + 0x10));
    if (kind == 0) {
        OpenPage(ctx, kSectionList, 0, -1);
    } else if (kind < 3) {
        OpenPage(ctx, kRowList, kind, -1);
    } else if (kind == 3) {
        int row = Selected(g_sections ? ListOf(ctx, R32(g_sections + kPage), kSectionList.list) : 0);
        if (row >= 0 && row < int(std::size(kSections))) OpenPage(ctx, kRowList, 0, Heading(kSections[row]));
    } else if (kind == 4) {
        Act(ctx, 0);
    } else if (g_popup.row) {
        int pick = Selected(g_popup.menu ? ListOf(ctx, R32(g_popup.menu + kPage), kChoiceList.list) : 0);
        const app::rows::Row* r = g_popup.row;
        ClosePopup(ctx, kSelectSound);
        // the value it has would be applied again
        if (pick >= 0 && pick < int(g_popup.choices.size()) && pick != g_popup.current) app::rows::Choose(*r, pick);
        Values(ctx);
    }
    ctx.eax = 1;
    ctx.esp += 8;
}

// Up / Down past a blank row: from the row before it to the one after, or off it when it was selected anyway (the
// mouse); the list moves by itself otherwise
bool PastGap(X86Context& ctx, int dir) {
    int row = Selected(g_rows.list), rows = int(g_rows.rows.size());
    if (row < 0 || row >= rows) return false;
    auto gap = [&](int r) { return r >= 0 && r < rows && g_rows.rows[size_t(r)] == &kGap; };
    int to = gap(row) ? row + dir : gap(row + dir) ? row + 2 * dir : -1;
    if (to < 0 || to >= rows) return false;
    ctx.ecx = g_rows.list;
    CallGuest(ctx, kSelect, {uint32_t(to), 1});
    return true;
}

// an event on our lists (thiscall handler, ?, event*, ret 8): Left / Right step a row, Up / Down pass the blank rows;
// our buttons leave theirs to P_Options
void Event(X86Context& __restrict ctx, uint8_t*) {
    int kind = int(R32(ctx.ecx + 0x10));
    uint32_t event = R32(ctx.esp + 8), key = event ? R32(event) : 0, handled = 0;
    if (kind == 4 && (key == kLeft || key == kRight)) {
        handled = Act(ctx, key == kLeft ? -1 : 1);
    } else if (kind == 4 && (key == kUp || key == kDown)) {
        handled = PastGap(ctx, key == kUp ? -1 : 1);
    }
    ctx.eax = handled;
    ctx.esp += 12;
}

// the handlers, laid out as the Game button's {vtable, refcount, 1, 0x3380E0} plus what they open; never released
uint32_t Handler(int kind) {
    constexpr uint32_t kHandlers = 6, kSize = 0x18;
    static uint32_t handlers = 0;
    if (!handlers) {
        uint32_t vtable = mem::AllocSystem(kHandlerSlots * 4 + kSize * kHandlers);
        for (uint32_t i = 0; i < kHandlerSlots; i++) W32(vtable + i * 4, R32(kHandlerVtable + i * 4));
        W32(vtable + kPressSlot * 4, dispatch::HostThunk(Press));
        W32(vtable + kEventSlot * 4, dispatch::HostThunk(Event));
        handlers = vtable + kHandlerSlots * 4;
        for (uint32_t i = 0; i < kHandlers; i++) {
            uint32_t h = handlers + i * kSize;
            W32(h, vtable);
            W32(h + 4, 0x10000);
            W32(h + 8, 1);
            W32(h + 0xC, kHandlerTail);
            W32(h + 0x10, i);
        }
    }
    return handlers + uint32_t(kind) * kSize;
}

}  // namespace

namespace versus {

// xbox_live.cpp's text lookup: our buttons' labels, in capitals as the stock rows, and our pages' titles until they
// open; 0 for the rest
uint32_t OptionsText(uint32_t section, uint32_t section2, uint32_t key) {
    auto in = [&](uint32_t page) { return section == page || section2 == page; };
    if ((in(Id(kRowList.page)) || in(Id(kSectionList.page))) && key == Id("Title")) return Guest(Titled(kButtons[0].label));
    if (!in(kOptionsPage)) return 0;
    for (const Button& b : kButtons)
        if (key == Id(b.name)) return Guest(Caps(Translated(b.label)));
    return 0;
}

}  // namespace versus

// the XML reader's parse (thiscall reader, text, ret 4): the text is copied in, so ours is freed after
X86_EXTERN_FUNC(__imp__sub_0026F0B0);
X86_FUNC(sub_0026F0B0) {
    uint32_t text = R32(ctx.esp + 4), copy = 0;
    if (text) {
        std::string_view view(static_cast<const char*>(HostPtr(text)));
        bool options = view.find("<NAME>P_Options</NAME>") != std::string_view::npos;
        bool common = view.find("<NAME>B_MainItem</NAME>") != std::string_view::npos;
        bool xboxlive = view.find("<NAME>P_TeamsActionPopup</NAME>") != std::string_view::npos;
        const char* name = options ? "Options" : common ? "common" : "xboxlive";
        if (options || common || xboxlive) {
            std::string xml(view);
            bool changed = (options && SpliceOptions(xml)) | (common && SpliceItem(xml)) | (xboxlive && SplicePopup(xml));
            if (changed && !Balanced(xml)) {
                LOGE("versus options: the spliced %s does not balance, the stock text is kept", name);
                changed = false;
            }
            if (changed) copy = CallGuest(ctx, kMalloc, {uint32_t(xml.size() + 1), 0});
            if (copy) {
                std::memcpy(HostPtr(copy), xml.c_str(), xml.size() + 1);
                W32(ctx.esp + 4, copy);
            }
            LOGI("versus options: %s %s", name, copy ? "spliced" : "not changed");
        }
    }
    __imp__sub_0026F0B0(ctx, base);
    if (copy) {
        uint32_t result = ctx.eax;
        CallGuest(ctx, kFree, {copy});
        ctx.eax = result;
    }
}

// P_Options' open (thiscall menu): the stock buttons get their handlers, ours theirs
X86_EXTERN_FUNC(__imp__sub_001AD030);
X86_FUNC(sub_001AD030) {
    uint32_t menu = ctx.ecx;
    __imp__sub_001AD030(ctx, base);
    for (int i = 0; i < int(std::size(kButtons)); i++) {
        uint32_t h = Handler(i);
        W32(h + 4, R32(h + 4) + 1);  // the bind takes a counted reference
        ctx.ecx = menu;
        CallGuest(ctx, kBind, {h, Id(kButtons[i].name)});
    }
}
