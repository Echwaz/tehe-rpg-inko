#pragma once
#include "defeat_cutscene.h"
#include "awakening.h"
// Logika pertempuran turn-based (tanpa dependensi 3DS).
//
// Aturan yang diadopsi:
//  - Party 6 karakter: 3 front (bertarung) dan 3 back (tidak menyerang/diserang).
//  - Tiap giliran: rencanakan aksi 3 slot front, lalu semuanya dieksekusi
//    (kiri ke kanan, skill dukungan lebih dulu, baru serangan), lalu musuh.
//  - Swap front/back gratis dan tanpa batas selama fase perencanaan.
//  - SP per karakter, beregenerasi tiap ronde (back row lebih cepat).
//  - DP menyerap damage. Satu hit yang menghabiskan DP: sisa damage hilang.
//    Hit berikutnya (skill multi-hit) langsung mengenai HP.
//  - Giliran party dan musuh paralel: ronde N = giliran party N (perencanaan + eksekusi), lalu
//    giliran musuh N. Stun/debuff dengan durasi 1 yang dipasang saat eksekusi ronde N: aktif
//    seketika, musuh melewatkan giliran musuh N, statusnya TETAP tampil di giliran party N+1,
//    dan hilang di awal giliran musuh N+1. Visual stun hilang DULU, jeda sebentar
//    (kStunEndDelay), baru musuh menyerang.
//  - Stun menumpuk: tiap stun (break, skill Megumi, dst) menambah 1 giliran musuh yang dilewati.
//    Status tampil sampai awal giliran musuh sesudah giliran terakhir yang dilewati.
//  - Musuh yang break: stun 1 giliran (aturan di atas). DP musuh TIDAK pulih sendiri; pemulihan hanya lewat
//    gimmick khusus (belum ada), yang bisa memakai recoverFromBreak().
//  - Devastation rate musuh HANYA naik oleh hit yang mengenai musuh yang sudah break.
//    Blaster menaikkannya 4x-5x lebih cepat. Rate kembali ke 100% hanya jika musuh pulih dari break.
//  - Sekutu yang break tidak memulihkan DP kecuali di-heal skill.
//  - HP satu sekutu 0 = game over.
//  - Buff/debuff: AtkUp dan DevUp sekali pakai. Hanya SKILL serangan yang memakai dan
//    menghabiskannya; attack biasa tidak memakai dan tidak menghabiskan. DefUp, DefDown, AtkDown
//    bertahan sesuai durasi (lihat tickEffects).
//    Efek sejenis menumpuk aditif (maks 2 tumpukan), efek berbeda saling dikali.
//
// Angka bertanda TUNING adalah nilai keseimbangan sementara; ubah sesuai kebutuhan.
#include <array>
#include <string>
#include <vector>

constexpr int kMaxSP              = 20;   // batas SP dari regen turn biasa
constexpr int kMaxSPOverdrive      = 40;   // TUNING: batas SP setelah SP instan Overdrive (boleh melebihi kMaxSP)
constexpr int kStartSP            = 10;   // TUNING: cukup untuk skill biasa di ronde 1
constexpr int kSpRegenFront       = 2;    // TUNING
constexpr int kSpRegenBack        = 4;    // TUNING
constexpr int kDevastationPerHit  = 4;    // TUNING: devastation rate per hit (hanya saat musuh break), karakter biasa
constexpr int kBlasterDevMult     = 4;    // Blaster: 4x lebih cepat (serangan biasa dan skill biasa)
constexpr int kBlasterSignatureDevMult = 5;   // Blaster: 5x lebih cepat (skill andalan)
constexpr int kMaxDevastation     = 300;
constexpr int kMaxEffectStacks    = 2;    // tumpukan maksimum per jenis efek
constexpr int kMaxDefUpPct        = 70;   // batas pengurangan damage dari DefUp
constexpr int kMaxAtkDownPct      = 60;   // batas pengurangan damage dari AtkDown
constexpr int kEnemyFocusPower    = 30;   // TUNING: damage serangan fokus musuh (1 hit total), fase Awaken
constexpr int kEnemyAoePower      = 15;   // TUNING: damage serangan massal musuh (per anggota), fase Awaken
constexpr int kEnemyFocusPowerPhase1 = 18;   // TUNING: fase 1 (Hellspider) dibuat lebih santai
constexpr int kEnemyAoePowerPhase1   = 8;    // TUNING: fase 1 (Hellspider) dibuat lebih santai
constexpr int kMaxDevastationAwaken  = 999;  // TUNING: cap devastation rate lebih tinggi khusus fase Awaken
constexpr int kStepDelay          = 50;   // frame antar aksi pemain
constexpr int kEnemyDelay         = 60;   // frame sebelum musuh bergerak
constexpr int kStunEndDelay       = 30;   // jeda antara visual stun hilang dan musuh menyerang

