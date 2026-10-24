#include "game/trust.h"
#include "game/trust_model.h"
#include "game/trust_rules.h"
#include "game/unreal.h"
#include "game/magma.h"
#include "game/console.h"
#include "core/mod_events.h"
#include "app/app.h"
#include "app/translations.h"
#include "xdk/hle.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <iterator>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cmath>
#define NOMINMAX
#include <windows.h>

using namespace kernel;
namespace game::trust {
namespace {
std::recursive_mutex lock;
State state;
uint32_t player = 0;
bool active = false, loading = false, legacy = false, suppress = false;
std::string objective, last;
int seen = 0, visible = 0;
int opsat_read = -1;
Pair opsat_levels{-1, -1};
Pair review_value, review_delta;
bool review_ready = false;
Pair popup_delta;
std::chrono::steady_clock::time_point popup_changed{}, popup_until{};
std::string Lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}
std::string Wide(uint32_t p, unsigned limit = 256) {
    std::string s;
    if (!p) return s;
    for (unsigned i = 0; i < limit; ++i) {
        auto c = R16(p + i * 2);
        if (!c) break;
        s += c < 128 ? char(c) : '?';
    }
    return s;
}
std::string FString(uint32_t p) {
    int n = int(R32(p + 4));
    return n > 0 && n < 1024 ? Wide(R32(p), unsigned(n)) : "";
}
std::filesystem::path Handover() {
    return app::ExeDir() / ("trust-handover-" + std::to_string(app::Instance()) + ".txt");
}
bool Read(const std::filesystem::path& path) {
    std::ifstream f(path);
    State next;
    int version = 0;
    bool pending = false;
    if (!(f >> version >> next.value.nsa >> next.value.jba >> pending
          >> std::quoted(next.map) >> std::quoted(next.mission)) || (version < 1 || version > 3) ||
        next.value.nsa < 0 || next.value.nsa > 100 || next.value.jba < 0 || next.value.jba > 100) return false;
    if (version >= 2) { if (!(f >> next.dispatched) || next.dispatched < 0 || next.dispatched > 100) return false; }
    else next.dispatched = CrisisValue(next.value);
    bool initialized = true, import_legacy = false;
    if (version >= 3 && !(f >> initialized >> import_legacy)) return false;
    std::string key;
    Credit credit;
    f >> std::ws;
    while (f.peek() != std::char_traits<char>::eof()) {
        if (!(f >> std::quoted(key) >> credit.requested.nsa >> credit.requested.jba
                >> credit.effective.nsa >> credit.effective.jba)) return false;
        if (next.credits.size() >= 4096) return false;
        next.credits[key] = credit;
        f >> std::ws;
    }
    if (!f.eof()) return false;
    next.initialized = initialized;
    state = std::move(next);
    loading = pending;
    legacy = import_legacy;
    return true;
}
void Write(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    auto temp = path; temp += ".tmp";
    std::ofstream f(temp, std::ios::trunc);
    f << "3 " << state.value.nsa << ' ' << state.value.jba << ' ' << loading << '\n'
      << std::quoted(state.map) << ' ' << std::quoted(state.mission) << '\n' << state.dispatched << '\n'
      << state.initialized << ' ' << legacy << '\n';
    for (auto& [key, c] : state.credits)
        f << std::quoted(key) << ' ' << c.requested.nsa << ' ' << c.requested.jba
          << ' ' << c.effective.nsa << ' ' << c.effective.jba << '\n';
    f.flush();
    bool ok = bool(f);
    f.close();
    if (ok) {
        // Replace atomically: a failed write leaves the last complete sidecar intact.
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            LOGW("trust: could not replace sidecar %s", path.string().c_str());
    } else LOGW("trust: could not write sidecar %s", path.string().c_str());
}
void Boot() {
    static bool booted = false;
    if (booted) return;
    booted = true;
    if (std::wstring(GetCommandLineW()).find(L"--relaunched") != std::wstring::npos) Read(Handover());
    else { std::error_code ec; std::filesystem::remove(Handover(), ec); }
}
void Changed(std::string_view why, Pair delta) {
    if (delta.nsa || delta.jba) {
        auto now = std::chrono::steady_clock::now();
        // Reversing an exclusive choice and awarding its replacement form one
        // notification. Show the actual net change, including saturation.
        if (now - popup_changed > std::chrono::milliseconds(250)) popup_delta = {};
        popup_delta.nsa += delta.nsa;
        popup_delta.jba += delta.jba;
        popup_changed = now;
        popup_until = now + std::chrono::seconds(5);
    }
    if (state.value.nsa <= 20 && state.value.nsa - delta.nsa > 20) state.credits.erase(Key(state, "recovery-nsa"));
    if (state.value.jba <= 20 && state.value.jba - delta.jba > 20) state.credits.erase(Key(state, "recovery-jba"));
    last = std::string(why);
    LOGI("trust: %s NSA %+d JBA %+d => %d/%d", last.c_str(), delta.nsa, delta.jba,
         state.value.nsa, state.value.jba);
}
void Award(std::string_view id, Pair delta) {
    auto key = Key(state, id);
    if (!state.Has(key)) Changed(id, state.Once(key, delta));
}
void Undo(std::string_view id) {
    auto key = Key(state, id);
    if (state.Has(key)) Changed("revoke " + std::string(id), state.Revoke(key));
}
void FinishMission() {
    // The JBA discovers sabotage at the outcome, rather than when Sam acts covertly.
    if (state.Has(Key(state, "chipcellphone"))) Award("bomb-discovered", {0, -30});
    if (state.Has(Key(state, "obj_hqb_p_30"))) Award("nashville-discovered", {0, -15});
    if (state.Has(Key(state, "obj_hqb_p_31"))) Award("la-discovered", {0, -15});
    // Hisham's successful fake-death branch cancels the body objective itself.
    // Missing completion is not evidence that Emile discovered the rescue.
}
void Observe(uint32_t pc) {
    Boot();
    if (!pc || !unreal::IsA(pc, unreal::Name("EPlayerController"))) return;
    uint32_t level = R32(pc + 0x270);
    std::string map = level ? Lower(FString(level + 0x5C)) : "";
    if (auto at = map.find('.'); at != std::string::npos) map.resize(at);
    if (auto at = map.find_last_of("/\\"); at != std::string::npos) map.erase(0, at + 1);
    active = map.starts_with("x") && map.find("coop") == std::string::npos &&
             !unreal::IsA(pc, unreal::Name("ECoopPlayerController"));
    if (!active) return;
    player = pc;
    if (!state.initialized) {
        if (legacy) {
            state.value = {int(G<uint8_t>(pc + 0x360)), 100 - int(G<uint8_t>(pc + 0x360))};
            state.dispatched = CrisisValue(state.value); // Existing checkpoint already owns its running warnings.
        }
        state.initialized = true;
        legacy = false;
        LOGI("trust: initialized %d/%d", state.value.nsa, state.value.jba);
    }
    auto mission = Mission(map);
    if (!loading && map == "x01_iceland_a" && !state.mission.empty() && state.mission != mission) {
        state = State{}; state.initialized = true;
    }
    if (!state.mission.empty() && state.mission != mission) FinishMission();
    state.map = map; state.mission = mission;
    G<uint8_t>(pc + 0x360) = uint8_t(state.value.nsa);
}
void Event(std::string id) {
    id = Lower(id);
    if (id.starts_with("temp")) { LOGI("trust: ignored debug objective %s", id.c_str()); return; }
    if (id == "p_trustnsa" || id == "p_trustnsahq2") {
        if (state.value.nsa <= 20) Award("recovery-nsa", {25, 0});
        return;
    }
    if (id == "p_trustjba" || id == "trustnow") {
        if (state.value.jba <= 20) Award("recovery-jba", {0, 25});
        return;
    }
    // Opposite outcomes replace their actual prior contribution, including saturation.
    constexpr std::pair<std::string_view, std::string_view> choices[] = {
        {"barnhamalive", "barnhamdead"}, {"p_chiefalive", "p_killchief"},
        {"yeagerlambert", "yeageremile"}, {"p_lambertcover", "p_lambertnotcover"},
        {"chipcellphone", "procellphone"}, {"obj_10_p_30", "obj_10_p_60"}};
    for (auto [a, b] : choices) {
        if (id == a) Undo(b);
        if (id == b) Undo(a);
    }
    if (id == "hishambody" || id == "obj_10_p_60") Undo("hisham-no-proof");
    if (id == "obj_10_p_60") Undo("hisham-exposed");
    if (id == "hishambody" && state.mission == "x10_kinshasa") {
        if (!state.Has(Key(state, "hishambody")) && !state.Has(Key(state, "hisham-exposed")))
            Changed(id, GrantHishamProof(state));
        return;
    }
    if (id == "procellphone") Undo("bomb-discovered");
    if (id == "keepbombs") {
        for (auto old : {"obj_hqb_p_30", "obj_hqb_p_31", "nashville-discovered", "la-discovered"}) Undo(old);
    }
    if (id == "obj_hqb_p_30" || id == "obj_hqb_p_31") Undo("keepbombs");
    for (auto& r : kRules) if (r.mission == state.mission && r.id == id) {
        Pair delta{r.nsa, r.jba};
        if (id == "chipcellphone" || id == "obj_hqb_p_30" || id == "obj_hqb_p_31") delta.jba = 0;
        Award(id, delta);
        return;
    }
    LOGI("trust: unweighted objective %s in %s", id.c_str(), state.map.c_str());
}
std::array<int, 11> StatisticsCounts(uint32_t pc) {
    uint32_t container = R32(pc + 0x710);
    if (!container) return {};
    std::array<int, 11> count{};
    uint32_t records = R32(container + 0xA8), n = R32(container + 0xAC);
    if (n > 100000 || (!records && n)) return {};
    for (uint32_t i = 0; i < n; ++i) {
        auto type = G<uint8_t>(records + i * 0x14);
        if (type < 11) ++count[type];
    }
    // These named deaths already carry a story consequence.
    for (auto id : {"barnhamdead", "p_killchief", "obj_10_p_60"})
        if (state.Has(Key(state, id))) {
            if (count[5]) --count[5]; else if (count[2]) --count[2];
        }
    // Exempt mandatory NY kills without discarding the other enabled rules.
    if (state.mission == "x12_ny") count[2] = count[5] = 0;
    return count;
}
Pair StatisticsDelta(uint32_t pc) {
    uint32_t info = R32(pc + 0xF4);
    if (!info) return {};
    unsigned enabled = 0;
    for (unsigned i = 0; i < 5; ++i) if (R32(info + 0xC60 + i * 4) & 1) enabled |= 1u << i;
    return StatisticsScore(StatisticsCounts(pc), enabled);
}
void Statistics(uint32_t pc) {
    Award("statistics-" + state.map, StatisticsDelta(pc));
}
void CellText(X86Context& ctx, uint32_t out, std::u16string_view text) {
    auto saved = ctx;
    uint32_t scratch = (ctx.esp - 512) & ~15u;
    size_t n = std::min<size_t>(text.size(), 200);
    for (size_t i = 0; i <= n; ++i) W16(scratch + uint32_t(i) * 2, i < n ? text[i] : 0);
    ctx.esp = scratch - 32; ctx.ecx = out; CallGuest(ctx, 0xD28EC, {scratch});
    ctx = saved;
}
void Files(const std::filesystem::path& path, bool written, uint64_t bytes) {
    std::lock_guard guard(lock);
    Boot();
    auto name = Lower(path.filename().string());
    if (!(name == "autosave.sav" || (name.starts_with("single") && name.ends_with(".sav") &&
          name.find("image") == std::string::npos))) return;
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec || size < 1000000 || (!written && (!loading || bytes < 65536))) return;
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    if (data.size() != size) return;
    std::string location = Lower(path.lexically_relative(app::ExeDir()).generic_string());
    std::vector<uint8_t> location_bytes(location.begin(), location.end());
    // Keep host metadata out of Xbox cache/profile enumeration and cleanup.
    auto sidecar = app::ExeDir() / "trust-state" / name / app::Sha1(location_bytes) / (app::Sha1(data) + ".txt");
    if (written) {
        if (active && state.initialized) Write(sidecar);
    } else {
        if (!Read(sidecar)) { state = State{}; legacy = true; LOGW("trust: legacy checkpoint imported on first player tick"); }
        loading = false;
        player = 0;
        LOGI("trust: restored checkpoint %s", sidecar.string().c_str());
    }
}
// Original-style faction strips: recessed ivory segments, olive metal and seals.
void TrustText(ImDrawList* d, ImVec2 p, float size, ImU32 color, const char* text) {
    int first = d->VtxBuffer.Size;
    int shadow_alpha = int((color >> IM_COL32_A_SHIFT) & 255) * 220 / 255;
    d->AddText(ImGui::GetFont(), size, {p.x + 1, p.y + 1}, IM_COL32(0, 0, 0, shadow_alpha), text);
    d->AddText(ImGui::GetFont(), size, p, color, text);
    for (int i = first; i < d->VtxBuffer.Size; ++i)
        d->VtxBuffer[i].pos.x += (p.y + size - d->VtxBuffer[i].pos.y) * .16f;
}
void FactionRow(ImDrawList* d, ImVec2 p, float width, float height, float scale,
                const char* camp, int value, int delta, bool show_delta, float age) {
    auto point = [&](float x, float y) { return ImVec2(p.x + x * scale, p.y + y * scale); };
    float w = width / scale, h = height / scale;
    ImU32 ivory = IM_COL32(222, 222, 182, 255);
    d->AddRectFilled(point(0, 0), point(w, h), IM_COL32(9, 13, 10, 245), 4 * scale);
    d->AddRectFilledMultiColor(point(2, 2), point(w - 2, h - 2),
        IM_COL32(104, 109, 86, 255), IM_COL32(85, 90, 71, 255),
        IM_COL32(34, 40, 31, 255), IM_COL32(45, 51, 39, 255));
    // Fine deterministic machining marks give the frame texture without assets.
    for (int y = 4; y < int(h) - 3; y += 3)
        d->AddLine(point(4, float(y)), point(w - 4, float(y)), IM_COL32(197, 197, 151, y % 2 ? 10 : 18));
    d->AddRect(point(1, 1), point(w - 1, h - 1), IM_COL32(187, 189, 151, 255), 3 * scale);
    d->AddRect(point(4, 4), point(w - 4, h - 4), IM_COL32(20, 27, 20, 255), 2 * scale);
    for (float x : {8.f, w - 8}) {
        for (float y : {8.f, h - 8}) {
            d->AddCircleFilled(point(x, y), 2.8f * scale, IM_COL32(13, 18, 13, 255), 12);
            d->AddLine(point(x - 1, y - 1), point(x + 1, y + 1), IM_COL32(144, 149, 117, 255));
        }
    }
    float radius = h * .41f * scale, cx = h * .53f, cy = h * .5f;
    d->AddCircleFilled(point(cx + 1, cy + 2), radius + 2 * scale, IM_COL32(0, 0, 0, 190), 48);
    d->AddCircleFilled(point(cx, cy), radius, IM_COL32(43, 49, 36, 255), 48);
    d->AddCircle(point(cx, cy), radius, ivory, 48, 1.5f * scale);
    d->AddCircle(point(cx, cy), radius - 3 * scale, IM_COL32(112, 120, 88, 255), 48, scale);
    d->AddCircleFilled(point(cx, cy), radius - 5 * scale, IM_COL32(13, 19, 13, 255), 48);
    TrustText(d, point(cx - 16, cy - 11), 20 * scale, ivory, camp);
    // Keep the scale identical for both camps, even when only one has a delta.
    float left = h + 8, right = w - 60, top = h * .28f, bottom = h * .72f;
    d->AddRectFilled(point(left - 4, top - 4), point(right + 4, bottom + 4), IM_COL32(12, 17, 11, 255));
    d->AddRect(point(left - 4, top - 4), point(right + 4, bottom + 4), IM_COL32(143, 149, 112, 255));
    constexpr int cells = 20;
    float pitch = (right - left) / cells;
    float progress = age < 0 ? 1.f : std::clamp((age - .15f) / 1.25f, 0.f, 1.f);
    progress = progress * progress * (3 - 2 * progress);
    float previous = float(std::clamp(value - delta, 0, 100));
    float animated = show_delta ? previous + (value - previous) * progress : float(value);
    ImU32 bright = value <= 20 ? IM_COL32(210, 140, 111, 255) : IM_COL32(232, 233, 184, 255);
    ImU32 dark = value <= 20 ? IM_COL32(113, 57, 43, 255) : IM_COL32(115, 124, 77, 255);
    for (int i = 0; i < cells; ++i) {
        float x = left + i * pitch;
        d->AddRectFilledMultiColor(point(x, top), point(x + pitch - 2, bottom),
            IM_COL32(49, 56, 43, 255), IM_COL32(44, 51, 40, 255), IM_COL32(23, 29, 23, 255), IM_COL32(27, 33, 25, 255));
        float fraction = std::clamp(animated / 5.f - i, 0.f, 1.f);
        if (fraction > 0) d->AddRectFilledMultiColor(point(x, top), point(x + (pitch - 2) * fraction, bottom), bright, bright, dark, dark);
        if (show_delta && age >= 0 && age < 2.5f) {
            float low = std::min(previous, float(value)) / 5.f;
            float high = std::max(previous, float(value)) / 5.f;
            float begin = std::clamp(low - i, 0.f, 1.f), end = std::clamp(high - i, 0.f, 1.f);
            if (end > begin) {
                int alpha = int(150 * std::clamp((2.5f - age) / 1.2f, 0.f, 1.f) * (.8f + .2f * std::sin(age * 10)));
                ImU32 highlight = delta < 0 ? IM_COL32(230, 111, 78, alpha) : IM_COL32(215, 240, 149, alpha);
                d->AddRectFilled(point(x + (pitch - 2) * begin, top), point(x + (pitch - 2) * end, bottom), highlight);
            }
        }
    }
    char number[12]; std::snprintf(number, sizeof(number), "%d", value);
    TrustText(d, point(right + 14, cy - 12), 23 * scale, ivory, number);
    if (show_delta) {
        char change[20]; std::snprintf(change, sizeof(change), "%+d", delta);
        float fade = age < 0 ? 1.f : std::clamp((5.f - age) / .65f, 0.f, 1.f);
        float lift = age < 0 ? 0 : 3 * std::min(age / 1.4f, 1.f);
        float text_width = ImGui::GetFont()->CalcTextSizeA(24 * scale, 1000, 0, change).x / scale;
        // Place the label immediately after the higher pre/post-change level.
        float maximum = std::max(previous, float(value));
        float center = left + (right - left) * maximum / 100.f + 6 + text_width * .5f;
        float margin = text_width * .5f + 9;
        center = std::clamp(center, left + margin, right - margin);
        d->PushClipRect(point(left, top - 2), point(right, bottom + 2), true);
        d->AddRectFilled(point(center - text_width * .5f - 7, cy - 14 - lift),
                         point(center + text_width * .5f + 8, cy + 13 - lift), IM_COL32(8, 13, 8, int(225 * fade)), 2 * scale);
        TrustText(d, point(center - text_width * .5f, cy - 12 - lift), 24 * scale,
            delta < 0 ? IM_COL32(242, 149, 118, int(255 * fade)) : IM_COL32(224, 241, 179, int(255 * fade)), change);
        d->PopClipRect();
    }
}
void DrawUi() {
    std::unique_lock guard(lock, std::try_to_lock);
    if (!guard.owns_lock()) return;
    bool popup = std::chrono::steady_clock::now() < popup_until && (popup_delta.nsa || popup_delta.jba);
    if (!active || app::g_hide_hud || app::ConsoleOpen() || app::MenuOpen()) return;
    if (visible < 2 && !popup) return;
    int mode = visible >= 2 ? visible : 1;
    auto& io = ImGui::GetIO();
    float scale = std::max(0.65f, io.DisplaySize.y / 720.f);
    float width = std::min((mode == 1 ? 410.f : 690.f) * scale, io.DisplaySize.x - 32 * scale);
    ImVec2 pos = mode == 1 ? ImVec2(24 * scale, io.DisplaySize.y - 220 * scale)
        : ImVec2((io.DisplaySize.x - width) * .5f, io.DisplaySize.y * (mode == 2 ? .28f : .55f));
    auto* d = ImGui::GetForegroundDrawList();
    Pair shown = mode == 3 && review_ready ? review_value : state.value;
    if (mode >= 2) {
        // Cover all of the native shared meter while retaining the OPSAT shell.
        float height = (mode == 3 ? 195 : 180) * scale;
        d->AddRectFilled(pos, {pos.x + width, pos.y + height}, IM_COL32(9, 15, 11, 255));
    }
    int row = 0;
    for (int i = 0; i < 2; ++i) {
        int delta = i ? popup_delta.jba : popup_delta.nsa;
        if (mode == 1 && !delta) continue;
        bool show_delta = mode == 3 ? review_ready && (i ? review_delta.jba : review_delta.nsa) != 0 : popup && delta != 0;
        int change = mode == 3 ? (i ? review_delta.jba : review_delta.nsa) : delta;
        float age = popup && change == delta ? std::chrono::duration<float>(std::chrono::steady_clock::now() - popup_changed).count() : -1.f;
        float inset = mode == 1 ? 0 : 8 * scale;
        ImVec2 start{pos.x + inset, pos.y + (mode == 1 ? row * 68 : 18 + row * 76) * scale};
        FactionRow(d, start, width - inset * 2, (mode == 1 ? 60 : 64) * scale,
                   scale, i ? "JBA" : "NSA", i ? shown.jba : shown.nsa, change, show_delta, age);
        ++row;
    }
}

