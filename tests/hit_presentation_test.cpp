#include <array>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#define private public
#include "../source/combat.h"
#undef private

using hit_presentation::Style;

static void testRhythms() {
    using hit_presentation::delay;
    const int cascade[] = {0, 11, 21, 30, 38, 45, 51, 56, 72};
    const int escapade[] = {0, 7, 14, 21, 28, 35, 42, 49, 56, 81};
    for (int i = 0; i < 9; ++i) assert(delay(Style::Cascade, i, 9) == cascade[i]);
    for (int i = 0; i < 10; ++i) assert(delay(Style::Escapade, i, 10) == escapade[i]);
    for (Style style : {Style::Regular, Style::Cascade, Style::Escapade}) {
        for (int hits = 1; hits <= 10; ++hits) {
            assert(delay(style, 0, hits) == 0);
            for (int i = 1; i < hits; ++i) assert(delay(style, i, hits) > delay(style, i - 1, hits));
            assert(delay(style, hits - 1, hits) == (hits - 1) * kFxHitGap);
        }
    }
    assert(hit_presentation::select("Ruka", 0) == Style::Cascade);
    assert(hit_presentation::select("Karen", 0) == Style::Escapade);
    assert(hit_presentation::select("Ruka", -1) == Style::Regular);
    assert(hit_presentation::select("Karen", 1) == Style::Regular);
    assert(hit_presentation::select("Yuki", 0) == Style::Regular);
    for (Style style : {Style::Cascade, Style::Escapade}) {
        const auto normal = hit_presentation::impact(style, false);
        const auto finish = hit_presentation::impact(style, true);
        assert(finish.recoil > normal.recoil && finish.recoilFrames > normal.recoilFrames);
        assert(finish.slashScale > normal.slashScale && finish.pitch < normal.pitch);
        assert(finish.gain <= 1.f && normal.gain < finish.gain);
    }
}

static void testEvents(Style style, int hits, int dp, int hp) {
    Battle b;
    b.start();
    if (style == Style::Escapade) b.swapSlot(0, 0);
    Combatant& attacker = b.party_.front[0];
    attacker.name = "Custom name";
    attacker.skills[0].name = "Custom skill";
    attacker.skills[0].hits = hits;
    attacker.sp = 20;
    b.enemy_.dp = dp;
    b.enemy_.hp = hp;
    b.enemy_.broken = dp == 0;
    Combatant expectedEnemy = b.enemy_;
    const auto expected = performAttack(attacker.skills[0], attacker, expectedEnemy);
    const int cost = attacker.skills[0].sp_cost;
    assert(b.setCommand(0, CommandType::Skill, 0));
    b.runPlayerAction(0);
    assert(attacker.sp == 20 - cost);
    assert(b.enemy().dp == expectedEnemy.dp && b.enemy().hp == expectedEnemy.hp);
    assert(b.enemy().devastation == expectedEnemy.devastation);
    assert(b.odGauge() == expected.hits);

    const auto events = b.takeFx();
    int count = 0, finishers = 0, sumDp = 0, sumHp = 0, last = -1;
    bool broke = false, finalBreak = false;
    for (const auto& event : events) {
        if (event.kind == FxKind::Damage) {
            assert(event.style == style && event.attacker_slot == 0);
            assert(event.hit_index == count && event.delay > last);
            assert(event.delay == hit_presentation::delay(style, count, hits));
            assert(event.finisher == (count + 1 == expected.hits));
            finalBreak = event.finisher && event.breaks_target;
            last = event.delay;
            sumDp += event.dp;
            sumHp += event.hp;
            finishers += event.finisher;
            ++count;
        } else if (event.kind == FxKind::Break) {
            broke = true;
            if (event.finisher) assert(finalBreak && event.delay == last);
        }
    }
    assert(count == expected.hits && finishers == 1);
    assert(sumDp == expected.dp_lost && sumHp == expected.hp_lost && broke == expected.broke);
    assert(b.fxSpan_ >= last + hit_presentation::FinishRecovery);
}

static void testRegularEvents() {
    Battle b;
    b.start();
    b.party_.front[0].sp = 20;
    assert(b.setCommand(0, CommandType::Skill, 1));
    b.runPlayerAction(0);
    int hit = 0;
    for (const auto& event : b.takeFx()) {
        assert(event.style == Style::Regular && !event.finisher);
        if (event.kind == FxKind::Damage) assert(event.delay == hit++ * kFxHitGap);
    }
    assert(hit == 2);
}

static void testKillingHitTransition() {
    Battle b;
    b.start();
    for (int phase = 0; phase < 2; ++phase) {
        b.party_.front[0].sp = 20;
        b.enemy_.dp = 0;
        b.enemy_.hp = 1;
        b.enemy_.broken = true;
        assert(b.setCommand(0, CommandType::Skill, 0));
        b.execute();
        for (int f = 0; f < 100 && b.phase() == BattlePhase::Executing; ++f) b.update();
        assert(b.phase() == (phase == 0 ? BattlePhase::Awakening : BattlePhase::Dying));
        const auto events = b.takeFx();
        assert(events.size() == 1 && events[0].finisher && events[0].delay == 0);
        assert(b.fxSpan_ == hit_presentation::FinishRecovery);
        for (int f = 0; f < 1000 && b.phase() != BattlePhase::Planning && b.phase() != BattlePhase::Victory; ++f) {
            b.update();
            assert(b.takeFx().empty());
        }
    }
    assert(b.phase() == BattlePhase::Victory);
}

int main() {
    testRhythms();
    for (Style style : {Style::Cascade, Style::Escapade}) {
        for (int hits = 1; hits <= 10; ++hits) {
            testEvents(style, hits, 1000, 10000);
            testEvents(style, hits, 0, 1); // early kill becomes the finisher
            testEvents(style, hits, 1, 10000); // Break before the combo ends
        }
        testEvents(style, 1, 1, 10000); // finisher and Break share an impact frame
        testEvents(style, 9, 0, 15); // kill in the middle, no trailing ghost hits
    }
    testRegularEvents();
    testKillingHitTransition();
    std::puts("PASS: combo rhythms, custom counts/names, unchanged damage/SP/OD, early kills, Break and transitions");
}