// Gauge terisi dari tiap hit yang mengenai musuh (dp atau hp, tidak masalah). Setiap
// kOdHitsPerBar hit = 1 bar. Diaktifkan manual saat Planning selama masih ada bar dan
// trigger tersisa: party dapat SP instan (boleh melebihi kMaxSP, dibatasi kMaxSPOverdrive;
// regen SP dari turn biasa tetap dibatasi kMaxSP dan tidak pernah menurunkan SP yang sudah
// di atas itu), damage serangan dikalikan kOdMultPct[bar-1] sampai giliran ekstra
// (total giliran per stage: kOdTotalTurns, sudah termasuk giliran aktivasi) habis. Giliran ekstra TIDAK memicu
// giliran musuh dan TIDAK meregen SP/menjalankan tickEffects (lihat startExtraRound()).
constexpr int kOdHitsPerBar       = 15;   // TUNING
constexpr int kOdMaxBars          = 3;    // jumlah stage OD
constexpr std::array<int, kOdMaxBars> kOdMultPct = {{ 110, 120, 130 }};  // TUNING
constexpr std::array<int, kOdMaxBars> kOdTotalTurns = {{ 1, 2, 3 }};  // TUNING: total giliran OD per stage (termasuk giliran aktivasi)
constexpr std::array<int, kOdMaxBars> kOdSpGrant = {{ 6, 12, 20 }};  // TUNING

enum class SkillKind { Attack, HealDP, Support };

enum class EffectType  { None, AtkUp, DefUp, DevUp, DefDown, AtkDown };
enum class EffectScope { Self, AllFront, AllParty, Enemy };

struct SkillEffect {
    EffectType  type  = EffectType::None;
    int         value = 0;    // persen (DevUp: poin devastation rate per hit)
    int         turns = 0;    // durasi (lihat tickEffects); 0 = sekali pakai
    EffectScope scope = EffectScope::Self;
};

struct StatusEffect {
    EffectType type;
    int        value;
    int        turns;         // 0 = sekali pakai, habis saat pemiliknya memakai skill serangan
    bool       fresh;         // baru dipasang di ronde ini: belum dihitung oleh tickEffects
};

struct Skill {
    const char* name   = "";
    SkillKind   kind   = SkillKind::Attack;
    int sp_cost = 0;
    int hits    = 1;      // jumlah hit (Attack)
    int power   = 0;      // damage per hit (Attack) atau DP dipulihkan ke tiap front (HealDP)
    int dp_pct  = 100;    // pengali damage saat mengenai DP (Breaker tinggi)
    int hp_pct  = 100;    // pengali damage saat mengenai HP (Attacker tinggi)
    SkillEffect fx;       // buff/debuff tambahan (opsional untuk Attack, wajib untuk Support)
    int dev_mult = 0;     // pengali devastation rate per hit; 0 = ikut pengali karakter
    bool is_basic = false;   // attack biasa: tidak memakai dan tidak menghabiskan ATK+/DEV+
    bool ex = false;      // EX Skill: mahal, tanpa batas pemakaian per battle
    int max_uses = 0;     // 0 = tanpa batas
    int stun_chance = 0;  // % peluang membuat musuh stun (lewat giliran berikutnya)
    int sp_gain = 0;      // SP yang dipulihkan ke pemakai jika berhasil
    int sp_gain_chance = 0;   // % peluang memulihkan SP
    const char* description = "";

