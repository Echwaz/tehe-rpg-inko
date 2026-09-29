// Tes logika combat + dialog. Berjalan di PC, tanpa devkitPro.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Hanya untuk tes: membuka anggota privat Battle supaya kondisi (mis. musuh sedang break)
// bisa disiapkan langsung.
#define private public
#include "../source/combat.h"
#undef private
#include "../source/dialogue.h"
#include "../source/mesh_white_tree.h"

static Combatant dummy(int dp, int hp) {
    return Combatant("D", "x", dp, hp, 0, Skill(), Skill());
}
static Skill hit(int hits, int power, int dpp = 100, int hpp = 100) {
    return Skill("Hit", SkillKind::Attack, 0, hits, power, dpp, hpp);
}
static AttackSummary strike(const Skill& s, Combatant& t) {
    const Combatant nobody = dummy(0, 1);
    return performAttack(s, nobody, t);
}

static void testHitRules() {
    // 1 hit besar: DP habis, sisa damage dibuang, HP utuh.
    Combatant a = dummy(20, 50);
    AttackSummary s = strike(hit(1, 100), a);
    assert(s.broke && a.dp == 0 && a.hp == 50 && s.hp_lost == 0);

    // Multi-hit: hit kedua setelah break langsung ke HP.
    Combatant b = dummy(20, 50);
    s = strike(hit(2, 30), b);
    assert(s.broke && b.dp == 0 && b.hp == 20 && s.hits == 2);

    // Pengali DP/HP
    Combatant c = dummy(100, 100);
    strike(hit(1, 10, 200, 50), c);
    assert(c.dp == 80);

    // Musuh: break = stun. Hit yang memecahkan DP TIDAK menaikkan devastation rate;
    // hit sesudahnya (musuh sudah break) menaikkannya dan damage HP dikalikan rate itu.
    Combatant e = dummy(10, 1000);
    e.is_enemy = true;
    strike(hit(1, 10), e);
    assert(e.broken && e.stunned && e.devastation == 100);
    strike(hit(1, 100), e);                              // hit 2: musuh sudah break
    assert(e.devastation == 100 + kDevastationPerHit);
    const int before = e.hp;
    strike(hit(1, 100), e);                              // hit 3: dikalikan rate 102%
    assert(before - e.hp == 100 * (100 + kDevastationPerHit) / 100);

    // Sekutu break tidak pulih sendiri, tapi heal DP memulihkan.
    Party p;
    p.front[0] = dummy(20, 50);
    p.front[1] = p.front[0];
    p.front[2] = p.front[0];
    p.back[0] = dummy(20, 50);
    strike(hit(1, 100), p.front[0]);
    strike(hit(1, 100), p.back[0]);
    assert(p.front[0].broken && p.front[0].dp == 0);
    assert(p.back[0].broken && p.back[0].dp == 0);
    healPartyDP(p, 15);
    assert(!p.front[0].broken && p.front[0].dp == 15);
    assert(!p.back[0].broken && p.back[0].dp == 15);   // Resupply juga menyembuhkan back row
}

static void testEffects() {
    // AtkUp aditif (2 tumpukan), tumpukan ke-3 menggantikan yang tertua.
    Combatant att = dummy(0, 100);
    addEffect(att, EffectType::AtkUp, 40, 0);
    addEffect(att, EffectType::AtkUp, 40, 0);
    assert(effectTotal(att, EffectType::AtkUp) == 80);
    addEffect(att, EffectType::AtkUp, 10, 0);
    assert(att.effects.size() == 2 && effectTotal(att, EffectType::AtkUp) == 50);

    // Damage: 10 * 1.8 = 18
    att.effects.clear();
    addEffect(att, EffectType::AtkUp, 40, 0);
    addEffect(att, EffectType::AtkUp, 40, 0);
    Combatant t = dummy(100, 100);
    performAttack(hit(1, 10), att, t);
    assert(t.dp == 82);

    // Jenis berbeda saling dikali: AtkUp 100% + DefDown 50% => 10 * 2 * 1.5 = 30
    Combatant att2 = dummy(0, 100);
    addEffect(att2, EffectType::AtkUp, 100, 0);
    Combatant t2 = dummy(100, 100);
    addEffect(t2, EffectType::DefDown, 50, 2);
    performAttack(hit(1, 10), att2, t2);
    assert(t2.dp == 70);

    // DefUp mengurangi damage, dengan batas kMaxDefUpPct.
    Combatant t3 = dummy(100, 100);
    addEffect(t3, EffectType::DefUp, 50, 2);
    performAttack(hit(1, 10), dummy(0, 1), t3);
    assert(t3.dp == 95);
    Combatant t4 = dummy(100, 100);
    addEffect(t4, EffectType::DefUp, 90, 2);
    performAttack(hit(1, 100), dummy(0, 1), t4);
    assert(t4.dp == 100 - (100 * (100 - kMaxDefUpPct) / 100));

    // AtkDown pada penyerang mengurangi damage keluar.
    Combatant att5 = dummy(0, 100);
    addEffect(att5, EffectType::AtkDown, 25, 2);
    Combatant t5 = dummy(100, 100);
    performAttack(hit(1, 20), att5, t5);
    assert(t5.dp == 85);

    // Damage selalu minimal 1.
    Combatant t6 = dummy(100, 100);
    addEffect(t6, EffectType::DefUp, 70, 2);
    performAttack(hit(1, 1), dummy(0, 1), t6);
    assert(t6.dp == 99);

    // Durasi: turun tiap tick, hilang di 0. Efek sekali pakai tidak ikut turun.
    Combatant d = dummy(0, 10);
    addEffect(d, EffectType::DefUp, 30, 2);
    addEffect(d, EffectType::AtkUp, 40, 0);
    tickEffects(d);                                   // ronde pemasangan tidak dihitung
    assert(effectTotal(d, EffectType::DefUp) == 30);
    tickEffects(d);                                   // sisa 1
    assert(effectTotal(d, EffectType::DefUp) == 30);
    tickEffects(d);
    assert(effectTotal(d, EffectType::DefUp) == 0);
    assert(effectTotal(d, EffectType::AtkUp) == 40);

    // AtkUp/DevUp habis setelah menyerang, DefUp tidak.
    addEffect(d, EffectType::DevUp, 4, 0);
    addEffect(d, EffectType::DefUp, 30, 2);
    consumeOneTimeBuffs(d);
    assert(effectTotal(d, EffectType::AtkUp) == 0 && effectTotal(d, EffectType::DevUp) == 0);
    assert(effectTotal(d, EffectType::DefUp) == 30);

    // Devastation rate hanya naik saat musuh sedang break (DevUp menambah kenaikannya).
    Combatant fresh = dummy(1000, 1000);
    fresh.is_enemy = true;
    Combatant boosted = dummy(0, 0);
    addEffect(boosted, EffectType::DevUp, 10, 0);
    performAttack(hit(1, 1), boosted, fresh);
    assert(fresh.devastation == 100);                                // belum break: tidak naik
    Combatant brk = dummy(1, 1000);
    brk.is_enemy = true;
    performAttack(hit(1, 5), dummy(0, 1), brk);                      // hit yang memecahkan DP
    assert(brk.broken && brk.devastation == 100);                    // ...tidak menaikkan rate
    performAttack(hit(1, 1), boosted, brk);                          // sedang break: naik + DevUp
    assert(brk.devastation == 100 + kDevastationPerHit + 10);

    // Ringkasan tampilan
    Combatant sm = dummy(0, 1);
    addEffect(sm, EffectType::AtkUp, 40, 0);
    addEffect(sm, EffectType::DefUp, 30, 2);
    assert(effectSummary(sm) == "ATK+ DEF+(2t)");
    assert(effectSummary(sm, effectBit(EffectType::AtkUp)) == "DEF+(2t)");     // jenis yang jadi icon dilewati
    assert(effectSummary(sm, effectBit(EffectType::AtkUp) | effectBit(EffectType::DefUp)).empty());
}