struct Register {
    Register() {
        mod_events::draw_ui = DrawUi;
        mod_events::file_closed = Files;
        mod_events::relaunch = [] {
            std::lock_guard guard(lock); Boot();
            // A title-screen load must carry its pending-load flag even before a
            // playable controller has initialized either independent score.
            Write(Handover());
        };
    }
} registration;
} // namespace

void Tick(X86Context& ctx, uint32_t viewport) {
    auto saved = ctx;
    uint32_t pc = R32(viewport + 0x34);
    if (pc) {
        std::lock_guard guard(lock);
        Observe(pc);
    }
    bool dispatch = false;
    {
        std::lock_guard guard(lock);
        dispatch = active && player && CrisisValue(state.value) != state.dispatched;
    }
    if (dispatch) { ctx.ecx = player; CallGuest(ctx, 0x92413, {0}); }
    ctx = saved;
}
void Page(uint32_t id) {
    std::lock_guard guard(lock);
    if (id == magma::Id("P_Trust")) seen = std::max(seen, 2);
    if (id == magma::Id("P_TrustReview")) seen = 3;
}
bool Element(uint32_t id) {
    std::lock_guard guard(lock);
    if (!active) return false;
    if (id != magma::Id("AInst_Trust") && id != magma::Id("A_Trust") && id != magma::Id("A_trust")) return false;
    seen = std::max(seen, 1);
    return true;
}
void EndFrame() { std::lock_guard guard(lock); visible = seen; seen = 0; }
void Localized(uint32_t section, uint32_t key) {
    std::lock_guard guard(lock);
    if (!active || state.mission != "x10_kinshasa" || Lower(Wide(section)) != "p_x10_kinshasa_b_comms") return;
    auto line = Lower(Wide(key));
    // These are script-selected dialogue branches, evaluated as Localize() args
    // before speech playback, independently of the subtitles visibility setting.
    if (line == "speech_0050l") Changed("hisham witnessed escaping", RecordHishamEvidence(state, HishamEvidence::Exposed));
    else if (line == "speech_0024l") RecordHishamEvidence(state, HishamEvidence::Accepted);
}
bool Command(std::string_view command, std::string& output) {
    if (!command.starts_with("trust ")) return false;
    std::lock_guard guard(lock);
    std::istringstream in(std::string{command.substr(6)});
    std::string verb; in >> verb;
    if (verb == "set") { int n, j; if (in >> n >> j) state.value = {std::clamp(n, 0, 100), std::clamp(j, 0, 100)}; }
    else if (verb == "event") { std::string id; if (in >> id && active) Event(id); }
    else if (verb == "preview") visible = 2;
    if (active && player) G<uint8_t>(player + 0x360) = uint8_t(state.value.nsa);
    output = "NSA " + std::to_string(state.value.nsa) + "/100; JBA " + std::to_string(state.value.jba) +
             "/100; " + state.map + "; events " + std::to_string(state.credits.size()) + "; " + last;
    return true;
}
} // namespace game::trust

