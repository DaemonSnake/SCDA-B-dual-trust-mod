#include "game/trust_consumers.h"
#include "game/trust_model.h"
#include <cassert>
#include <iostream>
using namespace game::trust;
int main() {
    assert(CrisisValue({100,40}) == 61);
    assert(CrisisValue({75,75}) == 50);
    assert(CrisisValue({100,0}) == 100);
    static_assert(LoadoutType(75, 0) == 9);
    static_assert(LoadoutType(0, 75) == 10);
    static_assert(LoadoutType(10, 10) == 7);
    static_assert(LoadoutType(60, 60) == 7);
    static_assert(RewardCamps(61, 61) == 3);
    static_assert(RewardCamps(100, 100) == 3);
    static_assert(RecoveryComponent(1, 40, 100));
    static_assert(!RecoveryComponent(2, 40, 100));
    static_assert(RecoveryComponent(2, 100, 40));
    static_assert(!RecoveryComponent(1, 100, 40));
    static_assert(RecoveryComponent(1, 0, 0) && RecoveryComponent(2, 0, 0));
    static_assert(!RecoveryComponent(1, 75, 75) && !RecoveryComponent(2, 75, 75));
    static_assert(RecoveryComponent(0, 100, 100) && RecoveryComponent(3, 100, 100));
    static_assert(CombinedGearFlags(1, 4) == 5);
    static_assert(CombinedGearFlags(1, 2) == 2); // a restriction is not bypassed
    static_assert(CombinedGearFlags(0x80000000u, 0x40000000u) == 0x80000000u);
    std::cout << "trust consumers: faction eligibility, recovery visibility and conservative gear union passed\n";
}