static void testPlanning() {
    std::srand(1);
    Battle b;
    b.start();
    assert(b.phase() == BattlePhase::Planning && b.round() == 1);

    // Susunan awal front: Ruka, Yuki, Tama.
    assert(b.party().front[0].name == "Ruka" && b.party().front[1].name == "Yuki" &&
           b.party().front[2].name == "Tama");

    // Skill dukungan (Tama, slot 2) harus jalan lebih dulu dari serangan.
    assert(b.setCommand(2, CommandType::Skill, 0));       // Resupply (heal = grup dukungan)
    b.execute();
    assert(b.queue().size() == 3 && b.queue()[0] == 2 && b.queue()[1] == 0 && b.queue()[2] == 1);

    // Swap hanya boleh saat perencanaan.
    b.swapSlot(0, 0);
    assert(b.party().front[0].name == "Ruka");

    Battle c;
    c.start();
    c.swapSlot(0, 0);
    assert(c.party().front[0].name == "Karen" && c.party().back[0].name == "Ruka");
    c.swapSlot(0, 0);                          // bisa berkali-kali
    assert(c.party().front[0].name == "Ruka");
}

static int backIndex(const Battle& b, const char* name) {
    for (int i = 0; i < 3; ++i)
        if (b.party().back[i].name == name) return i;
    assert(!"nama tidak ada di back row");
    return -1;
}

// Jalankan sampai kembali ke perencanaan. false jika battle keburu berakhir.
static void testSwapWithinRows() {
    Battle b;
    b.start();

    // Tukar urutan front: karakter dan aksinya berpindah bersama.
    assert(b.setCommand(0, CommandType::Skill, 1));          // Ruka: Cross Cut
    b.swapFront(0, 2);
    assert(b.party().front[0].name == "Tama" && b.party().front[2].name == "Ruka");
    assert(b.command(2).type == CommandType::Skill && b.command(0).type == CommandType::Attack);
    assert(b.commandLabel(2) == "Cross Cut");

    // Tukar urutan back.
    assert(b.party().back[0].name == "Karen" && b.party().back[2].name == "Tsukasa");
    b.swapBack(0, 2);
    assert(b.party().back[0].name == "Tsukasa" && b.party().back[2].name == "Karen");

    // Indeks tidak valid atau sama diabaikan.
    b.swapFront(1, 1);
    b.swapFront(-1, 2);
    b.swapBack(0, 3);
    assert(b.party().front[1].name == "Yuki" && b.party().back[0].name == "Tsukasa");

    // Di luar fase perencanaan semuanya diabaikan.
    b.execute();
    b.swapFront(0, 1);
    b.swapBack(0, 1);
    assert(b.party().front[0].name == "Tama" && b.party().back[0].name == "Tsukasa");
}

static bool runToPlanning(Battle& b) {
    for (int i = 0; i < 100000; ++i) {
        if (b.phase() == BattlePhase::Planning) return true;
        if (b.phase() == BattlePhase::Victory || b.phase() == BattlePhase::Defeat) return false;
        b.update();
    }
    assert(!"tidak selesai");
    return false;
}

static void runUntil(Battle& b, BattlePhase target) {
    for (int i = 0; i < 100000 && b.phase() != target; ++i) b.update();
    assert(b.phase() == target);
}

static void testBuffFlow() {
    // Tsukasa (buffer) maju ke slot 2, pakai Semangat Kerja (AtkUp tim, sekali pakai).
    std::srand(3);
    Battle b;
    b.start();
    b.swapSlot(2, backIndex(b, "Tsukasa"));
    assert(b.party().front[2].name == "Tsukasa");
    assert(b.setCommand(2, CommandType::Skill, 0));
    b.execute();
    assert(b.queue()[0] == 2);
    runUntil(b, BattlePhase::EnemyTurn);

    // Ruka & Yuki hanya attack biasa -> AtkUp mereka tetap ada (hanya skill yang menghabiskannya).
    assert(effectTotal(b.party().front[0], EffectType::AtkUp) == 40);
    assert(effectTotal(b.party().front[1], EffectType::AtkUp) == 40);
    assert(effectTotal(b.party().front[2], EffectType::AtkUp) == 40);

    // Megumi (Hard Knocks, durasi 1) memasang DefDown ke musuh: debuff musuh sengaja
    // tidak pernah berkurang durasinya (supaya gampang ditumpuk), jadi tetap ada terus.
    // Musuh menyerang target acak, jadi coba beberapa seed sampai party selamat dua ronde.
    bool checked = false;
    for (int seed = 0; seed < 40 && !checked; ++seed) {
        std::srand(seed);
        Battle c;
        c.start();
        c.swapSlot(2, backIndex(c, "Megumi"));
        assert(c.party().front[2].name == "Megumi");
        assert(c.setCommand(2, CommandType::Skill, 1));
        c.execute();
        runUntil(c, BattlePhase::EnemyTurn);
        assert(effectTotal(c.enemy(), EffectType::DefDown) == 30);
        if (!runToPlanning(c)) continue;                             // giliran musuh ronde 1: belum dihitung
        assert(effectTotal(c.enemy(), EffectType::DefDown) == 30);   // masih ada di ronde 2
        c.execute();
        if (!runToPlanning(c)) continue;                             // awal giliran musuh ronde 2
        assert(effectTotal(c.enemy(), EffectType::DefDown) == 30);   // tetap ada: debuff musuh tidak meluruh
        checked = true;
    }
    assert(checked);
}

static void testSkillList() {
    Battle b;
    b.start();
    const Party& p = b.party();

    // Nama skill sesuai Skill List.
    // Urutan menu: skill andalan (pemakaian terbatas) di atas skill biasa.
    assert(std::string(p.front[0].skills[0].name) == "Ephemeral Cascade" && p.front[0].skills[0].ex);
    assert(std::string(p.front[0].skills[1].name) == "Cross Cut");
    assert(std::string(p.front[1].skills[0].name) == "Meteor Shower");
    assert(std::string(p.front[1].skills[1].name) == "Break Booster");
    assert(std::string(p.front[2].skills[0].name) == "Resupply");
    assert(std::string(p.front[2].skills[1].name) == "Saltire Slash");
    assert(std::string(p.back[0].skills[0].name) == "Bloody Escapade");
    assert(std::string(p.back[0].skills[1].name) == "Wild Fling");
    assert(std::string(p.back[1].skills[0].name) == "Excelsior Impact");
    assert(std::string(p.back[1].skills[1].name) == "Hard Knocks");
    assert(std::string(p.back[2].skills[0].name) == "Full Enhance");
    assert(std::string(p.back[2].skills[1].name) == "Blessed Shot");

    // Biaya SP sesuai daftar; EX tanpa batas pemakaian, Resupply tetap 10 pemakaian.
    assert(p.front[0].skills[0].sp_cost == 12 && p.front[0].skills[1].sp_cost == 6);
    assert(p.front[1].skills[0].sp_cost == 11 && p.front[1].skills[1].sp_cost == 4);
    assert(p.front[2].skills[0].sp_cost == 8 && p.front[2].skills[1].sp_cost == 7);
    assert(p.back[0].skills[0].sp_cost == 11 && p.back[0].skills[1].sp_cost == 7);
    assert(p.back[1].skills[0].sp_cost == 10 && p.back[1].skills[1].sp_cost == 8);
    assert(p.back[2].skills[0].sp_cost == 9 && p.back[2].skills[1].sp_cost == 7);
    assert(p.front[0].uses_left[0] == -1 && p.front[0].uses_left[1] == -1);
    assert(p.front[2].uses_left[0] == 10);

    // Ronde 1: SP 10 -> skill 11-12 SP belum bisa; Cross Cut (6) dan Break Booster (4) bisa.
    assert(!b.canUseSkill(0, 0) && !b.canUseSkill(1, 0));
    assert(b.canUseSkill(0, 1) && b.canUseSkill(1, 1));

    // Ronde 2: SP front = 12 -> Ephemeral Cascade bisa dan memakai 12 SP; tanpa batas pemakaian.
    b.execute();
    runUntil(b, BattlePhase::Planning);
    assert(b.round() == 2 && b.party().front[0].sp == 10 + kSpRegenFront);
    assert(b.canUseSkill(0, 0));
    assert(b.setCommand(0, CommandType::Skill, 0));
    b.execute();
    runUntil(b, BattlePhase::EnemyTurn);
    assert(b.party().front[0].sp == 0 && b.party().front[0].uses_left[0] == -1);
}

