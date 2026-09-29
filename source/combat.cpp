#include "combat.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace {
// Menambah SP tanpa pernah MENURUNKANNYA: kalau SP sudah di atas 'cap' (sisa SP instan
// Overdrive yang bisa melebihi kMaxSP), regen/gimmick SP normal tidak berbuat apa-apa,
// bukan memotongnya balik ke cap.
int gainSpCapped(int sp, int amount, int cap) {
    return sp >= cap ? sp : std::min(cap, sp + amount);
}
}  // namespace


HitResult applyHit(Combatant& t, int dp_dmg, int hp_dmg) {
    HitResult r;
    if (!t.alive()) return r;

    // Aturan overflow: satu hit yang menghabiskan DP TIDAK melukai HP. Sisa damage hit itu
    // dibuang. Hit berikutnya (mis. hit ke-2 pada skill multi-hit) baru melukai HP dengan
    // damage penuhnya sendiri.
    if (t.dp > 0) {
        r.dp_lost = std::min(t.dp, dp_dmg);
        t.dp -= r.dp_lost;
        if (t.dp == 0) {
            t.broken = true;
            r.just_broken = true;
        }
        return r;
    }

    r.hp_lost = std::min(t.hp, hp_dmg);
    t.hp -= r.hp_lost;
    r.killed = !t.alive();
    return r;
}

AttackSummary performAttack(const Skill& s, const Combatant& att, Combatant& t) {
    AttackSummary sum;

    // Efek tidak berubah di tengah satu skill, jadi cukup dihitung sekali.
    // Attack biasa tidak memakai ATK+/DEV+ (hanya skill serangan yang memakainya).
    const int atkUp   = s.is_basic ? 0 : effectTotal(att, EffectType::AtkUp);
    const int atkDown = std::min(kMaxAtkDownPct, effectTotal(att, EffectType::AtkDown));
    const int devUp  = s.is_basic ? 0 : effectTotal(att, EffectType::DevUp);
    // Devastation rate per hit = dasar x pengali milik skill (bukan milik role/karakter).
    const int devMult = std::max(1, s.dev_mult);
    const int defDown = effectTotal(t, EffectType::DefDown);
    const int defUp   = std::min(kMaxDefUpPct, effectTotal(t, EffectType::DefUp));

    auto scale = [&](int v) {
        long x = v;
        x = x * (100 + atkUp)   / 100;    // jenis berbeda saling dikali,
        x = x * (100 - atkDown) / 100;    // tumpukan sejenis sudah dijumlah aditif
        x = x * (100 + defDown) / 100;
        x = x * (100 - defUp)   / 100;
        return std::max(1, static_cast<int>(x));
    };

    for (int i = 0; i < s.hits && t.alive(); ++i) {
        const int dp_dmg = scale(s.power * s.dp_pct / 100);
        int hp_dmg = scale(s.power * s.hp_pct / 100);
        if (t.is_enemy) hp_dmg = std::max(1, hp_dmg * t.devastation / 100);

        const bool wasBroken = t.broken;
        const HitResult r = applyHit(t, dp_dmg, hp_dmg);
        sum.hit_list.push_back(r);
        ++sum.hits;
        sum.dp_lost += r.dp_lost;
        sum.hp_lost += r.hp_lost;
        sum.killed = sum.killed || r.killed;

        if (t.is_enemy) {
            // Devastation rate HANYA naik oleh hit yang mengenai musuh yang sudah break.
            // Hit yang memecahkan DP sendiri (wasBroken == false) tidak menaikkannya.
            if (wasBroken) {
                const int gain = kDevastationPerHit * devMult + devUp;
                t.devastation = std::min(t.max_devastation, t.devastation + gain);
            }
            if (r.just_broken) {
                t.stunned = true;
                ++t.stun_skips;
                sum.broke = true;
            }
        } else if (r.just_broken) {
            sum.broke = true;
        }
    }

    if (s.stun_chance > 0 && t.is_enemy && t.alive() && std::rand() % 100 < s.stun_chance) {
        t.stunned = true;
        ++t.stun_skips;                              // menumpuk dengan stun dari break/skill lain
        sum.stunned = true;
    }
    return sum;
}


