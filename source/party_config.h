#pragma once
// Konfigurasi party kustom: nama karakter, serta nilai dan efek dua skill tiap karakter.
// Murni C++ (tanpa dependensi 3DS) supaya bisa dites di PC, sama seperti combat.h.
//
// Alur: defaults() -> (pemain mengedit lewat layar Customize, atau lewat berkas party.cfg)
//       -> sanitize() -> buildParty() -> Battle::setPartyOverride().
//
// Yang boleh diubah pemain: nama karakter, class (role), dan per skill: nama, biaya SP, jumlah
// hit, power, bonus (HP Eff / DP Eff / Devastation), serta efek buff/debuff. Class hanyalah label
// tanpa efek; bonus dipasang per skill serangan, jadi pemain menentukan sendiri skill mana yang
// dapat bonus. HP/DP/ATK, jenis skill (Attack/Heal/Support), dan mekanik lain tetap bawaan. Semua nilai dijepit ke batas di bawah supaya battle tidak
// rusak; ubah konstantanya kalau ingin batas yang lebih longgar atau ketat.
#include <array>
#include <string>

#include "combat.h"

namespace party_config {

constexpr int kRosterSize = 6;   // indeks 0..2 = front bawaan, 3..5 = back bawaan

// Batas panjang teks dalam KARAKTER (bukan byte), supaya nama Jepang tidak terpotong di tengah.
constexpr int kNameMaxChars      = 8;    // HUD party hanya punya ruang ~60 px untuk nama
constexpr int kSkillNameMaxChars = 18;

constexpr int kSpCostMax   = kMaxSP;   // skill yang lebih mahal dari batas SP tidak akan pernah bisa dipakai
constexpr int kHitsMin     = 1;
constexpr int kHitsMax     = 10;
constexpr int kAttackPowerMax = 20;
constexpr int kHealPowerMax   = 40;
constexpr int kFxTurnsMax  = 3;

struct SkillCfg {
    SkillKind   kind = SkillKind::Attack;   // hanya baca: selalu diambil dari roster bawaan
    bool        ex   = false;               // hanya baca: skill EX (bonus class lebih besar)
    std::string name;
    int         sp_cost = 0;
    int         hits    = 1;                // hanya dipakai skill Attack
    int         power   = 1;                // Attack: damage per hit. HealDP: DP yang dipulihkan.
    SkillBonus  bonus   = SkillBonus::None; // hanya skill Attack; angkanya dari konstanta di combat.h
    EffectType  fx_type  = EffectType::None;   // hanya Attack dan Support
    int         fx_value = 0;
    int         fx_turns = 0;
    EffectScope fx_scope = EffectScope::Self;

    bool operator==(const SkillCfg& o) const {
        return kind == o.kind && name == o.name && sp_cost == o.sp_cost && hits == o.hits &&
               power == o.power && bonus == o.bonus && fx_type == o.fx_type && fx_value == o.fx_value &&
               fx_turns == o.fx_turns && fx_scope == o.fx_scope;
    }
    bool operator!=(const SkillCfg& o) const { return !(*this == o); }
};

struct CharCfg {
    std::string key;    // pengenal tetap (mis. "Ruka"), hanya baca
    std::string role;   // class: salah satu roleName() di combat.h. Hanya label, tanpa efek.
    std::string name;   // nama tampilan yang bisa diganti
    std::array<SkillCfg, 2> skills;

    bool operator==(const CharCfg& o) const {
        return key == o.key && role == o.role && name == o.name && skills[0] == o.skills[0] &&
               skills[1] == o.skills[1];
    }
    bool operator!=(const CharCfg& o) const { return !(*this == o); }
};

struct PartyConfig {
    std::array<CharCfg, kRosterSize> chars;
};

// Konfigurasi yang sama persis dengan roster bawaan.
PartyConfig defaults();

// Menjepit semua nilai ke batas yang sah dan mengembalikan nilai bawaan untuk yang rusak
// (nama kosong, efek tidak masuk akal, class tidak dikenal, bonus di skill non-serangan, dst). Bagian hanya-baca (key, kind,
// ex) selalu ditimpa dari roster bawaan. Idempoten.
void sanitize(PartyConfig& cfg);

// Membangun Party siap-battle dari konfigurasi (sudah disanitasi di dalamnya).
// Memakai roster bawaan sebagai dasar (HP/DP/ATK tidak berubah). Class hanya mengganti label;
// bonus HP/DP/devastation datang dari SkillCfg::bonus tiap skill.
Party buildParty(const PartyConfig& cfg);

// Berkas teks sederhana bergaya INI. load(): kalau berkas tidak ada atau tidak terbaca,
// cfg diisi defaults() dan hasilnya false. Baris yang tidak dikenal atau bernilai rusak
// diabaikan, jadi berkas hasil edit tangan tidak pernah membuat game crash. Hasil load
// selalu sudah disanitasi. save(): membuat folder induk (satu tingkat) bila belum ada.
bool load(const std::string& path, PartyConfig& cfg);
bool save(const std::string& path, const PartyConfig& cfg);

// Reset ke bawaan.
void resetAll(PartyConfig& cfg);
void resetChar(PartyConfig& cfg, int charIdx);
void resetSkill(PartyConfig& cfg, int charIdx, int skillIdx);
bool isModified(const PartyConfig& cfg, int charIdx);                 // ada beda dari bawaan
bool isModified(const PartyConfig& cfg, int charIdx, int skillIdx);

// Teks: buang karakter kontrol dan byte UTF-8 rusak, potong pada 'maxChars' karakter (bukan
// byte), rapikan spasi tepi. Kosong setelah dibersihkan -> 'fallback'.
std::string sanitizeText(const std::string& in, int maxChars, const std::string& fallback);

// ---- Bantu layar Customize: satu tempat aturan "field mana yang bisa diubah dan bagaimana" ----

// ---- Class ----
// Pindah ke class berikutnya (+1) / sebelumnya (-1), berputar mengikuti urutan roleName().
void        stepRole(CharCfg& c, int dir);
// Teks bonus sebuah skill: "HP +30%", "DP +30%", "DEV x4" (skill EX: "DEV x5"); kosong bila tanpa bonus.
std::string bonusText(const SkillCfg& s);

enum class SkillField { SpCost, Hits, Power, Bonus, FxType, FxValue, FxTurns, FxScope };
constexpr int kSkillFieldCount = 8;

bool        fieldVisible(const SkillCfg& s, SkillField f);   // tergantung jenis skill dan efek
const char* fieldLabel(const SkillCfg& s, SkillField f);
std::string fieldText(const SkillCfg& s, SkillField f);      // mis. "12", "HP +30%", "ATK+", "40%", "Party"
// Naik (+1) atau turun (-1) satu langkah. Nilai dijepit; jenis efek dan target berputar.
// Mengganti jenis efek mengisi nilai/durasi/target dengan preset yang masuk akal.
void        stepField(SkillCfg& s, SkillField f, int dir);

}  // namespace party_config