static void testStunAndSpGain() {
    // Peluang stun 100% selalu membuat musuh stun; 0% tidak pernah.
    Combatant e1 = dummy(1000, 1000);
    e1.is_enemy = true;
    AttackSummary s = strike(hit(1, 1).withStun(100), e1);
    assert(e1.stunned && s.stunned);
    Combatant e2 = dummy(1000, 1000);
    e2.is_enemy = true;
    s = strike(hit(1, 1).withStun(0), e2);
    assert(!e2.stunned && !s.stunned);

    // Blessed Shot: 50% memulihkan 4 SP. Dalam banyak percobaan kedua hasil harus muncul.
    bool gained = false, notGained = false;
    for (int seed = 0; seed < 40; ++seed) {
        std::srand(seed);
        Battle b;
        b.start();
        b.swapSlot(2, backIndex(b, "Tsukasa"));
        assert(b.setCommand(2, CommandType::Skill, 1));       // Blessed Shot, 7 SP
        b.execute();
        runUntil(b, BattlePhase::EnemyTurn);
        const int sp = b.party().front[2].sp;
        assert(sp == kStartSP - 7 || sp == kStartSP - 7 + 4);
        (sp > kStartSP - 7 ? gained : notGained) = true;
    }
    assert(gained && notGained);
}

static void testBlasterDevastation() {
    // Musuh yang sudah break (DP kosong).
    auto brokenEnemy = []() {
        Combatant e = dummy(1000, 1000);
        e.is_enemy = true;
        e.dp = 0;
        e.broken = true;
        return e;
    };
    // Pengali devastation rate hanya datang dari skill, bukan dari karakter/role.
    Combatant user = dummy(0, 1);

    // Skill biasa: +4 per hit. Skill Blaster: x4. Skill andalan Blaster: x5.
    Combatant e1 = brokenEnemy();
    performAttack(hit(1, 1), user, e1);
    assert(e1.devastation == 100 + kDevastationPerHit);
    Combatant e2 = brokenEnemy();
    Skill blasterSkill = hit(1, 1);
    blasterSkill.dev_mult = kBlasterDevMult;
    performAttack(blasterSkill, user, e2);
    assert(e2.devastation == 100 + kDevastationPerHit * kBlasterDevMult);
    Combatant e3 = brokenEnemy();
    Skill sig = hit(3, 1);
    sig.dev_mult = kBlasterSignatureDevMult;
    performAttack(sig, user, e3);
    assert(e3.devastation == 100 + 3 * kDevastationPerHit * kBlasterSignatureDevMult);

    // Musuh yang belum break: Blaster pun tidak menaikkan devastation rate.
    Combatant intact = dummy(1000, 1000);
    intact.is_enemy = true;
    performAttack(sig, user, intact);
    assert(intact.devastation == 100);

    // Roster nyata, musuh sedang break: skill Karen (Blaster) jauh lebih cepat dari attack biasa.
    auto runRound = [](bool withKaren) {
        Battle b;
        b.start();
        assert(b.party().back[0].name == "Karen");
        assert(b.party().back[0].skills[0].dev_mult == kBlasterSignatureDevMult);
        assert(b.party().back[0].skills[1].dev_mult == kBlasterDevMult);
        b.enemy_.dp = 0;
        b.enemy_.broken = true;
        if (withKaren) {
            b.swapSlot(2, 0);                        // Karen menggantikan Tama
            b.setCommand(2, CommandType::Skill, 1);  // Wild Fling: 3 hit, tiap hit x4
        }
        b.execute();
        runUntil(b, BattlePhase::EnemyTurn);
        return b.enemy().devastation;
    };
    const int without = runRound(false);   // Ruka + Yuki + Tama, semua Attack biasa
    const int with = runRound(true);       // Ruka + Yuki + Karen (Wild Fling)
    assert(without == 100 + 3 * kDevastationPerHit);
    assert(with == 100 + 2 * kDevastationPerHit + 3 * kDevastationPerHit * kBlasterDevMult);
    assert(with > without);

    // Attack biasa Karen TIDAK dapat bonus role: sama seperti karakter lain.
    Battle b;
    b.start();
    b.enemy_.dp = 0;
    b.enemy_.broken = true;
    b.swapSlot(2, 0);                                // Karen di slot 2, command default = Attack
    b.execute();
    runUntil(b, BattlePhase::EnemyTurn);
    assert(b.enemy().devastation == 100 + 3 * kDevastationPerHit);
}

// Role hanya label. Bonus ada di Skill::bonus: cek nilai bawaan roster dan bahwa Attack biasa netral.
static void testDefaultSkillBonuses() {
    const Party p = makeDefaultParty();
    const Combatant& ruka = p.front[0];
    const Combatant& yuki = p.front[1];
    const Combatant& karen = p.back[0];
    for (int j = 0; j < 2; ++j) {
        assert(ruka.skills[j].bonus == SkillBonus::HpEff && ruka.skills[j].hp_pct == kHpEffPct);
        assert(ruka.skills[j].dp_pct == 100 && ruka.skills[j].dev_mult == 1);
        assert(yuki.skills[j].bonus == SkillBonus::DpEff && yuki.skills[j].hp_pct == 100);
    }
    assert(yuki.skills[0].dp_pct == kDpEffExPct && yuki.skills[1].dp_pct == kDpEffPct);   // EX lebih besar
    assert(karen.skills[0].dev_mult == kBlasterSignatureDevMult && karen.skills[1].dev_mult == kBlasterDevMult);
    assert(karen.skills[0].hp_pct == 100 && karen.skills[0].dp_pct == 100);

    // Karakter lain: tanpa bonus sama sekali.
    for (const Combatant* c : { &p.front[2], &p.back[1], &p.back[2] })
        for (const Skill& s : c->skills)
            assert(s.bonus == SkillBonus::None && s.hp_pct == 100 && s.dp_pct == 100 && s.dev_mult == 1);

    // Attack biasa selalu netral, siapa pun pemakainya.
    for (const Combatant* c : { &ruka, &yuki, &karen }) {
        const Skill basic = normalAttackOf(*c);
        assert(basic.bonus == SkillBonus::None && basic.hp_pct == 100 && basic.dp_pct == 100 && basic.dev_mult == 1);
    }

    // Mengganti label role tidak mengubah apa pun pada skill.
    Combatant k = karen;
    assert(setRole(k, "healer") && std::string(k.role) == "Healer");
    assert(k.skills[0].dev_mult == kBlasterSignatureDevMult && k.skills[1].dev_mult == kBlasterDevMult);
    assert(!setRole(k, "Tank") && std::string(k.role) == "Healer");

    // Bonus hanya berlaku untuk skill Attack; Heal/Support tetap netral walau diberi bonus.
    Skill heal("H", SkillKind::HealDP, 1, 0, 5);
    heal.withBonus(SkillBonus::HpEff);
    assert(heal.hp_pct == 100);
    Skill atk("A", SkillKind::Attack, 1, 1, 5);
    atk.withBonus(SkillBonus::DpEff);
    assert(atk.dp_pct == kDpEffPct);
    atk.asEx();                                   // menjadi EX: bonus dihitung ulang
    assert(atk.dp_pct == kDpEffExPct);
}