// Keep the guest's objective bookkeeping and notifications; replace only its trust effect.
X86_EXTERN_FUNC(__imp__sub_000E9CE4);
X86_FUNC(sub_000E9CE4) {
    using namespace game::trust;
    std::string saved = objective;
    {
        std::lock_guard guard(lock);
        Observe(R32(ctx.ecx + 0xA0));
        objective = active && R32(R32(ctx.esp + 8)) == 1 ? Lower(FString(R32(ctx.esp + 4))) : "";
    }
    __imp__sub_000E9CE4(ctx, base);
    objective = saved;
}
X86_EXTERN_FUNC(__imp__sub_00092413);
X86_FUNC(sub_00092413) {
    using namespace game::trust;
    std::unique_lock guard(lock);
    Observe(ctx.ecx);
    if (!active) { guard.unlock(); return __imp__sub_00092413(ctx, base); }
    int delta = int(R32(ctx.esp + 4));
    uint32_t pc = ctx.ecx;
    if (!suppress && !objective.empty()) Event(objective);
    else if (!suppress && R32(ctx.esp) == 0x001BB92F) Statistics(pc);
    else if (!suppress) LOGI("trust: ignored unclassified legacy delta %d (caller %08X)", delta, R32(ctx.esp));
    // The original event also dispatches crisis notifications. Feed a neutral value
    // unless one independent camp is actually low, then restore the NSA mirror.
    int target = CrisisValue(state.value);
    G<uint8_t>(pc + 0x360) = uint8_t(state.dispatched);
    W32(ctx.esp + 4, target - state.dispatched);
    state.dispatched = target;
    guard.unlock();
    __imp__sub_00092413(ctx, base);
    guard.lock();
    G<uint8_t>(pc + 0x360) = uint8_t(state.value.nsa);
}
X86_EXTERN_FUNC(__imp__sub_001CEB87);
X86_FUNC(sub_001CEB87) {
    using namespace game::trust;
    {
        std::lock_guard guard(lock);
        Boot();
        auto command = Lower(Wide(R32(ctx.esp + 4), 1024));
        if (command.starts_with("loadgame ") || command.find(" savegame=") != std::string::npos) loading = true;
    }
    __imp__sub_001CEB87(ctx, base);
}

