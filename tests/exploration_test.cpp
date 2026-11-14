#include <cassert>
#include <cstdio>
#include "../source/exploration.h"
#include "../source/combat.h"
using namespace exploration;
int main() {
    World w;
    assert(w.pending==-1 && w.cleared()==0 && canStand(w.player,.38f));
    assert(floorHeight(Point(0,35))==.04f && floorHeight(Point(6,35))==.18f);
    for(auto& e:w.enemies)e.alive=false;
    w.update(0,1,0,false,false,.05f);
    assert(w.player.z<35 && w.moving && w.walkTime>0);
    Point a=w.player;w.update(1,1,0,false,false,.05f);
    assert(distance(a,w.player)<=3.5f*.05f+.001f);
    for(int i=0;i<300;++i)w.update(-1,0,0,true,false,.05f);
    assert(canStand(w.player,.38f));
    assert(!clear(Point(-3.7f,20.f),.5f)); // Parked car.
    const Point blocked=move(Point(0,20),-6,0,.38f);
    assert(clear(blocked,.38f));
    w.reset();w.grace=0;w.player=w.enemies[0].position;
    w.update(0,0,0,false,false,.016f);
    assert(w.pending==0);
    const Point contact=w.player;
    w.victory();assert(!w.enemies[0].alive && w.pending==-1 && w.grace>0);
    assert(distance(contact,w.player)<.001f);
    w.reset();for(auto& e:w.enemies)e.alive=false;
    w.player=Point(-2,-66);w.update(0,0,0,false,false,.016f);assert(w.pending==-1);
    w.update(0,0,0,false,true,.016f);assert(w.pending==3);
    w.reset();w.grace=0;w.player=Point(0,27);
    for(int i=0;i<300;++i) {
        w.update(0,0,0,false,false,.016f);
        assert(canStand(w.player,.38f));
        for(const auto& e:w.enemies)assert(clear(e.position,1.099f));
    }
    assert(w.enemies[0].chasing || w.pending==0);
    // A fresh encounter carries the exact party/gauge while ending active Overdrive.
    Battle battle;battle.start(EnemyKind::Skitter);
    const auto party=battle.party();const int gauge=battle.odGauge();
    battle.start(EnemyKind::Skitter,true);
    assert(battle.party().front[0].sp==party.front[0].sp && battle.odGauge()==gauge);
    assert(!battle.odActive());
    battle.start(EnemyKind::Hellspider,true);
    assert(battle.party().front[0].sp==party.front[0].sp && battle.odGauge()==gauge);
    std::puts("Exploration: movement, collisions, contact, removal, boss gate, patrols and carry OK");
}