static void testOverflowRule() {
    // 50 damage dalam 1 hit ke DP 30: DP habis, 20 sisanya dibuang, HP utuh.
    Combatant a = dummy(30, 100);
    AttackSummary s = strike(hit(1, 50), a);
    assert(s.broke && a.dp == 0 && a.hp == 100 && s.dp_lost == 30 && s.hp_lost == 0);

    // 50 damage dalam 2 hit (25 + 25) ke DP 20: hit 1 memecahkan DP (sisa 5 dibuang),
    // hit 2 melukai HP sebesar 25 penuh.
    Combatant b = dummy(20, 100);
    s = strike(hit(2, 25), b);
    assert(s.hits == 2 && s.dp_lost == 20 && s.hp_lost == 25 && b.dp == 0 && b.hp == 75);

    // Hit yang memecahkan DP tidak pernah menyentuh HP, sebanyak apa pun hit-nya:
    // DP 60, 3 hit x 25: DP 35 -> 10 -> 0 (sisa 15 dibuang). HP tetap utuh.
    Combatant c = dummy(60, 100);
    s = strike(hit(3, 25), c);
    assert(s.dp_lost == 60 && s.hp_lost == 0 && c.hp == 100 && c.broken);
    // Hit ke-4 (musuh sudah break) baru melukai HP.
    s = strike(hit(1, 25), c);
    assert(s.hp_lost == 25 && c.hp == 75);

    // applyHit langsung: dp_dmg besar tidak pernah "tumpah" ke HP.
    Combatant d = dummy(10, 50);
    HitResult r = applyHit(d, 999, 999);
    assert(r.dp_lost == 10 && r.hp_lost == 0 && d.hp == 50);
}

// DP musuh tidak pulih sendiri setelah break, dan devastation rate tidak di-reset.
static void testNoAutoRecovery() {
    bool checked = false;
    for (int seed = 0; seed < 40 && !checked; ++seed) {
        std::srand(seed);
        Battle b;
        b.start();
        b.enemy_.dp = 0;
        b.enemy_.broken = true;
        b.enemy_.devastation = 150;
        bool alive = true;
        for (int round = 0; round < 3 && alive; ++round) {
            b.execute();
            alive = runToPlanning(b);
            if (alive) {
                assert(b.enemy().dp == 0 && b.enemy().broken);
                assert(b.enemy().devastation >= 150);
            }
        }
        checked = alive;
    }
    assert(checked);

    // recoverFromBreak: dipakai gimmick nanti; DP penuh, break berakhir, rate kembali 100%.
    Combatant e = dummy(90, 100);
    e.is_enemy = true;
    e.dp = 0;
    e.broken = true;
    e.devastation = 220;
    recoverFromBreak(e);
    assert(e.dp == 90 && !e.broken && e.devastation == 100);
}

static void testFxEvents() {
    // 1) Rincian per hit cocok dengan total.
    Combatant t = dummy(20, 60);
    const AttackSummary a = strike(hit(5, 9), t);
    assert(static_cast<int>(a.hit_list.size()) == a.hits);
    int dp = 0, hp = 0, broken = 0;
    for (const HitResult& h : a.hit_list) { dp += h.dp_lost; hp += h.hp_lost; broken += h.just_broken ? 1 : 0; }
    assert(dp == a.dp_lost && hp == a.hp_lost && broken == 1);

    // 2) Heal: satu event per anggota front yang benar-benar pulih (yang sudah penuh tidak dapat).
    Battle b;
    b.start();
    b.party_.front[0].dp = 0;  b.party_.front[0].broken = true;
    b.party_.front[1].dp = 10;
    b.setCommand(2, CommandType::Skill, 0);              // Tama: Resupply (+20 DP tim)
    b.execute();
    std::vector<FxEvent> ev;
    for (int i = 0; i < 400 && ev.empty(); ++i) { b.update(); ev = b.takeFx(); }
    assert(ev.size() == 2);
    assert(ev[0].kind == FxKind::Heal && ev[0].slot == 0 && ev[0].dp == 20);
    assert(ev[1].kind == FxKind::Heal && ev[1].slot == 1 && ev[1].dp == 10);
    assert(b.takeFx().empty());                          // sudah dikosongkan
}

static void testStunEvents() {
    // Skill stun 100% ke musuh: event Stun terbit sekali, setelah hit terakhir, tanpa angka.
    Battle b;
    b.start();
    Combatant boss = dummy(500, 500);
    boss.is_enemy = true;
    Skill s = hit(3, 5);
    s.withStun(100);
    const AttackSummary a = performAttack(s, dummy(0, 1), boss);
    assert(a.stunned && boss.stunned && !boss.broken);
    b.fx_.clear();
    b.recordHits(a, true, 0, 0);
    std::vector<FxEvent> ev = b.takeFx();
    assert(ev.size() == 4);
    for (int i = 0; i < 3; ++i) assert(ev[i].kind == FxKind::Damage);
    assert(ev[3].kind == FxKind::Stun && ev[3].on_enemy && ev[3].dp == 0 && ev[3].hp == 0);
    assert(ev[3].delay == 2 * kFxHitGap + 4 && ev[3].delay >= ev[2].delay);

    // Tanpa stun (peluang 0): tidak ada event Stun.
    Combatant boss2 = dummy(500, 500);
    boss2.is_enemy = true;
    const AttackSummary a2 = performAttack(hit(3, 5), dummy(0, 1), boss2);
    b.recordHits(a2, true, 0, 0);
    for (const FxEvent& e : b.takeFx()) assert(e.kind != FxKind::Stun);

    // Break saja: musuh stun lewat break, event-nya Break (bukan Stun) dan a.stunned tetap false.
    Combatant boss3 = dummy(10, 500);
    boss3.is_enemy = true;
    const AttackSummary a3 = performAttack(hit(1, 50), dummy(0, 1), boss3);
    assert(a3.broke && !a3.stunned && boss3.stunned);
    b.recordHits(a3, true, 0, 0);
    ev = b.takeFx();
    assert(ev.size() == 2 && ev[1].kind == FxKind::Break);

    // Serangan ke sekutu tidak pernah menerbitkan Stun.
    AttackSummary a4 = a;
    b.recordHits(a4, false, 1, 0);
    for (const FxEvent& e : b.takeFx()) assert(e.kind != FxKind::Stun);
}

// Jalankan satu ronde penuh: eksekusi lalu giliran musuh, sampai perencanaan ronde berikutnya.
static void playRound(Battle& b) {
    assert(b.phase() == BattlePhase::Planning);
    b.execute();
    runUntil(b, BattlePhase::Planning);
}
// Supaya tes linimasa tidak berakhir kalah/menang di tengah jalan.
static void makeSturdy(Battle& b) {
    for (auto& c : b.party_.front) c.hp = c.max_hp = 10000;
    for (auto& c : b.party_.back)  c.hp = c.max_hp = 10000;
    b.enemy_.dp = b.enemy_.max_dp = 100000;
    b.enemy_.hp = b.enemy_.max_hp = 100000;
}
static int partyTotal(const Battle& b) {
    int t = 0;
    for (const auto& c : b.party().front) t += c.dp + c.hp;
    return t;
}