    Skill() = default;
    Skill(const char* n, SkillKind k, int cost, int h, int pw, int dpp = 100, int hpp = 100)
        : name(n), kind(k), sp_cost(cost), hits(h), power(pw), dp_pct(dpp), hp_pct(hpp) {}

    Skill& withEffect(EffectType t, int value, int turns, EffectScope scope) {
        fx.type = t; fx.value = value; fx.turns = turns; fx.scope = scope;
        return *this;
    }
    Skill& withDescription(const char* d) { description = d; return *this; }
    Skill& withDevMult(int m) { dev_mult = m; return *this; }
    Skill& asEx() { ex = true; return *this; }
    Skill& withUses(int uses) { max_uses = uses; return *this; }
    Skill& withStun(int chance) { stun_chance = chance; return *this; }
    Skill& withSpGain(int amount, int chance) { sp_gain = amount; sp_gain_chance = chance; return *this; }
    bool valid() const { return name && name[0]; }
};

inline Skill makeSupport(const char* name, int cost, EffectType t, int value, int turns,
                         EffectScope scope) {
    Skill s(name, SkillKind::Support, cost, 0, 0);
    s.withEffect(t, value, turns, scope);
    return s;
}

struct Combatant {
    std::string name;
    const char* role = "";
    int  max_dp = 0, dp = 0;
    int  max_hp = 0, hp = 0;
    int  atk = 0;
    int  sp  = 0;
    std::array<Skill, 2> skills;
    int  dev_mult = 1;           // pengali kenaikan devastation rate musuh per hit (Blaster > 1)
    std::array<int, 2> uses_left = {{ -1, -1 }};   // sisa pemakaian tiap skill; -1 = tanpa batas

    bool is_enemy     = false;
    bool broken       = false;
    bool stunned      = false;   // musuh: status stun yang TAMPIL (ikon dan latar)
    int  stun_skips   = 0;       // sisa giliran musuh yang dilewati; tiap stun menambah 1 (bisa menumpuk)
    int  devastation  = 100;     // persen, hanya dipakai musuh
    int  max_devastation = kMaxDevastation;   // batas atas devastation rate; beda per fase (lihat updateAwakening)
    std::vector<StatusEffect> effects;

    Combatant() = default;
    Combatant(const char* n, const char* r, int dp_, int hp_, int atk_,
              const Skill& s0, const Skill& s1)
        : name(n), role(r), max_dp(dp_), dp(dp_), max_hp(hp_), hp(hp_), atk(atk_), sp(kStartSP) {
        skills[0] = s0;
        skills[1] = s1;
        for (int i = 0; i < 2; ++i)
            uses_left[i] = skills[i].max_uses > 0 ? skills[i].max_uses : -1;
    }

    Combatant& withDevMult(int m) { dev_mult = m; return *this; }
    bool alive() const { return hp > 0; }
};

struct Party {
    std::array<Combatant, 3> front;
    std::array<Combatant, 3> back;
};


struct HitResult {
    int  dp_lost = 0;
    int  hp_lost = 0;
    bool just_broken = false;
    bool killed = false;
};

// Satu hit. dp_dmg dipakai jika target masih punya DP, hp_dmg jika DP kosong.
// Sisa damage saat DP habis dibuang.
HitResult applyHit(Combatant& target, int dp_dmg, int hp_dmg);

struct AttackSummary {
    int  hits = 0;
    int  dp_lost = 0;
    int  hp_lost = 0;
    bool broke = false;
    bool killed = false;
    bool stunned = false;     // stun dari skill (bukan dari break)
    std::vector<HitResult> hit_list;   // rincian tiap hit, urut (untuk angka damage per hit)
};


enum class FxKind { Damage, Heal, Break, Stun };   // Stun: hanya musuh, tanpa angka (mengatur latar layar atas)