// OPSAT originally obtains NSA's level twice, then displays 4-level for JBA.
// Feed JBA's own level into that second read while retaining localized wording.
X86_EXTERN_FUNC(__imp__sub_0019890F);
X86_FUNC(sub_0019890F) {
    using namespace game::trust;
    int saved = opsat_read;
    opsat_read = 0;
    __imp__sub_0019890F(ctx, base);
    opsat_read = saved;
}
X86_EXTERN_FUNC(__imp__sub_001429C3);
X86_FUNC(sub_001429C3) {
    using namespace game::trust;
    uint32_t pc = ctx.ecx;
    int index = opsat_read >= 0 ? opsat_read++ : -1;
    bool second = index == 1;
    uint8_t previous = 0;
    {
        std::lock_guard guard(lock);
        second = second && active && pc == player;
        if (second) { previous = G<uint8_t>(pc + 0x360); G<uint8_t>(pc + 0x360) = uint8_t(state.value.jba); }
    }
    __imp__sub_001429C3(ctx, base);
    if (index == 0) opsat_levels.nsa = ctx.eax & 0xFF;
    if (index == 1) opsat_levels.jba = ctx.eax & 0xFF;
    if (second) {
        G<uint8_t>(pc + 0x360) = previous;
        ctx.eax = (ctx.eax & ~0xFFu) | uint8_t(4 - (ctx.eax & 0xFF));
    }
}