static void testTurnTimeline() {
    // Stun dari ronde 1: musuh skip giliran musuh 1, stun TETAP tampil di perencanaan ronde 2,
    // hilang di awal giliran musuh 2 SEBELUM musuh menyerang, lalu musuh menyerang.
    std::srand(1);
    Battle b;
    b.start();
    makeSturdy(b);
    const int start = partyTotal(b);
    performAttack(hit(1, 1).withStun(100), b.party_.front[0], b.enemy_);   // seolah skill stun kena di eksekusi
    assert(b.enemy_.stunned && b.enemy_.stun_skips == 1);
    playRound(b);
    assert(b.round() == 2);
    assert(b.enemy_.stunned && b.enemy_.stun_skips == 0);   // masih terlihat di giliran party ronde 2
    assert(partyTotal(b) == start);                          // musuh melewatkan giliran musuh 1
    b.execute();
    for (int i = 0; i < 100000 && b.enemy_.stunned; ++i) b.update();
    assert(!b.enemy_.stunned && b.phase() == BattlePhase::EnemyTurn);
    assert(partyTotal(b) == start);                          // visual hilang, musuh BELUM menyerang
    runUntil(b, BattlePhase::Planning);
    assert(b.round() == 3 && partyTotal(b) < start);         // ...baru menyerang setelah jeda

    // Jeda itu tepat kStunEndDelay frame antara visual hilang dan serangan.
    Battle e;
    e.start();
    makeSturdy(e);
    const int estart = partyTotal(e);
    performAttack(hit(1, 1).withStun(100), e.party_.front[0], e.enemy_);
    playRound(e);
    e.execute();
    for (int i = 0; i < 100000 && e.enemy_.stunned; ++i) e.update();
    int frames = 0;
    while (partyTotal(e) == estart && e.phase() == BattlePhase::EnemyTurn && frames < 1000) { e.update(); ++frames; }
    assert(frames == kStunEndDelay);

    // Stun dipasang ulang di ronde 2: giliran musuh 2 dilewati lagi (1 + 1 = 2 skip berurutan).
    Battle c;
    c.start();
    makeSturdy(c);
    const int cstart = partyTotal(c);
    performAttack(hit(1, 1).withStun(100), c.party_.front[0], c.enemy_);
    playRound(c);
    performAttack(hit(1, 1).withStun(100), c.party_.front[0], c.enemy_);
    playRound(c);
    assert(c.round() == 3 && c.enemy_.stunned && partyTotal(c) == cstart);
    playRound(c);
    assert(!c.enemy_.stunned && partyTotal(c) < cstart);

    // DEF- pada musuh sengaja tidak pernah meluruh (supaya gampang ditumpuk): tetap ada
    // di perencanaan ronde 2 maupun setelah giliran musuh ronde 2.
    Battle d;
    d.start();
    makeSturdy(d);
    addEffect(d.enemy_, EffectType::DefDown, 30, 1);
    playRound(d);
    assert(d.round() == 2 && effectTotal(d.enemy_, EffectType::DefDown) == 30);
    playRound(d);
    assert(effectTotal(d.enemy_, EffectType::DefDown) == 30);
}

static void testStunStacking() {
    // Break dan stun skill pada skill yang sama menumpuk: 2 giliran musuh dilewati.
    Combatant boss = dummy(10, 500);
    boss.is_enemy = true;
    const AttackSummary a = performAttack(hit(1, 50).withStun(100), dummy(0, 1), boss);
    assert(a.broke && a.stunned && boss.stun_skips == 2);

    // Dua stun di giliran yang sama: musuh skip ronde 1 dan 2, visual tampil sampai awal
    // giliran musuh ronde 3, lalu musuh menyerang.
    std::srand(1);
    Battle b;
    b.start();
    makeSturdy(b);
    const int start = partyTotal(b);
    performAttack(hit(1, 1).withStun(100), b.party_.front[0], b.enemy_);
    performAttack(hit(1, 1).withStun(100), b.party_.front[1], b.enemy_);
    assert(b.enemy_.stun_skips == 2);
    playRound(b);                                            // giliran musuh 1: skip
    assert(b.round() == 2 && b.enemy_.stunned && partyTotal(b) == start);
    playRound(b);                                            // giliran musuh 2: skip
    assert(b.round() == 3 && b.enemy_.stunned && partyTotal(b) == start);
    playRound(b);                                            // giliran musuh 3: stun hilang, menyerang
    assert(b.round() == 4 && !b.enemy_.stunned && partyTotal(b) < start);
}

static void testOverdrive() {
    // Gauge terisi dari attack dasar (3 hit/ronde, tanpa skill): kOdHitsPerBar hit = 1 bar.
    std::srand(1);
    Battle g;
    g.start();
    makeSturdy(g);
    assert(g.odGauge() == 0 && g.odBarsReady() == 0);
    for (int i = 0; i * 3 < kOdHitsPerBar; ++i) playRound(g);
    assert(g.odGauge() == kOdHitsPerBar && g.odBarsReady() == 1);

    // Tidak bisa aktivasi tanpa bar, dan tidak bisa di luar fase Planning.
    std::srand(2);
    Battle b;
    b.start();
    makeSturdy(b);
    assert(!b.activateOverdrive());              // belum ada bar
    b.odBarsReady_ = 1;
    b.execute();
    assert(!b.activateOverdrive());               // sedang Executing, bukan Planning
    runUntil(b, BattlePhase::Planning);

    // Aktivasi: konsumsi seluruh bar, kasih SP instan ke party (boleh melebihi kMaxSP,
    // dibatasi kMaxSPOverdrive).
    b.odBarsReady_ = 2;
    b.odGauge_ = kOdHitsPerBar * 2;
    const int spBefore = b.party_.front[0].sp;
    assert(b.activateOverdrive());
    assert(b.odActive() && b.odBarsReady() == 0 && b.odGauge() == 0);
    assert(b.party_.front[0].sp == std::min(kMaxSPOverdrive, spBefore + kOdSpGrant[1]));   // 2 bar -> level 2

    // Giliran ekstra: stage 2 = kOdTotalTurns[1] (2) giliran, jadi 1 giliran ekstra TANPA giliran
    // musuh, baru sesudahnya normal lagi. odTurnsLeft() berkurang 1 tiap giliran ekstra terpakai.
    const int roundAtActivation = b.round();
    const int startTotal = partyTotal(b);
    assert(b.odTurnsLeft() == kOdTotalTurns[1] - 1);
    playRound(b);
    assert(b.round() == roundAtActivation && b.odActive() && partyTotal(b) == startTotal);
    assert(b.odTurnsLeft() == 0);
    playRound(b);                                  // giliran ekstra habis: giliran musuh jalan seperti biasa
    assert(b.round() == roundAtActivation + 1 && !b.odActive() && partyTotal(b) < startTotal);
    assert(b.odTurnsLeft() == 0);

    // Pengali damage: 1 ronde bersih (tanpa giliran ekstra) di bawah OD melukai DP musuh lebih
    // besar daripada 1 ronde biasa dengan penyerang yang sama.
    std::srand(7);
    Battle c;
    c.start();
    makeSturdy(c);
    playRound(c);
    const int dpAfterNormalRound = c.enemy_.dp;
    const int normalLoss = c.enemy_.max_dp - dpAfterNormalRound;
    c.odBarsReady_ = 1;
    c.odGauge_ = kOdHitsPerBar;
    assert(c.activateOverdrive());
    c.odExtraTurns_ = 0;                           // isolasi: giliran ekstra sudah dites di atas
    playRound(c);
    const int odLoss = dpAfterNormalRound - c.enemy_.dp;
    assert(odLoss > normalLoss);

    // SP dari OD boleh melebihi kMaxSP (sampai kMaxSPOverdrive), dan regen turn biasa
    // sesudahnya TIDAK menurunkannya balik ke kMaxSP.
    std::srand(3);
    Battle e;
    e.start();
    makeSturdy(e);
    e.party_.front[0].sp = kMaxSP;                 // sudah penuh dari regen biasa
    e.odBarsReady_ = 3;                            // level 3: +20 SP
    e.odGauge_ = kOdHitsPerBar * 3;
    assert(e.activateOverdrive());
    assert(e.party_.front[0].sp == kMaxSPOverdrive);   // 20 + 20 dibatasi 40, bukan dibatasi 20
    e.odExtraTurns_ = 0;                           // langsung lanjut ke ronde biasa berikutnya
    playRound(e);
    assert(e.party_.front[0].sp == kMaxSPOverdrive);   // regen turn biasa tidak menurunkannya
}

