#pragma once
#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <string_view>

namespace game::trust {
struct Pair {
    int nsa = 0, jba = 0;
    bool operator==(const Pair&) const = default;
};
struct Credit {
    Pair requested, effective;
};

// Host-owned state. Save sidecars serialize the ledger as well as both values.
// Store effective changes: reversing a choice at 100 must not manufacture points.
struct State {
    Pair value{75, 75};
    std::map<std::string, Credit> credits;
    std::string map, mission;
    bool initialized = false;
    int dispatched = 50;

    Pair Change(Pair delta) {
        Pair before = value;
        value.nsa = std::clamp(value.nsa + delta.nsa, 0, 100);
        value.jba = std::clamp(value.jba + delta.jba, 0, 100);
        return {value.nsa - before.nsa, value.jba - before.jba};
    }
    bool Has(const std::string& key) const { return credits.contains(key); }
    Pair Once(const std::string& key, Pair delta) {
        if (Has(key)) return {};
        Pair effective = Change(delta);
        credits.emplace(key, Credit{delta, effective});
        return effective;
    }
    Pair Revoke(const std::string& key) {
        auto it = credits.find(key);
        if (it == credits.end()) return {};
        Pair delta = Change({-it->second.effective.nsa, -it->second.effective.jba});
        credits.erase(it);
        return delta;
    }
};

inline std::string Mission(std::string map) {
    if (map.find("hq2") != std::string::npos) return "hq1";
    if (map.find("hq5") != std::string::npos) return "hq2";
    size_t at = map.rfind('_');
    return at == std::string::npos ? map : map.substr(0, at);
}
inline std::string Key(const State& state, std::string_view id) { return state.mission + ":" + std::string(id); }

enum class HishamEvidence { Accepted, Exposed };
inline Pair RecordHishamEvidence(State& state, HishamEvidence evidence) {
    Pair before = state.value;
    // Earlier builds inferred discovery from a canceled body objective. Both
    // explicit branches supersede that inference, including restored saves.
    state.Revoke(Key(state, "hisham-no-proof"));
    if (evidence == HishamEvidence::Exposed) {
        state.Revoke(Key(state, "hishambody"));
        state.Revoke(Key(state, "hisham-proof-accepted"));
        state.Once(Key(state, "hisham-exposed"), {0, -15});
    } else if (!state.Has(Key(state, "hisham-exposed"))) {
        state.Once(Key(state, "hisham-proof-accepted"), {});
    }
    return {state.value.nsa - before.nsa, state.value.jba - before.jba};
}
inline Pair GrantHishamProof(State& state) {
    if (state.Has(Key(state, "hisham-exposed"))) return {};
    return state.Once(Key(state, "hishambody"), {0, 5});
}

// Bits correspond to the mission's kill/knockout/seen/alarm/body rule switches.
inline Pair StatisticsScore(const std::array<int, 11>& count, unsigned enabled) {
    int gain = 0, loss = 0;
    if (enabled & 1) loss += count[2] * 2 + count[5] * 5;
    if (enabled & 2) { gain += count[0]; loss += count[3]; }
    if (enabled & 4) loss += count[7];
    if (enabled & 8) loss += count[10] * 2;
    if (enabled & 16) loss += count[6];
    return {std::min(gain, 15) - std::min(loss, 15), 0};
}

// Only a low trust value starts a crisis. A high value for one camp cannot
// manufacture distrust in the other. The guest adapter uses this for its legacy
// crisis dispatch; the persistent state and the two displayed bars stay independent.
inline int CrisisValue(Pair value) {
    if (value.nsa < 50 && value.nsa <= value.jba) return value.nsa;
    if (value.jba < 50) return 100 - value.jba;
    return 50;
}
}