int effectTotal(const Combatant& c, EffectType type) {
    int sum = 0;
    for (const auto& e : c.effects)
        if (e.type == type) sum += e.value;
    return sum;
}

void addEffect(Combatant& c, EffectType type, int value, int turns) {
    if (type == EffectType::None) return;
    int count = 0;
    for (const auto& e : c.effects)
        if (e.type == type) ++count;
    if (count >= kMaxEffectStacks) {                 // penuh: tumpukan tertua diganti
        for (auto it = c.effects.begin(); it != c.effects.end(); ++it) {
            if (it->type == type) { c.effects.erase(it); break; }
        }
    }
    c.effects.push_back(StatusEffect{ type, value, turns, true });
}

void tickEffects(Combatant& c) {
    if (c.is_enemy) return;   // debuff musuh sengaja tidak berkurang durasinya, supaya gampang ditumpuk
    for (auto it = c.effects.begin(); it != c.effects.end();) {
        if (it->fresh) { it->fresh = false; ++it; continue; }     // ronde pemasangan tidak dihitung
        if (it->turns > 0 && --it->turns == 0) it = c.effects.erase(it);
        else ++it;
    }
}

void consumeOneTimeBuffs(Combatant& c) {
    for (auto it = c.effects.begin(); it != c.effects.end();) {
        if (it->type == EffectType::AtkUp || it->type == EffectType::DevUp) it = c.effects.erase(it);
        else ++it;
    }
}

namespace {
const char* effectTag(EffectType t) {
    switch (t) {
    case EffectType::AtkUp:   return "ATK+";
    case EffectType::DefUp:   return "DEF+";
    case EffectType::DevUp:  return "DEV+";
    case EffectType::DefDown: return "DEF-";
    case EffectType::AtkDown: return "ATK-";
    default:                  return "";
    }
}
}  // namespace

std::string effectSummary(const Combatant& c, unsigned skipMask) {
    static const EffectType kOrder[] = { EffectType::AtkUp, EffectType::DefUp, EffectType::DevUp,
                                         EffectType::DefDown, EffectType::AtkDown };
    std::string out;
    for (EffectType t : kOrder) {
        if (skipMask & effectBit(t)) continue;
        int n = 0, maxTurns = 0;
        for (const auto& e : c.effects) {
            if (e.type != t) continue;
            ++n;
            maxTurns = std::max(maxTurns, e.turns);
        }
        if (n == 0) continue;
        char buf[24];
        if (!out.empty()) out += ' ';
        out += effectTag(t);
        if (n > 1) { std::snprintf(buf, sizeof buf, "x%d", n); out += buf; }
        if (maxTurns > 0) { std::snprintf(buf, sizeof buf, "(%dt)", maxTurns); out += buf; }
    }
    return out;
}

std::string effectLabel(const SkillEffect& fx) {
    if (fx.type == EffectType::None) return std::string();
    char buf[48];
    std::snprintf(buf, sizeof buf, fx.type == EffectType::DevUp ? "%s%d" : "%s%d%%",
                  effectTag(fx.type), fx.value);
    std::string out = buf;
    if (fx.turns > 0) {
        std::snprintf(buf, sizeof buf, "(%dt)", fx.turns);
        out += buf;
    }
    if (fx.scope == EffectScope::AllFront || fx.scope == EffectScope::AllParty) out += " team";
    else if (fx.scope == EffectScope::Enemy) out += " enemy";
    return out;
}

Skill normalAttackOf(const Combatant& c) {
    Skill s("Attack", SkillKind::Attack, 0, 1, c.atk);
    s.is_basic = true;
    return s;
}