static void testOverdriveCounterAndNoChain() {
    // Counter "sisa/total": total = kOdTotalTurns[stage - 1] (1/2/3, termasuk giliran aktivasi),
    // sisa turun 1 tiap giliran; nol (dan level 0) begitu OD selesai. OD tidak bisa diaktifkan
    // lagi selama masih aktif, walau ada bar yang siap.
    assert(kOdTotalTurns[0] == 1 && kOdTotalTurns[1] == 2 && kOdTotalTurns[2] == 3);
    std::srand(4);
    Battle b;
    b.start();
    makeSturdy(b);
    assert(b.odTurnsRemaining() == 0 && b.odTurnsTotal() == 0 && b.odLevel() == 0);

    // Stage 3: 3 giliran (3/3, 2/3, 1/3), lalu mati.
    b.odBarsReady_ = 3;
    b.odGauge_ = kOdHitsPerBar * 3;
    assert(b.activateOverdrive());
    assert(b.odTurnsRemaining() == 3 && b.odTurnsTotal() == 3 && b.odLevel() == 3);

    // Bar baru terkumpul saat OD aktif, tapi tombol OD tidak boleh jalan.
    b.odBarsReady_ = 1;
    b.odGauge_ = kOdHitsPerBar;
    const int mult = b.odMultPct_;
    const int sp = b.party_.front[0].sp;
    assert(!b.activateOverdrive());
    assert(b.odBarsReady() == 1 && b.odGauge() == kOdHitsPerBar);
    assert(b.odTurnsRemaining() == 3 && b.odTurnsTotal() == 3 && b.odMultPct_ == mult);
    assert(b.party_.front[0].sp == sp);

    for (int left = 2; left >= 1; --left) {
        playRound(b);
        assert(b.odActive() && b.odTurnsRemaining() == left && b.odTurnsTotal() == 3);
        assert(!b.activateOverdrive());
    }
    playRound(b);                                  // giliran terakhir selesai: OD mati
    assert(!b.odActive());
    assert(b.odTurnsRemaining() == 0 && b.odTurnsTotal() == 0 && b.odLevel() == 0);

    // Bar yang terkumpul selama OD tetap ada dan bisa dipakai setelah OD selesai.
    // Stage 1: cuma 1 giliran (1/1), OD mati begitu ronde itu selesai.
    assert(b.odBarsReady() >= 1);
    b.odBarsReady_ = 1;
    assert(b.activateOverdrive());
    assert(b.odTurnsRemaining() == 1 && b.odTurnsTotal() == 1 && b.odLevel() == 1);
    playRound(b);
    assert(!b.odActive() && b.odTurnsRemaining() == 0);

    // Stage 2: 2 giliran (2/2, 1/2).
    b.odBarsReady_ = 2;
    assert(b.activateOverdrive());
    assert(b.odTurnsRemaining() == 2 && b.odTurnsTotal() == 2 && b.odLevel() == 2);
    playRound(b);
    assert(b.odActive() && b.odTurnsRemaining() == 1 && b.odTurnsTotal() == 2);
    playRound(b);
    assert(!b.odActive() && b.odTurnsRemaining() == 0);
}

static void testOverdriveAwakening() {
    // Preserve charged gauge and active OD across the boss transition. Awakening
    // still starts its normal next round, without another OD SP grant.
    for (int bars = 0; bars <= kOdMaxBars; ++bars) {
        Battle b;
        b.start();
        b.swapSlot(0, 1);
        const std::string frontName = b.party().front[0].name;
        b.party_.front[0].hp = 51;
        b.party_.front[0].dp = 7;
        b.party_.front[0].uses_left[0] = 3;
        addEffect(b.party_.back[0], EffectType::DefUp, 20, 3);
        b.odGauge_ = kOdHitsPerBar * bars;
        b.odBarsReady_ = bars;
        if (bars > 0) assert(b.activateOverdrive());
        b.odGauge_ = kOdHitsPerBar - 1;
        b.enemy_.dp = 0;
        b.enemy_.hp = 1;
        b.enemy_.broken = true;
        b.execute();
        runUntil(b, BattlePhase::Awakening);
        assert(b.odGauge() == kOdHitsPerBar && b.odBarsReady() == 1);
        assert(!b.activateOverdrive());
        const Party before = b.party();
        const int extraTurns = b.odExtraTurns_;
        const int multiplier = b.odMultPct_;
        runUntil(b, BattlePhase::Planning);
        assert(b.awakened() && b.round() == 2);
        assert(b.enemy().dp == 160 && b.enemy().hp == 1300);
        assert(b.enemy().max_devastation == kMaxDevastationAwaken);
        assert(b.odGauge() == kOdHitsPerBar && b.odBarsReady() == 1);
        assert(b.odActive() == (bars > 0));
        assert(b.odExtraTurns_ == extraTurns && b.odMultPct_ == multiplier);
        assert(b.party().front[0].name == frontName);
        for (int i = 0; i < 3; ++i) {
            assert(b.party().front[i].hp == before.front[i].hp);
            assert(b.party().front[i].dp == before.front[i].dp);
            assert(b.party().front[i].uses_left == before.front[i].uses_left);
            // Regen turn biasa tidak pernah menurunkan SP yang sudah di atas kMaxSP
            // (sisa SP instan Overdrive, lihat kMaxSPOverdrive).
            assert(b.party().front[i].sp == (before.front[i].sp >= kMaxSP
                ? before.front[i].sp : std::min(kMaxSP, before.front[i].sp + kSpRegenFront)));
            assert(b.party().back[i].sp == (before.back[i].sp >= kMaxSP
                ? before.back[i].sp : std::min(kMaxSP, before.back[i].sp + kSpRegenBack)));
        }
        assert(effectTotal(b.party().back[0], EffectType::DefUp) == 20);
        assert(b.queue().empty() && b.activeSlot() == -1);
        makeSturdy(b);
        const int total = partyTotal(b);
        for (int turn = 0; turn < extraTurns; ++turn) {
            playRound(b);
            assert(b.round() == 2 && b.odActive() && partyTotal(b) == total);
        }
        playRound(b);
        assert(b.round() == 3 && !b.odActive() && partyTotal(b) < total);
        b.enemy_.dp = 0;
        b.enemy_.hp = 1;
        b.execute();
        runUntil(b, BattlePhase::Victory);
        b.start();
        assert(!b.awakened() && !b.odActive());
        assert(b.odGauge() == 0 && b.odBarsReady() == 0);
        assert(b.odExtraTurns_ == 0 && b.odMultPct_ == 100);
    }
}

static void testAtkUpOnlyBySkill() {
    // Attack biasa: tanpa bonus ATK+ dan tidak menghabiskannya. Skill serangan: kena bonus, lalu habis.
    Combatant att = dummy(0, 1);
    att.atk = 10;
    addEffect(att, EffectType::AtkUp, 100, 0);
    Combatant boss = dummy(1000, 1000);
    boss.is_enemy = true;
    AttackSummary basic = performAttack(normalAttackOf(att), att, boss);
    assert(basic.dp_lost == 10);
    AttackSummary skill = performAttack(hit(1, 10), att, boss);
    assert(skill.dp_lost == 20);

    // Lewat Battle: attack biasa ronde 1 tidak menghabiskan buff, skill ronde 2 menghabiskannya.
    std::srand(1);
    Battle b;
    b.start();
    makeSturdy(b);
    addEffect(b.party_.front[0], EffectType::AtkUp, 40, 0);       // Ruka
    playRound(b);                                                  // semua attack biasa
    assert(effectTotal(b.party_.front[0], EffectType::AtkUp) == 40);
    assert(b.setCommand(0, CommandType::Skill, 1));                // Cross Cut
    playRound(b);
    assert(effectTotal(b.party_.front[0], EffectType::AtkUp) == 0);
}

