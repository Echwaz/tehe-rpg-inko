#pragma once
#include "defeat_cutscene.h"

enum class EnemyKind { Hellspider, Skitter, Count };
enum class ArenaKind { Boss, Street, Exploration };
constexpr EnemyKind kFirstEncounter = EnemyKind::Skitter;

struct BattleCamera {
    float x, y, z, targetX, targetY, targetZ, fov, farPlane;
};
struct Encounter {
    const char* name;
    const char* role;
    int dp, hp, focusPower, aoePower, aoeEvery, maxDevastation;
    bool awakens;
    int deathDuration;
    EnemyKind next;
    ArenaKind arena;
    BattleCamera camera;
};
namespace encounter_data {
    constexpr BattleCamera bossCamera = {-1.8f, 1.55f, 7.3f, -1.8f, 2.05f, .3f, 50.f, 60.f};
    constexpr BattleCamera streetCamera = {.1f, .711f, 3.8f, .1f, 1.211f, -3.2f, 50.f, 120.f};
    constexpr Encounter boss = {"Hellspider", "Boss", 55, 500, 18, 8, 3, 300, true,
                                  defeat_cutscene::Duration, EnemyKind::Count, ArenaKind::Boss, bossCamera};
    constexpr Encounter awaken = {"Awaken Hellspider", "Boss", 160, 1300, 30, 15, 3, 999, false,
                                    defeat_cutscene::Duration, EnemyKind::Count, ArenaKind::Boss, bossCamera};
    constexpr Encounter skitter = {"Skitter", "Enemy", 45, 180, 12, 0, 0, 300, false,
                                     75, EnemyKind::Hellspider, ArenaKind::Street, streetCamera};
}
constexpr const Encounter& encounter(EnemyKind kind, bool awakened = false) {
    return kind == EnemyKind::Skitter ? encounter_data::skitter :
        (awakened ? encounter_data::awaken : encounter_data::boss);
}
