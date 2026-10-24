#pragma once
#include <algorithm>
#include <cstdint>

namespace game::trust {
// Original confident/trusting tiers begin above 60; eligibility is per camp.
constexpr unsigned RewardCamps(int nsa, int jba) {
    return (nsa > 60 ? 1u : 0u) | (jba > 60 ? 2u : 0u);
}
constexpr uint8_t LoadoutType(int nsa, int jba) {
    unsigned camps = RewardCamps(nsa, jba);
    return camps & 1 ? 9 : camps & 2 ? 10 : 7;
}
// Faction components are recovery content, not general hacking difficulty.
constexpr bool RecoveryComponent(uint8_t faction, int nsa, int jba) {
    return faction == 0 || faction == 3 || (faction == 1 && nsa <= 40) || (faction == 2 && jba <= 40);
}
constexpr uint32_t CombinedGearFlags(uint32_t first, uint32_t second) {
    // Bits 0..23 are positive/negative equipment pairs. Explicit restrictions win.
    uint32_t flags = (first & 0xE0000000u) | ((first | second) & 0x1FFFFFFFu);
    for (unsigned bit = 0; bit < 24; bit += 2)
        if (flags & (1u << (bit + 1))) flags &= ~(1u << bit);
    return flags;
}
} // namespace game::trust