static void testDialogue() {
    DialogueScene d;
    d.load(makePrologueScript());
    assert(!d.finished() && d.count() == 5);
    assert(d.current()->speaker == "Nanami Nanase" && d.current()->text == "Visual telah dikonfirmasi. Hellspider kini terlihat di tengah kota.");
    d.advance();
    assert(d.lineComplete());
    d.advance();
    assert(d.index() == 1);

    // Melewati seluruh baris satu per satu sampai selesai.
    DialogueScene all;
    all.load(makePrologueScript());
    size_t lines = 0;
    while (!all.finished()) {
        all.advance();          // tampilkan semua
        all.advance();          // baris berikutnya
        ++lines;
    }
    assert(lines == 5);

    d.skipAll();
    assert(d.finished() && d.current() == nullptr);

    // Riwayat untuk tombol Log: baris sekarang hanya menampilkan bagian yang sudah tampil.
    DialogueScene h;
    h.load(makePrologueScript());
    assert(h.historyCount() == 1 && h.logText(0).empty());
    h.advance();                                        // tampilkan seluruh baris 1
    assert(h.logText(0) == "Visual telah dikonfirmasi. Hellspider kini terlihat di tengah kota.");
    h.advance();                                        // baris 2 mulai
    assert(h.historyCount() == 2 && h.logText(0) == "Visual telah dikonfirmasi. Hellspider kini terlihat di tengah kota." && h.logText(1).empty());
    for (int i = 0; i < 10; ++i) h.update();            // mesin ketik jalan sebagian
    assert(!h.logText(1).empty() && h.logText(1) == h.visibleText());
    assert(h.logText(99).empty());
    assert(h.lineAt(1).speaker == "Nanami Nanase");
}

// ---- simulasi ----------------------------------------------------------

enum class Policy { Random, Smart, SmartNoFx };

static bool has(const Combatant& c, EffectType t) { return effectTotal(c, t) > 0; }

static int fxScore(const Battle& b, const SkillEffect& fx) {
    switch (fx.type) {
    case EffectType::AtkUp:
    case EffectType::DevUp: {
        for (const auto& c : b.party().front) if (has(c, fx.type)) return 0;
        return fx.type == EffectType::AtkUp ? 45 : 20;
    }
    case EffectType::DefUp: {
        for (const auto& c : b.party().front) if (has(c, fx.type)) return 0;
        return 10;
    }
    case EffectType::DefDown:
    case EffectType::AtkDown:
        return has(b.enemy(), fx.type) ? 0 : 12;
    default:
        return 0;
    }
}

static void plan(Battle& b, Policy pol) {
    for (int slot = 0; slot < 3; ++slot) {
        if (pol == Policy::Random) {
            if (std::rand() % 4 == 0) b.swapSlot(slot, std::rand() % 3);
            const int k = std::rand() % 3;
            if (k > 0) b.setCommand(slot, CommandType::Skill, k - 1);
            continue;
        }
        // Smart: tukar karakter yang SP/HP-nya habis dengan back row yang segar,
        // lalu pilih skill dengan nilai terbaik untuk kondisi saat ini.
        const Combatant& c = b.party().front[slot];
        if (c.sp < 3 || c.hp * 100 < c.max_hp * 40) {
            int best = -1;
            for (int i = 0; i < 3; ++i) {
                const Combatant& bk = b.party().back[i];
                if (bk.sp >= 4 && bk.hp * 100 >= bk.max_hp * 50 &&
                    (best < 0 || bk.sp > b.party().back[best].sp)) best = i;
            }
            if (best >= 0) b.swapSlot(slot, best);
        }
        const Combatant& n = b.party().front[slot];
        const bool broken = b.enemy().broken;
        int pick = -1, score = 0;
        for (int i = 0; i < 2; ++i) {
            if (!b.canUseSkill(slot, i)) continue;
            const Skill& s = n.skills[i];
            const bool useFx = (pol == Policy::Smart);
            int sc = 0;
            if (s.kind == SkillKind::HealDP) {
                bool need = false;
                for (const auto& f : b.party().front) if (f.broken) need = true;
                sc = need ? 1000 : 0;
            } else if (s.kind == SkillKind::Support) {
                sc = useFx ? fxScore(b, s.fx) : 0;
            } else {
                sc = s.hits * s.power * (broken ? s.hp_pct : s.dp_pct) / 100;
                // Saat musuh sedang break, kenaikan devastation rate ikut dihargai
                // (sekitar 1 poin damage tim per 1% rate).
                if (broken) sc += s.hits * std::max(1, s.dev_mult) * kDevastationPerHit;
                if (useFx) sc += fxScore(b, s.fx) / 2;
            }
            if (sc > score) { score = sc; pick = i; }
        }
        if (pick >= 0) b.setCommand(slot, CommandType::Skill, pick);
    }
}

static void simulate(Policy pol, const char* label) {
    int wins = 0, losses = 0;
    long rounds = 0, totalBreaks = 0, totalStuns = 0;
    const int N = 400;
    for (int seed = 0; seed < N; ++seed) {
        std::srand(seed);
        Battle b;
        b.start();
        int frames = 0;
        int enemyLostByFx = 0, enemyBreaks = 0, enemyStuns = 0;
        while (b.phase() != BattlePhase::Victory && b.phase() != BattlePhase::Defeat) {
            if (b.phase() == BattlePhase::Planning) {
                plan(b, pol);
                b.execute();
            }
            b.update();
            assert(++frames < 200000);
            for (const auto& c : b.party().front) {
                assert(c.sp >= 0 && c.sp <= kMaxSP && c.dp >= 0 && c.dp <= c.max_dp && c.hp >= 0);
                assert(c.effects.size() <= 3 * kMaxEffectStacks);
            }
            for (const auto& c : b.party().back) assert(c.sp >= 0 && c.sp <= kMaxSP);
            assert(b.enemy().dp >= 0 && b.enemy().hp >= 0);

            // Event visual: konsisten dengan hasil sebenarnya.
            const std::vector<FxEvent> ev = b.takeFx();
            int lastDelay = 0;
            for (std::size_t i = 0; i < ev.size(); ++i) {
                const FxEvent& e = ev[i];
                assert(e.slot >= 0 && e.slot < 3 && e.delay >= lastDelay);
                lastDelay = e.delay;
                if (e.kind == FxKind::Damage) {
                    assert((e.dp > 0) != (e.hp > 0));      // satu hit: DP atau HP, tidak keduanya
                    if (e.on_enemy) enemyLostByFx += e.dp + e.hp;
                } else if (e.kind == FxKind::Break) {
                    assert(i > 0 && ev[i - 1].kind == FxKind::Damage && ev[i - 1].on_enemy == e.on_enemy &&
                           ev[i - 1].slot == e.slot);      // break selalu menyusul hit yang memecahkan DP
                    if (e.on_enemy) ++enemyBreaks;
                } else if (e.kind == FxKind::Stun) {
                    assert(e.on_enemy && e.dp == 0 && e.hp == 0);
                    assert(i > 0 && ev[i - 1].on_enemy &&
                           (ev[i - 1].kind == FxKind::Damage || ev[i - 1].kind == FxKind::Break));
                    ++enemyStuns;
                } else {
                    assert(!e.on_enemy && e.dp > 0);
                }
            }
        }
        const Combatant& boss = b.enemy();
        // Fase 1 (sebelum tuning "santai") punya max_dp+max_hp = 90+800 = 890; sekarang 55+500 = 555.
        // Nilai ini dijumlahkan balik karena boss fase 1 sudah pasti dikalahkan penuh sebelum awaken.
        assert(enemyLostByFx == (boss.max_dp - boss.dp) + (boss.max_hp - boss.hp)
               + (b.awakened() ? 555 : 0));
        totalBreaks += enemyBreaks;
        totalStuns += enemyStuns;
        rounds += b.round();
        (b.phase() == BattlePhase::Victory ? wins : losses)++;
    }
    assert(totalBreaks > 0);                              // event Break memang muncul di simulasi
    assert(totalStuns > 0);                               // event Stun dari skill andalan muncul di simulasi
    std::printf("%-10s menang %3d / kalah %3d  (rata-rata %.1f ronde, %ld break musuh)\n", label, wins,
                losses, static_cast<double>(rounds) / N, totalBreaks);
}