void recoverFromBreak(Combatant& c) {
    c.dp = c.max_dp;
    c.broken = false;
    c.devastation = 100;
}

void healPartyDP(Party& p, int amount) {
    auto healOne = [amount](Combatant& c) {
        if (!c.alive()) return;
        c.dp = std::min(c.max_dp, c.dp + amount);
        if (c.dp > 0) c.broken = false;              // heal DP = pulih dari break
    };
    for (auto& c : p.front) healOne(c);
    for (auto& c : p.back)  healOne(c);
}


const char* roleName(int idx) {
    static const char* const kNames[kRoleCount] = { "Attacker", "Breaker", "Blaster",
                                                    "Healer", "Buffer", "Debuffer" };
    return idx >= 0 && idx < kRoleCount ? kNames[idx] : "";
}

int roleIndex(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < kRoleCount; ++i) {
        const char* a = name;
        const char* b = roleName(i);
        while (*a && *b && std::tolower(static_cast<unsigned char>(*a)) ==
                           std::tolower(static_cast<unsigned char>(*b))) { ++a; ++b; }
        if (!*a && !*b) return i;
    }
    return -1;
}

bool setRole(Combatant& c, const char* role) {
    const int idx = roleIndex(role);
    if (idx < 0) return false;
    c.role = roleName(idx);
    return true;
}

const char* skillTag(const Skill& s) {
    if (s.kind != SkillKind::Attack) return "";
    if (s.hp_pct > 100)  return "[HP Eff]";
    if (s.dp_pct > 100)  return "[DP Eff]";
    if (s.dev_mult > 1)  return "[High Devastation]";
    return "";
}

Party makeDefaultParty() {
    Party p;
    // Angka damage/SP adalah rancangan sendiri (TUNING).
    p.front = {{
        Combatant("Ruka", "Attacker", 30, 70,  9,
                  Skill("Ephemeral Cascade", SkillKind::Attack, 12, 9, 6).asEx().withBonus(SkillBonus::HpEff)
                      .withDescription("Flits through the air to deal a 9-hit attack to a "
                                       "single enemy."),
                  Skill("Cross Cut",         SkillKind::Attack, 6, 2, 8).withBonus(SkillBonus::HpEff)
                      .withDescription("Slashes an enemy with twin blades.")),
        Combatant("Yuki", "Breaker", 20, 45, 12,
                  Skill("Meteor Shower",     SkillKind::Attack, 11, 6, 8).asEx().withBonus(SkillBonus::DpEff)
                      .withDescription("Rains down a powerful salvo on all enemies."),
                  Skill("Break Booster",     SkillKind::Attack, 4, 3, 7).withBonus(SkillBonus::DpEff)
                      .withDescription("Fires a salvo on all enemies.")),
        Combatant("Tama", "Healer", 30, 60, 7,
                  Skill("Resupply",          SkillKind::HealDP, 8, 0, 20).withUses(10)
                      .withDescription("Envelops all allies in a gentle aura that moderately "
                                       "restores their DP."),
                  Skill("Saltire Slash",     SkillKind::Attack, 7, 2, 8).withSpGain(4, 50)
                      .withDescription("Unleashes a valiant slash on all enemies, with a chance "
                                       "of restoring this unit's SP.")),
    }};
    p.back = {{
        Combatant("Karen", "Blaster", 30, 55, 8,
                  Skill("Bloody Escapade",   SkillKind::Attack, 11, 10, 4)
                      .asEx().withBonus(SkillBonus::Devastation)
                      .withDescription("Deals a 10-hit slash attack from all directions."),
                  Skill("Wild Fling",        SkillKind::Attack, 7, 3, 5)
                      .withBonus(SkillBonus::Devastation)
                      .withDescription("Hurls a scythe to chop up all enemies.")),
        Combatant("Megumi", "Debuffer", 30, 60, 7,
                  Skill("Excelsior Impact",  SkillKind::Attack, 10, 2, 8)
                      .asEx().withStun(70)
                      .withDescription("Strikes from up high with a power that surpasses one's "
                                       "limits. Has a high chance to stun."),
                  Skill("Hard Knocks",       SkillKind::Attack, 8, 1, 14)
                      .withEffect(EffectType::DefDown, 30, 1, EffectScope::Enemy)
                      .withDescription("Crushes down forcefully on a single enemy, reducing "
                                       "their DEF.")),
        Combatant("Tsukasa", "Buffer", 35, 70, 8,
                  makeSupport("Full Enhance", 9, EffectType::AtkUp, 40, 0, EffectScope::AllParty)
                      .withDescription("Awakens the potential of the entire squad, increasing "
                                       "their Skill ATK."),
                  Skill("Blessed Shot",      SkillKind::Attack, 7, 1, 14).withSpGain(4, 50)
                      .withDescription("Launches a focused shot on an enemy, with a chance of "
                                       "restoring this unit's SP.")),
    }};
    return p;
}