X86_EXTERN_FUNC(__imp__sub_001BA097);
X86_FUNC(sub_001BA097) {
    using namespace game::trust;
    uint32_t menu = ctx.ecx;
    __imp__sub_001BA097(ctx, base);
    auto saved = ctx;
    ctx.ecx = R32(menu + 0x14);
    uint32_t pc = CallGuest(ctx, 0x17B720, {});
    ctx = saved;
    std::lock_guard guard(lock);
    Observe(pc);
    if (!active) return;
    auto it = state.credits.find(Key(state, "statistics-" + state.map));
    Pair before = state.value;
    if (it == state.credits.end()) {
        State preview = state;
        review_delta = preview.Change(StatisticsDelta(pc));
        review_value = preview.value;
    } else {
        review_delta = it->second.effective;
        review_value = state.value;
        before.nsa -= review_delta.nsa;
        before.jba -= review_delta.jba;
    }
    review_ready = true;
    W32(menu + 0xB0, before.nsa);
    W32(menu + 0xA0, review_delta.nsa);
}

// The rules table used each old scalar coefficient even after the total had
// become independent. Keep its localized rule names, replace its numeric cells.
X86_EXTERN_FUNC(__imp__sub_001BB4F7);
X86_FUNC(sub_001BB4F7) {
    using namespace game::trust;
    uint32_t out = R32(ctx.esp + 4), rule = R32(ctx.esp + 8);
    __imp__sub_001BB4F7(ctx, base);
    std::array<int, 11> count;
    {
        std::lock_guard guard(lock);
        if (!active || !player || rule < 1 || rule > 5) return;
        count = StatisticsCounts(player);
    }
    int values[] = {-count[2] * 2 - count[5] * 5, count[0] - count[3], -count[7], -count[10] * 2, -count[6]};
    char text[64]; std::snprintf(text, sizeof(text), "NSA %+d", values[rule - 1]);
    CellText(ctx, out, std::u16string(text, text + std::strlen(text)));
}

X86_EXTERN_FUNC(__imp__sub_001BB476);
X86_FUNC(sub_001BB476) {
    using namespace game::trust;
    uint32_t out = R32(ctx.esp + 4), rule = R32(ctx.esp + 8);
    __imp__sub_001BB476(ctx, base);
    { std::lock_guard guard(lock); if (!active || (rule != 1 && rule != 2)) return; }
    std::string_view english = rule == 1 ? "Kills (enemies/civilians)" : "Knockouts (enemies/civilians)";
    if (const char16_t* local = app::Translation(english)) CellText(ctx, out, local);
    else CellText(ctx, out, std::u16string(english.begin(), english.end()));
}