static void testAwakenBattle() {
    Battle b;
    b.start();
    assert(!b.awakened());
    b.party_.front[0].hp = 51;
    b.party_.front[0].dp = 7;
    b.party_.front[0].uses_left[0] = 3;
    b.enemy_.dp = 0;
    b.enemy_.hp = 1;
    b.enemy_.broken = true;
    b.enemy_.stunned = true;
    b.enemy_.stun_skips = 2;
    addEffect(b.enemy_, EffectType::DefDown, 30, 2);
    b.execute();
    for (int i = 0; i < 1000 && b.phase() == BattlePhase::Executing; ++i) b.update();
    assert(b.phase() == BattlePhase::Awakening && !b.enemy().alive());
    const int sp = b.party().front[0].sp;
    const int waitingSp = b.party().front[1].sp;
    assert(!b.setCommand(0, CommandType::Attack));
    b.execute(); // input during the transition must not interrupt it
    assert(b.phase() == BattlePhase::Awakening);
    for (int i = 0; i < 60; ++i) b.update();
    assert(b.phase() == BattlePhase::Awakening && !b.awakened());
    for (int i = 0; i < 1000 && b.phase() == BattlePhase::Awakening; ++i) b.update();
    assert(b.phase() == BattlePhase::Planning && b.awakened());
    assert(b.enemy().hp == 1300 && b.enemy().dp == 160);
    assert(!b.enemy().broken && !b.enemy().stunned && b.enemy().devastation == 100);
    assert(effectTotal(b.enemy(), EffectType::DefDown) == 0);
    assert(b.enemy().stun_skips == 0 && !b.enemyActPending_);
    assert(b.party().front[0].hp == 51 && b.party().front[0].dp == 7);
    assert(b.party().front[0].uses_left[0] == 3);
    assert(b.party().front[0].sp == sp + kSpRegenFront);
    assert(b.party().front[1].sp == waitingSp + kSpRegenFront);
    assert(b.queue().empty() && b.round() == 2 && b.activeSlot() == -1);
    b.enemy_.hp = 1;
    b.enemy_.dp = 0;
    b.execute();
    for (int i = 0; i < 1000 && b.phase() == BattlePhase::Executing; ++i) b.update();
    assert(b.phase() == BattlePhase::Dying && b.awakened());
    runUntil(b, BattlePhase::Victory);
    for (int i = 0; i < 200; ++i) b.update();
    assert(b.phase() == BattlePhase::Victory);
    b.start();
    assert(!b.awakened() && b.phase() == BattlePhase::Planning && b.round() == 1);
}

static void testFinalDeathCutscene() {
    Battle b;
    b.start();
    b.awakened_ = true;
    b.enemy_.dp = 0;
    b.enemy_.hp = 25;
    b.party_.front[0].sp = 20;
    b.party_.front[0].skills[0] = hit(5, 10);
    b.setCommand(0, CommandType::Skill, 0);
    b.execute();
    runUntil(b, BattlePhase::Dying);
    const auto events = b.takeFx();
    int last = 0, damageEvents = 0;
    for (const auto& e : events) {
        last = std::max(last, e.delay);
        if (e.kind == FxKind::Damage) ++damageEvents;
    }
    assert(damageEvents == 3 && last > 0);
    assert(b.defeatFrame() == -(last + 60));
    const int sp = b.party().front[0].sp, hp = b.party().front[0].hp, round = b.round();
    const std::string frontName = b.party().front[0].name;
    for (int i = 0; i < last + 60; ++i) {
        assert(b.defeatFrame() < 0);
        b.update();
    }
    for (int f = 0; f < defeat_cutscene::Duration; ++f) {
        assert(b.phase() == BattlePhase::Dying && b.defeatFrame() == f);
        assert(!b.setCommand(0, CommandType::Attack));
        assert(!b.activateOverdrive());
        b.execute(); b.swapSlot(0, 0);
        assert(b.party().front[0].name == frontName);
        assert(b.party().front[0].hp == hp && b.party().front[0].sp == sp && b.round() == round);
        const auto p = defeat_cutscene::sample(f);
        assert(std::isfinite(p.shake) && p.growth >= 0.f && p.growth <= 1.f);
        assert(p.fragments == (f >= defeat_cutscene::Burst && f < 156));
        assert(p.tree == (f >= defeat_cutscene::TreeStart));
        b.update();
    }
    assert(b.phase() == BattlePhase::Victory && b.defeatFrame() == defeat_cutscene::Duration);
    for (int i = 0; i < 100; ++i) b.update();
    assert(b.defeatFrame() == defeat_cutscene::Duration && b.queue().empty());
    b.start();
    assert(b.defeatFrame() == -1 && !b.awakened());
}

template<size_t V, size_t I>
static void validateTreeMesh(const white_tree::Vertex (&v)[V], const unsigned short (&idx)[I]) {
    assert(V < 65536 && I % 3 == 0);
    for (size_t i = 0; i < V; ++i) {
        assert(std::isfinite(v[i].x) && std::isfinite(v[i].y) && std::isfinite(v[i].z));
        assert(v[i].y >= -.5f && v[i].y <= 6.5f);
    }
    for (size_t i = 0; i < I; ++i) assert(idx[i] < V);
}

static void testDefeatSoundCues() {
    using namespace defeat_cutscene;
    unsigned seen = 0;
    for (int f = -90; f <= Duration + 30; ++f) {
        unsigned cues = soundCues(f - 1, f);
        assert((seen & cues) == 0);
        seen |= cues;
        assert(soundCues(f, f) == 0); // held frame / Victory cannot retrigger
        if (f != 0 && f != Burst && f != TreeStart && f != Settle) assert(cues == 0);
    }
    assert(seen == (PressureCue | ShatterCue | RootsCue | SettleCue));
    assert(soundCues(-1, 0) == PressureCue); // next battle starts with a fresh timeline
    assert(soundCues(Burst-1, TreeStart) == (ShatterCue | RootsCue)); // crossed milestones
    assert(soundCues(Duration, -1) == 0); // restart reset is silent
}

int main() {
    testDefeatSoundCues();
    testFinalDeathCutscene();
    validateTreeMesh(white_tree::light_vertices, white_tree::light_indices);
    validateTreeMesh(white_tree::shade_vertices, white_tree::shade_indices);
    validateTreeMesh(white_tree::hole_vertices, white_tree::hole_indices);
    testAwakenBattle();
    assert(kMaxSP == 20);
    testHitRules();
    testEffects();
    testPlanning();
    testSwapWithinRows();
    testBuffFlow();
    testBlasterDevastation();
    testDefaultSkillBonuses();
    testOverflowRule();
    testNoAutoRecovery();
    testSkillList();
    testStunAndSpGain();
    testFxEvents();
    testStunEvents();
    testTurnTimeline();
    testStunStacking();
    testOverdrive();
    testOverdriveCounterAndNoChain();
    testOverdriveAwakening();
    testAtkUpOnlyBySkill();
    testDialogue();
    simulate(Policy::Random, "random");
    simulate(Policy::SmartNoFx, "tanpa-fx");
    simulate(Policy::Smart, "smart");
    std::puts("OK");
    return 0;
}
