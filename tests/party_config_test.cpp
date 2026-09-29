// Tes konfigurasi party kustom. Berjalan di PC, tanpa devkitPro.
//   g++ -std=c++17 tests/party_config_test.cpp source/party_config.cpp source/combat.cpp -o party_config_test
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "../source/party_config.h"

using namespace party_config;

static void spit(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary);
    out << data;
}

static bool validUtf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (len == 0 || i + len > s.size()) return false;
        for (int k = 1; k < len; ++k)
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
        i += len;
    }
    return true;
}

static int utf8Len(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

// Roster bawaan: 0 Ruka, 1 Yuki, 2 Tama (front); 3 Karen, 4 Megumi, 5 Tsukasa (back).
enum { kRuka, kYuki, kTama, kKaren, kMegumi, kTsukasa };

static void testDefaultsMatchRoster() {
    PartyConfig d = defaults();
    PartyConfig s = d;
    sanitize(s);
    assert(s.chars[0] == d.chars[0] && s.chars[5] == d.chars[5]);   // roster bawaan sudah sah

    // Membangun dari defaults = roster bawaan persis.
    const Party ref = makeDefaultParty();
    const Party got = buildParty(d);
    for (int i = 0; i < 3; ++i) {
        for (int side = 0; side < 2; ++side) {
            const Combatant& a = side ? ref.back[i] : ref.front[i];
            const Combatant& b = side ? got.back[i] : got.front[i];
            assert(a.name == b.name && a.key == b.key && a.max_hp == b.max_hp && a.max_dp == b.max_dp);
            for (int j = 0; j < 2; ++j) {
                assert(std::string(a.skills[j].name) == b.skills[j].name);
                assert(std::string(a.skills[j].description) == b.skills[j].description);
                assert(a.skills[j].sp_cost == b.skills[j].sp_cost && a.skills[j].hits == b.skills[j].hits &&
                       a.skills[j].power == b.skills[j].power && a.skills[j].fx.type == b.skills[j].fx.type &&
                       a.skills[j].fx.value == b.skills[j].fx.value && a.uses_left[j] == b.uses_left[j]);
            }
        }
    }
    assert(std::string(got.front[kTama].skills[0].name) == "Resupply" && got.front[kTama].uses_left[0] == 10);
}

static void testTextSanitizing() {
    // Dipotong per karakter, bukan per byte: nama Jepang tidak boleh terbelah.
    const std::string jp = sanitizeText("ルカルカルカルカルカルカ", 10, "x");
    assert(utf8Len(jp) == 10 && validUtf8(jp) && jp.size() == 30);

    assert(sanitizeText("", 10, "def") == "def");
    assert(sanitizeText("   \t ", 10, "def") == "def");
    assert(sanitizeText("  Ruka  ", 10, "def") == "Ruka");
    assert(sanitizeText("Ru\nka\x01", 10, "def") == "Ruka");                 // karakter kontrol dibuang
    assert(validUtf8(sanitizeText(std::string("A\xE3\x83", 3) + "B", 10, "d")));   // urutan UTF-8 terpotong dibuang
    assert(sanitizeText("\xFF\xFE", 10, "def") == "def");
    assert(sanitizeText("Ruka Kayamori Extra", 10, "def") == "Ruka Kayam");
    assert(sanitizeText("Ruka Kayamori", kNameMaxChars, "def") == "Ruka Kay");
}

static void testClampAndRules() {
    PartyConfig c = defaults();

    // Nilai gila dijepit.
    SkillCfg& atk = c.chars[kRuka].skills[0];         // Attack
    atk.sp_cost = 999; atk.hits = -5; atk.power = 9999;
    atk.fx_type = EffectType::DefDown; atk.fx_value = 9999; atk.fx_turns = 50; atk.fx_scope = EffectScope::Self;
    // Skill dukungan tanpa efek harus kembali ke efek bawaan.
    c.chars[kTsukasa].skills[0].fx_type = EffectType::None;
    // Heal tidak boleh punya efek.
    c.chars[kTama].skills[0].fx_type = EffectType::AtkUp;
    c.chars[kTama].skills[0].power = 9999;
    // Buff tidak boleh menarget musuh; DefUp minimal 1 giliran (0 = tidak pernah habis).
    SkillCfg& buff = c.chars[kYuki].skills[1];
    buff.fx_type = EffectType::DefUp; buff.fx_value = 1; buff.fx_turns = 0; buff.fx_scope = EffectScope::Enemy;
    // Nama rusak
    c.chars[kKaren].name = "";
    c.chars[kMegumi].skills[1].name = "\x01\x02";
    // Bagian hanya-baca tidak bisa dipalsukan.
    c.chars[kRuka].key = "Hacker";
    c.chars[kTama].skills[0].kind = SkillKind::Attack;

    sanitize(c);
    assert(atk.sp_cost == kSpCostMax && atk.hits == kHitsMin && atk.power == kAttackPowerMax);
    assert(atk.fx_type == EffectType::DefDown && atk.fx_value == 80 && atk.fx_turns == kFxTurnsMax &&
           atk.fx_scope == EffectScope::Enemy);          // debuff selalu menarget musuh
    assert(c.chars[kTsukasa].skills[0].fx_type == EffectType::AtkUp);
    assert(c.chars[kTama].skills[0].fx_type == EffectType::None && c.chars[kTama].skills[0].power == kHealPowerMax);
    assert(c.chars[kTama].skills[0].kind == SkillKind::HealDP);
    assert(buff.fx_value == 5 && buff.fx_turns == 1 && buff.fx_scope == EffectScope::Self);
    assert(c.chars[kKaren].name == "Karen");
    assert(c.chars[kMegumi].skills[1].name == "Hard Knocks");
    assert(c.chars[kRuka].key == "Ruka");

    PartyConfig again = c;
    sanitize(again);
    assert(again.chars[kRuka] == c.chars[kRuka]);       // idempoten
}

static void testStepField() {
    PartyConfig c = defaults();

    // Attack tanpa efek: efek bisa ditambah, lalu kolom terkait muncul.
    SkillCfg& s = c.chars[kRuka].skills[1];             // Cross Cut, tanpa efek
    assert(s.fx_type == EffectType::None);
    assert(fieldVisible(s, SkillField::Hits) && fieldVisible(s, SkillField::FxType));
    assert(!fieldVisible(s, SkillField::FxValue) && !fieldVisible(s, SkillField::FxScope));
    stepField(s, SkillField::FxType, +1);
    assert(s.fx_type == EffectType::AtkUp && fieldVisible(s, SkillField::FxValue) &&
           fieldVisible(s, SkillField::FxScope));
    assert(fieldText(s, SkillField::FxType) == "ATK+");
    stepField(s, SkillField::FxValue, +1);
    assert(s.fx_value == 35 && fieldText(s, SkillField::FxValue) == "35%");
    for (int i = 0; i < 50; ++i) stepField(s, SkillField::FxValue, +1);
    assert(s.fx_value == 80);                            // berhenti di batas
    stepField(s, SkillField::FxScope, +1);
    assert(s.fx_scope == EffectScope::Self || s.fx_scope == EffectScope::AllFront ||
           s.fx_scope == EffectScope::AllParty);
    // Ganti ke debuff: target otomatis musuh dan kolom Target hilang.
    stepField(s, SkillField::FxType, +1);                // DefUp
    stepField(s, SkillField::FxType, +1);                // DevUp
    stepField(s, SkillField::FxType, +1);                // DefDown
    assert(s.fx_type == EffectType::DefDown && s.fx_scope == EffectScope::Enemy &&
           !fieldVisible(s, SkillField::FxScope) && s.fx_turns >= 1);
    stepField(s, SkillField::FxType, +1);                // AtkDown
    stepField(s, SkillField::FxType, +1);                // kembali ke None
    assert(s.fx_type == EffectType::None && !fieldVisible(s, SkillField::FxValue));

    // SP dan hits dijepit.
    for (int i = 0; i < 40; ++i) stepField(s, SkillField::SpCost, +1);
    assert(s.sp_cost == kSpCostMax);
    for (int i = 0; i < 40; ++i) stepField(s, SkillField::Hits, -1);
    assert(s.hits == kHitsMin);

    // Support tidak boleh sampai ke "None".
    SkillCfg& sup = c.chars[kTsukasa].skills[0];
    for (int i = 0; i < 12; ++i) {
        stepField(sup, SkillField::FxType, -1);
        assert(sup.fx_type != EffectType::None);
    }
    assert(!fieldVisible(sup, SkillField::Hits) && !fieldVisible(sup, SkillField::Power));

    // Heal: hanya SP dan DP dipulihkan.
    SkillCfg& heal = c.chars[kTama].skills[0];
    assert(!fieldVisible(heal, SkillField::FxType) && !fieldVisible(heal, SkillField::Hits));
    assert(fieldVisible(heal, SkillField::Power) && std::string(fieldLabel(heal, SkillField::Power)) == "DP Restored");
    stepField(heal, SkillField::FxType, +1);            // tidak berbuat apa-apa
    assert(heal.fx_type == EffectType::None);

    sanitize(c);                                          // hasil edit lewat stepField selalu sah
    assert(c.chars[kRuka].skills[1].sp_cost == kSpCostMax);
}

static void testSaveLoad() {
    const std::string path = "/tmp/party_config_test_dir/party.cfg";

    PartyConfig missing;
    assert(!load("/tmp/party_config_test_dir/tidak_ada.cfg", missing));
    assert(missing.chars[0] == defaults().chars[0]);      // berkas tidak ada = bawaan

    PartyConfig c = defaults();
    c.chars[kRuka].name = "ルカ";
    c.chars[kRuka].skills[0].name = "Serangan Kilat";
    c.chars[kRuka].skills[0].sp_cost = 7;
    c.chars[kRuka].skills[0].hits = 4;
    c.chars[kRuka].skills[0].power = 11;
    c.chars[kRuka].skills[1].fx_type = EffectType::AtkDown;
    c.chars[kRuka].skills[1].fx_value = 25;
    c.chars[kRuka].skills[1].fx_turns = 2;
    c.chars[kRuka].skills[1].fx_scope = EffectScope::Enemy;
    c.chars[kTama].skills[0].power = 33;                // heal
    c.chars[kTsukasa].skills[0].fx_type = EffectType::DefUp;
    c.chars[kTsukasa].skills[0].fx_value = 45;
    c.chars[kTsukasa].skills[0].fx_turns = 3;
    c.chars[kTsukasa].skills[0].fx_scope = EffectScope::AllFront;
    sanitize(c);
    assert(save(path, c));

    PartyConfig back;
    assert(load(path, back));
    for (int i = 0; i < kRosterSize; ++i) assert(back.chars[i] == c.chars[i]);
    assert(isModified(back, kRuka) && !isModified(back, kYuki) && isModified(back, kRuka, 0) &&
           isModified(back, kRuka, 1));

    // Berkas hasil edit tangan: CRLF, komentar, section asing, angka rusak, di luar batas.
    spit(path,
         "# komentar\r\n"
         "[ruka]\r\n"
         "name = Ruka Sensei Panjang Sekali\r\n"
         "skill1.sp=abc\r\n"          // rusak: pakai bawaan
         "skill1.hits=99\r\n"          // di luar batas: dijepit
         "skill1.power = 7\r\n"
         "skill2.fx=DEF_UP\r\n"        // huruf besar tetap dikenali
         "skill2.fx_value=1000\r\n"
         "skill2.fx_scope=enemy\r\n"   // buff tidak boleh ke musuh
         "skill9.sp=3\r\n"             // skill tidak ada
         "[orang_asing]\r\n"
         "name=Hacker\r\n"
         "[Karen]\r\n"                 // nama section tidak peka huruf besar-kecil
         "name=Karrie\r\n"
         "tanpa_sama_dengan\r\n");
    PartyConfig h;
    assert(load(path, h));
    assert(h.chars[kRuka].name == "Ruka Sen");                          // dipotong 8 karakter
    assert(h.chars[kRuka].skills[0].sp_cost == defaults().chars[kRuka].skills[0].sp_cost);
    assert(h.chars[kRuka].skills[0].hits == kHitsMax && h.chars[kRuka].skills[0].power == 7);
    assert(h.chars[kRuka].skills[1].fx_type == EffectType::DefUp && h.chars[kRuka].skills[1].fx_value == 70 &&
           h.chars[kRuka].skills[1].fx_scope == EffectScope::Self);
    assert(h.chars[kKaren].name == "Karrie");
    for (int i = 0; i < kRosterSize; ++i) assert(h.chars[i].name != "Hacker");

    // Isi sampah total tidak boleh crash dan hasilnya sah.
    spit(path, std::string("\x00\x01\xFF[[[]]]===\n=x\n[ruka]\nname=\n", 29));
    PartyConfig g;
    assert(load(path, g));
    assert(g.chars[kRuka].name == "Ruka");
    std::remove(path.c_str());
}

static void testBuildPartyAndBattle() {
    PartyConfig c = defaults();
    c.chars[kRuka].name = "Ruka-san";
    c.chars[kRuka].skills[1].name = "Tebasan Ganda";
    c.chars[kRuka].skills[1].sp_cost = 0;
    c.chars[kRuka].skills[1].hits = 3;
    c.chars[kRuka].skills[1].power = 20;
    c.chars[kMegumi].skills[1].fx_type = EffectType::AtkDown;   // efek bawaan diganti -> deskripsi diganti
    sanitize(c);

    Party p = buildParty(c);
    assert(p.front[0].name == "Ruka-san" && p.front[0].key == "Ruka");   // ikon tetap terkunci ke "Ruka"
    assert(std::string(p.front[0].skills[1].name) == "Tebasan Ganda");
    assert(p.front[0].skills[1].sp_cost == 0 && p.front[0].skills[1].hits == 3 && p.front[0].skills[1].power == 20);
    assert(std::string(p.back[1].skills[1].description) == "Custom effect.");
    // Yang tidak diubah tetap deskripsi aslinya.
    assert(std::string(p.front[0].skills[0].description) == std::string(makeDefaultParty().front[0].skills[0].description));

    // Pointer nama tetap valid setelah Party lain dibuat dan dibuang (pool teks).
    const char* n = p.front[0].skills[1].name;
    {
        PartyConfig other = defaults();
        other.chars[kRuka].skills[1].name = "Nama Lain";
        Party q = buildParty(other);
        (void)q;
    }
    assert(std::string(n) == "Tebasan Ganda");

    // Dipakai di battle sungguhan.
    std::srand(5);
    Battle b;
    b.setPartyOverride(p);
    b.start();
    assert(b.party().front[0].name == "Ruka-san");
    assert(b.canUseSkill(0, 1));                         // biaya SP 0
    const int dp0 = b.enemy().dp;
    assert(b.setCommand(0, CommandType::Skill, 1));
    b.execute();
    bool sawName = false;
    for (int i = 0; i < 100000 && b.phase() != BattlePhase::Planning; ++i) {
        b.update();
        if (b.message().find("Ruka-san uses Tebasan Ganda") != std::string::npos) sawName = true;
    }
    assert(sawName);
    assert(b.enemy().dp < dp0 || b.enemy().hp < 500);   // damage benar-benar masuk

    // start() ulang selalu mulai dari kondisi awal (party override tidak ikut termutasi).
    b.start();
    assert(b.party().front[0].sp == kStartSP && b.party().front[0].hp == b.party().front[0].max_hp);
    assert(b.enemy().hp == 500 && b.round() == 1);

    // Tanpa override: roster bawaan.
    Battle d;
    d.setPartyOverride(p);
    d.clearPartyOverride();
    d.start();
    assert(d.party().front[0].name == "Ruka");
}

static void testResets() {
    PartyConfig c = defaults();
    c.chars[kYuki].name = "Yuu";
    c.chars[kYuki].skills[0].power = 20;
    c.chars[kYuki].skills[1].sp_cost = 1;
    assert(isModified(c, kYuki) && isModified(c, kYuki, 0) && isModified(c, kYuki, 1));

    resetSkill(c, kYuki, 0);
    assert(!isModified(c, kYuki, 0) && isModified(c, kYuki, 1) && c.chars[kYuki].name == "Yuu");
    resetChar(c, kYuki);
    assert(!isModified(c, kYuki));

    c.chars[kTama].name = "Tam";
    resetAll(c);
    assert(!isModified(c, kTama));
    resetChar(c, 99);                                     // indeks buruk: tidak crash
    resetSkill(c, 0, 5);
    assert(!isModified(c, -1));
}

int main() {
    testDefaultsMatchRoster();
    testTextSanitizing();
    testClampAndRules();
    testStepField();
    testSaveLoad();
    testBuildPartyAndBattle();
    testResets();
    std::puts("party_config OK");
    return 0;
}
