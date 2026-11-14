#include <cassert>
#include <cstdio>
#include <cstring>
#include "../source/battle_ui_cache.h"
#include "../source/skill_description_layout.h"
int main() {
    static_assert(encounter(kFirstEncounter).next==EnemyKind::Hellspider,"Encounter order changed");
    static_assert(!encounter(EnemyKind::Skitter).awakens,"Skitter must remain single phase");
    Battle battle;battle.start(EnemyKind::Skitter);
    const auto plan=battleButtonsKey(battle,0,0);
    for (int i=0;i<600;++i) assert(battleButtonsKey(battle,0,0)==plan);
    assert(battleButtonsKey(battle,1,0)!=plan);
    assert(battleButtonsKey(battle,0,1)!=plan);
    Party p=battle.party();p.front[0].sp=0;battle.setPartyOverride(p);battle.start(EnemyKind::Skitter);
    assert(battleButtonsKey(battle,0,0)!=plan); // Skill affordability must invalidate the buttons.
    battle.execute();assert(battleButtonsKey(battle,0,0)!=plan);
    battle.start(EnemyKind::Hellspider);assert(battleButtonsKey(battle,0,0)!=plan);
    SkillDescriptionLayout layout;
    int measured=0;auto width=[&](const char* s) {++measured;return std::strlen(s)*10.f;};
    const auto first=layout.lines("Hit enemy and restore SP after attack", "[HP Eff]",width);
    assert(first.size()==2 && first[0]=="Hit enemy and restore SP" && first[1]=="after attack [HP Eff]");
    const int once=measured;
    for(int i=0;i<600;++i) assert(layout.lines("Hit enemy and restore SP after attack","[HP Eff]",width)==first);
    assert(measured==once);
    layout.reset();const auto second=layout.lines("Heal DP","",width);
    assert(second.size()==1 && second[0]=="Heal DP");
    layout.reset();assert(layout.lines("","",width).empty());
    std::puts("UI cache: idle stability, slot/menu/phase/SP/encounter changes and unchanged description wrapping OK");
}