void Battle::start() {
    party_ = hasPartyOverride_ ? partyOverride_ : makeDefaultParty();

    // TUNING: fase 1 dibuat lebih santai (DP/HP lebih rendah) sebagai kontras untuk fase Awaken.
    enemy_ = Combatant("Hellspider", "Boss", 55, 500, 0, Skill(), Skill());
    enemy_.is_enemy = true;
    enemy_.sp = 0;

    awakened_ = false;
    timer_ = 0;
    round_   = 0;
    enemyActPending_ = false;
    fx_.clear();
    message_ = "Hellspider appears!";

    odGauge_        = 0;
    odBarsReady_    = 0;
    odActive_        = false;
    odExtraTurns_   = 0;
    odTotalTurns_   = 0;
    odLevel_        = 0;
    odMultPct_       = 100;

    startRound();
}

void Battle::gainOdGauge(int hits) {
    if (odBarsReady_ >= kOdMaxBars) return;
    odGauge_ += hits;
    odBarsReady_ = std::min(kOdMaxBars, odGauge_ / kOdHitsPerBar);
}

bool Battle::activateOverdrive() {
    if (phase_ != BattlePhase::Planning) return false;
    if (odActive_) return false;
    if (odBarsReady_ <= 0) return false;

    odMultPct_    = kOdMultPct[odBarsReady_ - 1];
    odLevel_      = odBarsReady_;
    odTotalTurns_ = kOdTotalTurns[odLevel_ - 1];   // sudah termasuk giliran aktivasi
    odExtraTurns_ = odTotalTurns_ - 1;
    odActive_     = true;

    const int spGrant = kOdSpGrant[odBarsReady_ - 1];
    for (auto& c : party_.front) c.sp = std::min(kMaxSPOverdrive, c.sp + spGrant);
    for (auto& c : party_.back)  c.sp = std::min(kMaxSPOverdrive, c.sp + spGrant);

    odGauge_     = 0;
    odBarsReady_ = 0;
    message_ = "OVERDRIVE!";
    return true;
}

void Battle::recordHits(const AttackSummary& a, bool onEnemy, int slot, int baseDelay,
                         int attackerSlot) {
    int delay = baseDelay;
    for (const HitResult& h : a.hit_list) {
        if (h.dp_lost > 0 || h.hp_lost > 0) {
            FxEvent e;
            e.kind = FxKind::Damage;
            e.on_enemy = onEnemy;
            e.slot = slot;
            e.dp = h.dp_lost;
            e.hp = h.hp_lost;
            e.delay = delay;
            e.attacker_slot = attackerSlot;
            fx_.push_back(e);
            fxSpan_ = std::max(fxSpan_, e.delay);
        }
        if (h.just_broken) {
            FxEvent b;
            b.kind = FxKind::Break;
            b.on_enemy = onEnemy;
            b.slot = slot;
            b.delay = delay + 2;                        // sesaat setelah angka hit yang memecahkan DP
            fx_.push_back(b);
            fxSpan_ = std::max(fxSpan_, b.delay);
        }
        delay += kFxHitGap;
    }

    // Stun dari skill (break sudah punya event Break sendiri). Muncul sesaat setelah hit terakhir.
    if (onEnemy && a.stunned) {
        const int lastHit = a.hit_list.empty() ? baseDelay : delay - kFxHitGap;
        FxEvent s;
        s.kind = FxKind::Stun;
        s.on_enemy = true;
        s.slot = slot;
        s.delay = lastHit + 4;
        fx_.push_back(s);
        fxSpan_ = std::max(fxSpan_, s.delay);
    }
}

