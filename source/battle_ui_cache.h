#pragma once
#include "combat.h"
#include <array>

inline std::array<int, 7> battleButtonsKey(const Battle& battle, int menu, int slot) {
    const auto& c = battle.party().front[slot];
    int valid = 0, usable = 0;
    for (int i = 0; i < 2; ++i) {
        if (c.skills[i].valid()) valid |= 1 << i;
        if (battle.canUseSkill(slot, i)) usable |= 1 << i;
    }
    return {{static_cast<int>(battle.phase()), menu, slot, valid, usable,
        battle.odBarsReady() > 0 && !battle.odActive(), static_cast<int>(encounter(battle.enemyKind()).next)}};
}