// Satu kejadian yang perlu digambar layar (angka melayang, efek break). Logika battle sudah
// selesai dihitung saat event dibuat; 'delay' hanya mengatur kapan efeknya tampil.
struct FxEvent {
    FxKind kind     = FxKind::Damage;
    bool   on_enemy = false;   // true = mengenai musuh; false = sekutu di slot front 'slot'
    int    slot     = 0;
    int    dp       = 0;       // Damage: DP yang hilang. Heal: DP yang pulih.
    int    hp       = 0;       // Damage: HP yang hilang.
    int    delay    = 0;       // frame sejak aksi dijalankan sampai efek ini tampil
    int    attacker_slot = -1; // slot party (0..2) penyerang; hanya diisi saat on_enemy true,
                                // dipakai tampilan untuk membedakan gaya hit senjata jarak dekat/jauh
};
constexpr int kFxHitGap = 9;   // jeda antar hit beruntun dalam satu skill (frame)

// Menjalankan seluruh hit sebuah skill serangan ke satu target, termasuk
// devastation rate dan stun jika target adalah musuh.
// Damage memperhitungkan buff/debuff: (1+AtkUp) x (1-AtkDown) x (1+DefDown) x (1-DefUp).
AttackSummary performAttack(const Skill& skill, const Combatant& attacker, Combatant& target);

Skill normalAttackOf(const Combatant& c);
void  healPartyDP(Party& p, int amount);
// Memulihkan DP penuh dan mengakhiri break (devastation rate kembali 100%). Tidak dipanggil
// oleh alur battle biasa: dipakai bila nanti ada gimmick musuh yang memulihkan DP.
void  recoverFromBreak(Combatant& c);


void addEffect(Combatant& c, EffectType type, int value, int turns);  // maks 2 tumpukan/jenis
int  effectTotal(const Combatant& c, EffectType type);                 // jumlah nilai aditif
// Awal giliran musuh: kurangi durasi efek (yang baru dipasang di ronde ini dilewati sekali), yang
// mencapai 0 hilang sebelum musuh bertindak. Durasi 1 = bertahan sampai awal giliran musuh
// ronde berikutnya. Efek pada MUSUH (DefDown, AtkDown, dst) sengaja tidak pernah berkurang
// durasinya lewat fungsi ini, supaya gampang ditumpuk terus; hanya direset saat Awaken
// (lihat updateAwakening()).
void tickEffects(Combatant& c);
void consumeOneTimeBuffs(Combatant& c);      // habiskan AtkUp/DevUp setelah skill serangan
constexpr unsigned effectBit(EffectType t) { return 1u << static_cast<int>(t); }
// skipMask: jenis efek (effectBit) yang tidak dituliskan, mis. karena sudah tampil sebagai icon.
std::string effectSummary(const Combatant& c, unsigned skipMask = 0);   // mis. "ATK+x2 DEF+(2t)"
std::string effectLabel(const SkillEffect& fx);     // mis. "DEF+30%(2t) tim"


enum class BattlePhase { Planning, Executing, EnemyTurn, Awakening, Dying, Victory, Defeat };
enum class CommandType { Attack, Skill };

struct Command {
    CommandType type  = CommandType::Attack;
    int         skill = 0;    // index 0..1 jika type == Skill
};

class Battle {
public:
    void start();
    void update();                       // panggil tiap frame

    bool canUseSkill(int slot, int skillIdx) const;
    bool setCommand(int slot, CommandType type, int skillIdx = 0);
    void swapSlot(int slot, int backIdx);   // gratis, tanpa batas
    void swapFront(int a, int b);           // tukar urutan dua slot front (urutan eksekusi)
    void swapBack(int a, int b);            // tukar urutan dua anggota back
    void execute();

    int  odGauge()        const { return odGauge_; }         // total hit terkumpul (lihat kOdHitsPerBar)
    int  odBarsReady()    const { return odBarsReady_; }      // 0..kOdMaxBars, siap dipakai
    bool odActive()        const { return odActive_; }
    int  odTurnsLeft()     const { return odExtraTurns_; }   // sisa giliran ekstra OD yang belum terpakai
    // Counter tampilan "sisa/total": total = giliran saat OD diaktifkan + giliran ekstra
    // kOdTotalTurns[stage - 1], sisa = giliran yang masih berada di bawah OD, termasuk
    // giliran yang sedang berjalan. Keduanya 0 selama OD tidak aktif.
    int  odTurnsRemaining() const { return odActive_ ? odExtraTurns_ + 1 : 0; }
    int  odTurnsTotal()     const { return odActive_ ? odTotalTurns_ : 0; }
    int  odLevel()          const { return odActive_ ? odLevel_ : 0; }   // 1..kOdMaxBars
    // Aktifkan OD memakai seluruh bar yang sudah terkumpul. false kalau belum Planning, belum
    // ada bar penuh, atau OD sedang aktif (tidak bisa OD berantai). Gauge/bar TIDAK berubah
    // saat gagal.
    bool activateOverdrive();