std::vector<FxEvent> Battle::takeFx() {
    std::vector<FxEvent> out;
    out.swap(fx_);
    return out;
}

void Battle::startRound() {
    ++round_;

    if (round_ > 1) {
        // Regen turn biasa dibatasi kMaxSP, tapi tidak pernah MENURUNKAN SP yang sudah
        // di atas kMaxSP (sisa SP instan dari Overdrive, lihat kMaxSPOverdrive).
        for (auto& c : party_.front) c.sp = gainSpCapped(c.sp, kSpRegenFront, kMaxSP);
        for (auto& c : party_.back)  c.sp = gainSpCapped(c.sp, kSpRegenBack, kMaxSP);
    }

    for (auto& c : cmds_) c = Command();
    queue_.clear();
    qpos_ = 0;
    activeSlot_ = -1;
    fxSpan_ = 0;
    phase_ = BattlePhase::Planning;
}

// Giliran ekstra OD: kesempatan aksi tambahan sebelum giliran musuh, BUKAN ronde baru.
// Sengaja tidak menaikkan round_, meregen SP, atau memanggil tickEffects, supaya buff/debuff
// berdurasi tidak berkurang selama party masih dalam jendela overdrive.
void Battle::startExtraRound() {
    for (auto& c : cmds_) c = Command();
    queue_.clear();
    qpos_ = 0;
    activeSlot_ = -1;
    fxSpan_ = 0;
    phase_ = BattlePhase::Planning;
}

bool Battle::anyAllyDown() const {
    for (const auto& c : party_.front)
        if (!c.alive()) return true;
    return false;
}


bool Battle::canUseSkill(int slot, int idx) const {
    if (phase_ != BattlePhase::Planning) return false;
    if (slot < 0 || slot > 2 || idx < 0 || idx > 1) return false;
    const Combatant& c = party_.front[slot];
    return c.skills[idx].valid() && c.sp >= c.skills[idx].sp_cost && c.uses_left[idx] != 0;
}

bool Battle::setCommand(int slot, CommandType type, int idx) {
    if (phase_ != BattlePhase::Planning || slot < 0 || slot > 2) return false;
    if (type == CommandType::Skill && !canUseSkill(slot, idx)) return false;
    cmds_[slot].type  = type;
    cmds_[slot].skill = (type == CommandType::Skill) ? idx : 0;
    return true;
}

void Battle::swapSlot(int slot, int backIdx) {
    if (phase_ != BattlePhase::Planning) return;
    if (slot < 0 || slot > 2 || backIdx < 0 || backIdx > 2) return;

    const std::string out = party_.front[slot].name;
    std::swap(party_.front[slot], party_.back[backIdx]);
    cmds_[slot] = Command();
    message_ = party_.front[slot].name + " steps in to replace " + out + ".";
}

void Battle::swapFront(int a, int b) {
    if (phase_ != BattlePhase::Planning || a == b) return;
    if (a < 0 || a > 2 || b < 0 || b > 2) return;
    std::swap(party_.front[a], party_.front[b]);
    std::swap(cmds_[a], cmds_[b]);                   // aksi ikut pindah bersama karakternya
    message_ = party_.front[a].name + " and " + party_.front[b].name + " swap places.";
}

void Battle::swapBack(int a, int b) {
    if (phase_ != BattlePhase::Planning || a == b) return;
    if (a < 0 || a > 2 || b < 0 || b > 2) return;
    std::swap(party_.back[a], party_.back[b]);
}

std::string Battle::commandLabel(int slot) const {
    const Command& cmd = cmds_[slot];
    if (cmd.type == CommandType::Attack) return "Attack";
    return party_.front[slot].skills[cmd.skill].name;
}

void Battle::execute() {
    if (phase_ != BattlePhase::Planning) return;

    queue_.clear();
    qpos_ = 0;
    // Grup 1: skill dukungan (heal/buff/debuff), kiri ke kanan.
    for (int i = 0; i < 3; ++i) {
        const Command& cmd = cmds_[i];
        if (cmd.type == CommandType::Skill &&
            party_.front[i].skills[cmd.skill].kind != SkillKind::Attack)
            queue_.push_back(i);
    }
    // Grup 2: normal attack dan skill serangan, kiri ke kanan.
    for (int i = 0; i < 3; ++i) {
        const Command& cmd = cmds_[i];
        if (cmd.type == CommandType::Attack ||
            party_.front[i].skills[cmd.skill].kind == SkillKind::Attack)
            queue_.push_back(i);
    }

    phase_ = BattlePhase::Executing;
    timer_ = kStepDelay / 2;
    message_.clear();
}


namespace {
// Kalimat tambahan hasil serangan: KO, BROKEN, STUN. Angka damage sudah tampil sebagai
// fx melayang di layar, jadi pesan teks ini murni naratif, tanpa angka.
std::string resultTail(const AttackSummary& a) {
    std::string tail;
    if (a.broke)   tail += " Defense broken!";
    if (a.stunned) tail += " Stunned!";
    if (a.killed)  tail += " Defeated!";
    return tail;
}

}  // namespace

void Battle::report(const std::string& who, const std::string& what, bool isBasic,
                    const AttackSummary& a) {
    message_ = who + (isBasic ? " attacks " : " uses " + what + " on ") + enemy_.name + "." +
               resultTail(a);
}

void Battle::applyFx(const SkillEffect& fx, Combatant& caster) {
    switch (fx.scope) {
    case EffectScope::Self:
        addEffect(caster, fx.type, fx.value, fx.turns);
        break;
    case EffectScope::AllFront:
        for (auto& c : party_.front)
            if (c.alive()) addEffect(c, fx.type, fx.value, fx.turns);
        break;
    case EffectScope::AllParty:
        for (auto& c : party_.front)
            if (c.alive()) addEffect(c, fx.type, fx.value, fx.turns);
        for (auto& c : party_.back)
            if (c.alive()) addEffect(c, fx.type, fx.value, fx.turns);
        break;
    case EffectScope::Enemy:
        addEffect(enemy_, fx.type, fx.value, fx.turns);
        break;
    }
}