    const Command&     command(int slot) const { return cmds_[slot]; }
    std::string        commandLabel(int slot) const;

    BattlePhase        phase()      const { return phase_; }
    const Party&       party()      const { return party_; }
    const Combatant&   enemy()      const { return enemy_; }
    bool               awakened()   const { return awakened_; }
    // Frame index into awakening::sample() saat phase_ == Awakening; negatif selagi hit terakhir
    // masih menampilkan visual tail-nya (cutscene belum mulai), habis begitu phase_ pindah.
    int awakeningFrame() const {
        return phase_ == BattlePhase::Awakening ? awakening::Duration - timer_ : -1;
    }
    int defeatFrame() const {
        if (phase_ == BattlePhase::Victory) return defeat_cutscene::Duration;
        return phase_ == BattlePhase::Dying ? defeat_cutscene::Duration - timer_ : -1;
    }
    int                round()      const { return round_; }
    int                activeSlot() const { return activeSlot_; }
    const std::vector<int>& queue() const { return queue_; }
    const std::string& message()    const { return message_; }

    // Kejadian visual sejak pemanggilan terakhir (angka damage, heal, break). Mengosongkan daftarnya.
    // Murni informasi untuk tampilan; tidak memengaruhi logika battle.
    std::vector<FxEvent> takeFx();

private:
    void startRound();
    void updateAwakening();
    void startExtraRound();      // giliran ekstra OD: seperti startRound() tapi tanpa ++round_,
                                  // regen SP, atau tickEffects
    void gainOdGauge(int hits);  // tambah gauge OD, hitung ulang bar (berhenti saat penuh)
    void runPlayerAction(int slot);
    void updateExecution();
    void updateEnemyTurn();
    void enemyFocus();
    void enemyAoe();
    void enemyAct();                        // pilih & jalankan serangan musuh (focus/AoE) ronde ini
    void skipEnemyTurnForStun();            // giliran musuh dilewati, stun_skips masih tersisa
    void clearStunVisualThenDelayAttack();  // skip terakhir habis: lepas visual, tunda serangan
    bool anyAllyDown() const;
    void applyFx(const SkillEffect& fx, Combatant& caster);
    void report(const std::string& who, const std::string& what, bool isBasic,
                const AttackSummary& a);
    void recordHits(const AttackSummary& a, bool onEnemy, int slot, int baseDelay,
                     int attackerSlot = -1);

    Party       party_;
    Combatant   enemy_;
    BattlePhase phase_ = BattlePhase::Planning;
    bool        awakened_ = false;
    int         round_ = 0;
    int         timer_ = 0;
    int         activeSlot_ = -1;
    std::array<Command, 3> cmds_;
    std::vector<int>       queue_;
    std::size_t            qpos_ = 0;
    std::string            message_;
    std::vector<FxEvent>   fx_;
    bool                   enemyActPending_ = false;   // visual stun sudah hilang, musuh menyerang setelah jeda
    int                    fxSpan_ = 0;   // panjang animasi (frame) dari aksi terakhir; aksi berikutnya menunggu

    int  odGauge_        = 0;
    int  odBarsReady_    = 0;
    bool odActive_        = false;
    int  odExtraTurns_   = 0;
    int  odTotalTurns_   = 0;     // total giliran di bawah OD sejak diaktifkan (untuk counter "sisa/total")
    int  odLevel_        = 0;     // stage OD yang sedang berjalan (1..kOdMaxBars)
    int  odMultPct_       = 100;
};