void Battle::runPlayerAction(int slot) {
    Combatant& c = party_.front[slot];
    const Command& cmd = cmds_[slot];
    activeSlot_ = slot;

    Skill s = normalAttackOf(c);
    int usedIdx = -1;
    if (cmd.type == CommandType::Skill) {
        const Skill& cand = c.skills[cmd.skill];
        if (cand.valid() && c.sp >= cand.sp_cost && c.uses_left[cmd.skill] != 0) {
            s = cand;                                    // SP/pemakaian kurang: jatuh ke Attack
            usedIdx = cmd.skill;
        }
    }
    c.sp -= s.sp_cost;
    if (usedIdx >= 0 && c.uses_left[usedIdx] > 0) --c.uses_left[usedIdx];

    if (s.kind == SkillKind::HealDP) {
        int before[3];
        for (int i = 0; i < 3; ++i) before[i] = party_.front[i].dp;
        healPartyDP(party_, s.power);
        for (int i = 0; i < 3; ++i) {
            const int gained = party_.front[i].dp - before[i];
            if (gained <= 0) continue;
            FxEvent e;
            e.kind = FxKind::Heal;
            e.slot = i;
            e.dp = gained;
            e.delay = i * 4;
            fx_.push_back(e);
            fxSpan_ = std::max(fxSpan_, e.delay);
        }
        message_ = c.name + " uses " + s.name + ", repairing the team's DP.";
    } else if (s.kind == SkillKind::Support) {
        applyFx(s.fx, c);
        message_ = c.name + " uses " + s.name + ".";
    } else {
        Skill atk = s;
        if (odActive_) atk.power = atk.power * odMultPct_ / 100;
        const AttackSummary a = performAttack(atk, c, enemy_);
        recordHits(a, true, 0, 0, slot);
        gainOdGauge(a.hits);                          // gauge terus terisi, aktif atau tidak
        if (usedIdx >= 0) consumeOneTimeBuffs(c);     // AtkUp/DevUp hanya habis oleh skill, bukan attack biasa
        report(c.name, s.name, s.is_basic, a);
        if (s.fx.type != EffectType::None && enemy_.alive()) applyFx(s.fx, c);
        if (s.sp_gain > 0 && std::rand() % 100 < s.sp_gain_chance) {
            c.sp = gainSpCapped(c.sp, s.sp_gain, kMaxSP);   // gimmick turn biasa: cap tetap kMaxSP
            message_ += " " + c.name + "'s SP is restored!";
        }
    }
}

void Battle::updateExecution() {
    if (--timer_ > 0) return;

    if (qpos_ >= queue_.size()) {
        if (odActive_ && odExtraTurns_ > 0) {         // masih ada giliran ekstra OD: lompat balik
            --odExtraTurns_;                          // ke Planning, giliran musuh belum datang
            startExtraRound();
            return;
        }
        odActive_ = false;
        odTotalTurns_ = 0;
        phase_ = BattlePhase::EnemyTurn;
        timer_ = kEnemyDelay;
        activeSlot_ = -1;
        return;
    }
    runPlayerAction(queue_[qpos_++]);
    if (!enemy_.alive()) {
        activeSlot_ = -1;
        if (!awakened_) {
            phase_ = BattlePhase::Awakening;
            // Tunggu ekor visual hit terakhir (fxSpan_) + jeda 60 frame, baru cutscene awakening
            // penuh (awakening::Duration, lihat awakening.h) main sebelum ronde berikutnya mulai.
            timer_ = fxSpan_ + 60 + awakening::Duration;
            message_ += " A new energy begins to stir...";
        } else {
            phase_ = BattlePhase::Dying;
            timer_ = fxSpan_ + 60 + defeat_cutscene::Duration;
            // Efek tertunda dari serangan terakhir dipertahankan; input battle diblokir sampai pohon terungkap.
            odActive_ = false;
            odExtraTurns_ = 0;
            odTotalTurns_ = 0;
            enemyActPending_ = false;
        }
        return;
    }
    timer_ = kStepDelay + fxSpan_;                   // tunggu angka hit beruntun selesai muncul
    fxSpan_ = 0;
}


// Serangan bos ditampilkan sebagai deskripsi, bukan nama skill.
void Battle::enemyFocus() {
    const int targetSlot = std::rand() % 3;
    Combatant& target = party_.front[targetSlot];
    const int power = awakened_ ? kEnemyFocusPower : kEnemyFocusPowerPhase1;
    const Skill s("focus attack", SkillKind::Attack, 0, 1, power);
    const AttackSummary a = performAttack(s, enemy_, target);
    recordHits(a, false, targetSlot, 0);

    message_ = enemy_.name + " attacks " + target.name + "." + resultTail(a);
}

void Battle::enemyAoe() {
    const int power = awakened_ ? kEnemyAoePower : kEnemyAoePowerPhase1;
    const Skill s("mass attack", SkillKind::Attack, 0, 1, power);
    int breaks = 0, kills = 0;
    for (int i = 0; i < 3; ++i) {
        Combatant& c = party_.front[i];
        const AttackSummary a = performAttack(s, enemy_, c);
        recordHits(a, false, i, i * 4);
        if (a.broke)  ++breaks;
        if (a.killed) ++kills;
    }
    message_ = enemy_.name + " attacks the entire party.";
    if (breaks) message_ += " Some defenses shatter!";
    if (kills)  message_ += kills > 1 ? " Several allies fall!" : " An ally falls!";
}

// Musuh melewatkan giliran ini karena masih ada stun yang belum di-skip (skips > 0 sebelum
// masuk sini). Statusnya sengaja tetap menyala, supaya terlihat di giliran party berikutnya;
// baru hilang di awal giliran musuh sesudah skip terakhir (lihat clearStunVisual).
void Battle::skipEnemyTurnForStun() {
    --enemy_.stun_skips;
    message_ = enemy_.name + " is stunned and can't move!";
}

// Skip terakhir baru saja habis: lepas visual stun (ikon dan latar) dulu, lalu tunda serangan
// musuh selama kStunEndDelay frame supaya perubahannya sempat terlihat sebelum musuh bertindak.
void Battle::clearStunVisualThenDelayAttack() {
    enemy_.stunned = false;
    enemyActPending_ = true;
    timer_ = kStunEndDelay;
}

void Battle::enemyAct() {
    if (round_ % 3 == 0) enemyAoe();
    else                 enemyFocus();
}

void Battle::updateEnemyTurn() {
    if (--timer_ > 0) return;

    if (enemyActPending_) {
        // Jeda pasca-stun (clearStunVisualThenDelayAttack) sudah lewat: musuh menyerang sekarang.
        enemyActPending_ = false;
        enemyAct();
    } else {
        // Awal giliran musuh: durasi buff/debuff berkurang. Yang baru dipasang di ronde ini belum
        // dihitung, dan yang habis hilang sebelum musuh bertindak.
        tickEffects(enemy_);
        for (auto& c : party_.front) tickEffects(c);
        for (auto& c : party_.back)  tickEffects(c);

        if (enemy_.stun_skips > 0) {
            skipEnemyTurnForStun();
        } else if (enemy_.stunned) {
            clearStunVisualThenDelayAttack();
            return;                                   // belum bertindak, jangan lanjut ke startRound()
        } else {
            enemyAct();
        }
    }

    if (anyAllyDown()) {
        phase_ = BattlePhase::Defeat;
        message_ += " Your party has fallen...";
        return;
    }
    startRound();
}

void Battle::updateAwakening() {
    if (--timer_ > 0) return;
    awakened_ = true;
    enemyActPending_ = false;
    // Bos baru hanya mereset break, stun, devastation, dan debuff musuh.
    // HP, DP, SP, formasi, efek, dan sisa pemakaian skill party terbawa ke battle 2.
    // TUNING: fase Awaken lebih tangguh lagi (DP 110->160, HP 950->1300).
    enemy_ = Combatant("Awaken Hellspider", "Boss", 160, 1300, 0, Skill(), Skill());
    enemy_.is_enemy = true;
    enemy_.sp = 0;
    enemy_.max_devastation = kMaxDevastationAwaken;   // TUNING: devastation rate fase Awaken bisa naik sampai 999%
    fx_.clear();
    startRound(); // perintah yang belum dijalankan dibuang; regen SP ronde berikutnya normal
    message_ = "Awaken Hellspider rises!";
}

void Battle::update() {
    switch (phase_) {
    case BattlePhase::Executing: updateExecution(); break;
    case BattlePhase::EnemyTurn: updateEnemyTurn(); break;
    case BattlePhase::Awakening: updateAwakening(); break;
    case BattlePhase::Dying:
        if (--timer_ <= 0) {
            phase_ = BattlePhase::Victory;
            queue_.clear();
            fx_.clear();
            message_ = "You win!";
        }
        break;
    default: break;
    }
}