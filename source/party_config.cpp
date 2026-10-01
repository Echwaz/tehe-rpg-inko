#include "party_config.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace party_config {
namespace {

// ---- string milik selamanya untuk Skill::name / Skill::description (const char*) ----
// Skill menyimpan const char*, sedangkan nama kustom berasal dari std::string yang bisa berubah
// atau hilang. Pool ini menyimpan tiap teks unik sekali dan tidak pernah menghapusnya, jadi
// pointer-nya aman dipakai sepanjang program. Ukurannya dibatasi jumlah nama unik yang pernah
// dibuat pemain, yaitu kecil sekali.
const char* intern(const std::string& s) {
    static std::set<std::string> pool;
    return pool.insert(s).first->c_str();
}

template <typename T>
T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---- aturan efek ----

struct FxRange {
    int  vmin, vmax, vstep;   // nilai (persen; DevUp: poin devastation rate)
    int  tmin, tmax;
    bool buff;                // buff: target Self/AllFront/AllParty. Debuff: selalu Enemy.
};

// Catatan durasi: AtkUp/DevUp durasi 0 = sekali pakai. DefUp/DefDown/AtkDown WAJIB minimal 1,
// karena durasi 0 pada efek itu tidak pernah habis (tickEffects hanya menghapus yang > 0).
FxRange rangeOf(EffectType t) {
    switch (t) {
    case EffectType::AtkUp:   return {  5, 80, 5, 0, kFxTurnsMax, true };
    case EffectType::DefUp:   return {  5, 70, 5, 1, kFxTurnsMax, true };
    case EffectType::DevUp:   return {  1, 20, 1, 0, kFxTurnsMax, true };
    case EffectType::DefDown: return {  5, 80, 5, 1, kFxTurnsMax, false };
    case EffectType::AtkDown: return {  5, 60, 5, 1, kFxTurnsMax, false };
    default:                  return {  0,  0, 1, 0, 0, false };
    }
}

void presetFx(SkillCfg& s, EffectType t) {
    s.fx_type = t;
    switch (t) {
    case EffectType::AtkUp:   s.fx_value = 30; s.fx_turns = 0; s.fx_scope = EffectScope::AllParty; break;
    case EffectType::DefUp:   s.fx_value = 30; s.fx_turns = 2; s.fx_scope = EffectScope::AllParty; break;
    case EffectType::DevUp:   s.fx_value = 4;  s.fx_turns = 0; s.fx_scope = EffectScope::AllParty; break;
    case EffectType::DefDown: s.fx_value = 30; s.fx_turns = 1; s.fx_scope = EffectScope::Enemy;    break;
    case EffectType::AtkDown: s.fx_value = 30; s.fx_turns = 1; s.fx_scope = EffectScope::Enemy;    break;
    default:                  s.fx_value = 0;  s.fx_turns = 0; s.fx_scope = EffectScope::Self;     break;
    }
}

void normalizeFx(SkillCfg& s) {
    if (s.fx_type == EffectType::None) {
        s.fx_value = 0; s.fx_turns = 0; s.fx_scope = EffectScope::Self;
        return;
    }
    const FxRange r = rangeOf(s.fx_type);
    s.fx_value = clampv(s.fx_value, r.vmin, r.vmax);
    s.fx_turns = clampv(s.fx_turns, r.tmin, r.tmax);
    if (r.buff) { if (s.fx_scope == EffectScope::Enemy) s.fx_scope = EffectScope::Self; }
    else        { s.fx_scope = EffectScope::Enemy; }
}

const EffectType kFxOrder[] = { EffectType::None, EffectType::AtkUp, EffectType::DefUp,
                                EffectType::DevUp, EffectType::DefDown, EffectType::AtkDown };
const int kFxOrderCount = static_cast<int>(sizeof kFxOrder / sizeof kFxOrder[0]);

// nama di berkas <-> enum
struct FxName { const char* key; EffectType type; const char* label; };
const FxName kFxNames[] = {
    { "none",     EffectType::None,    "None" },
    { "atk_up",   EffectType::AtkUp,   "ATK+" },
    { "def_up",   EffectType::DefUp,   "DEF+" },
    { "dev_up",   EffectType::DevUp,   "DEV+" },
    { "def_down", EffectType::DefDown, "DEF-" },
    { "atk_down", EffectType::AtkDown, "ATK-" },
};
struct BonusName { const char* key; SkillBonus bonus; };
const BonusName kBonusNames[] = {
    { "none", SkillBonus::None },   { "hp", SkillBonus::HpEff },
    { "dp", SkillBonus::DpEff },    { "dev", SkillBonus::Devastation },
};
const char* bonusKey(SkillBonus b) { for (const auto& n : kBonusNames) if (n.bonus == b) return n.key; return "none"; }

struct ScopeName { const char* key; EffectScope scope; const char* label; };
const ScopeName kScopeNames[] = {
    { "self",  EffectScope::Self,     "Self" },
    { "front", EffectScope::AllFront, "Front" },
    { "party", EffectScope::AllParty, "Party" },
    { "enemy", EffectScope::Enemy,    "Enemy" },
};

const char* fxKey(EffectType t)     { for (const auto& n : kFxNames) if (n.type == t) return n.key;   return "none"; }
const char* fxLabel(EffectType t)   { for (const auto& n : kFxNames) if (n.type == t) return n.label; return "None"; }
const char* scopeKey(EffectScope s) { for (const auto& n : kScopeNames) if (n.scope == s) return n.key;   return "self"; }
const char* scopeLabel(EffectScope s){ for (const auto& n : kScopeNames) if (n.scope == s) return n.label; return "Self"; }

// ---- roster bawaan <-> konfigurasi ----

SkillCfg fromSkill(const Skill& s) {
    SkillCfg k;
    k.kind     = s.kind;
    k.ex       = s.ex;
    k.name     = s.name ? s.name : "";
    k.sp_cost  = s.sp_cost;
    k.hits     = s.hits;
    k.power    = s.power;
    k.bonus    = s.bonus;
    k.fx_type  = s.fx.type;
    k.fx_value = s.fx.value;
    k.fx_turns = s.fx.turns;
    k.fx_scope = s.fx.scope;
    return k;
}

CharCfg fromCombatant(const Combatant& c) {
    CharCfg r;
    r.key  = c.key;
    r.role = c.role ? c.role : "";
    r.name = c.name;
    r.skills[0] = fromSkill(c.skills[0]);
    r.skills[1] = fromSkill(c.skills[1]);
    return r;
}

const PartyConfig& defaultsRef() {
    static const PartyConfig d = defaults();
    return d;
}

void sanitizeSkill(SkillCfg& s, const SkillCfg& d) {
    s.kind    = d.kind;
    s.ex      = d.ex;
    s.name    = sanitizeText(s.name, kSkillNameMaxChars, d.name);
    s.sp_cost = clampv(s.sp_cost, 0, kSpCostMax);

    switch (d.kind) {
    case SkillKind::Attack:
        s.hits  = clampv(s.hits, kHitsMin, kHitsMax);
        s.power = clampv(s.power, 1, kAttackPowerMax);
        s.bonus = static_cast<SkillBonus>(clampv(static_cast<int>(s.bonus), 0, kSkillBonusCount - 1));
        normalizeFx(s);
        break;
    case SkillKind::HealDP:
        s.hits  = d.hits;
        s.bonus = SkillBonus::None;         // bonus hanya untuk skill serangan
        s.power = clampv(s.power, 1, kHealPowerMax);
        s.fx_type = EffectType::None;       // heal tidak memasang efek
        normalizeFx(s);
        break;
    case SkillKind::Support:
        s.hits  = d.hits;
        s.power = d.power;
        s.bonus = SkillBonus::None;
        if (s.fx_type == EffectType::None) {        // skill dukungan tanpa efek = tidak berguna
            s.fx_type = d.fx_type; s.fx_value = d.fx_value;
            s.fx_turns = d.fx_turns; s.fx_scope = d.fx_scope;
        }
        normalizeFx(s);
        break;
    }
}

// ---- parsing berkas ----

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool parseInt(const std::string& v, int& out) {
    if (v.empty()) return false;
    char* end = nullptr;
    const long x = std::strtol(v.c_str(), &end, 10);
    if (!end || *end != '\0') return false;
    out = static_cast<int>(clampv<long>(x, -100000, 100000));
    return true;
}

void applyKey(CharCfg& c, const std::string& key, const std::string& val) {
    if (key == "name") { c.name = val; return; }
    if (key == "role" || key == "class") { c.role = val; return; }
    if (key.size() < 8 || key.compare(0, 5, "skill") != 0 || key[6] != '.') return;   // "skillN.xxx"
    const int idx = key[5] - '1';
    if (idx < 0 || idx > 1) return;
    SkillCfg& s = c.skills[idx];
    const std::string f = key.substr(7);
    int n = 0;
    if      (f == "name")     s.name = val;
    else if (f == "sp")       { if (parseInt(val, n)) s.sp_cost  = n; }
    else if (f == "hits")     { if (parseInt(val, n)) s.hits     = n; }
    else if (f == "power")    { if (parseInt(val, n)) s.power    = n; }
    else if (f == "fx_value") { if (parseInt(val, n)) s.fx_value = n; }
    else if (f == "fx_turns") { if (parseInt(val, n)) s.fx_turns = n; }
    else if (f == "bonus") {
        for (const auto& e : kBonusNames) if (lower(val) == e.key) s.bonus = e.bonus;
    }
    else if (f == "fx") {
        for (const auto& e : kFxNames) if (lower(val) == e.key) s.fx_type = e.type;
    } else if (f == "fx_scope") {
        for (const auto& e : kScopeNames) if (lower(val) == e.key) s.fx_scope = e.scope;
    }
}

void ensureParentDir(const std::string& path) {
#if !defined(_WIN32)
    const size_t p = path.find_last_of('/');
    if (p == std::string::npos || p == 0) return;
    mkdir(path.substr(0, p).c_str(), 0777);   // gagal (sudah ada) tidak masalah
#else
    (void)path;
#endif
}

}  // namespace


// ================= API publik =================

PartyConfig defaults() {
    const Party p = makeDefaultParty();
    PartyConfig cfg;
    for (int i = 0; i < 3; ++i) {
        cfg.chars[i]     = fromCombatant(p.front[i]);
        cfg.chars[3 + i] = fromCombatant(p.back[i]);
    }
    return cfg;
}

std::string sanitizeText(const std::string& in, int maxChars, const std::string& fallback) {
    std::string out;
    int chars = 0;
    size_t i = 0;
    while (i < in.size() && chars < maxChars) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        int len = 0;
        if (c < 0x80)              len = 1;
        else if ((c >> 5) == 0x06) len = 2;
        else if ((c >> 4) == 0x0E) len = 3;
        else if ((c >> 3) == 0x1E) len = 4;
        bool ok = len > 0 && i + static_cast<size_t>(len) <= in.size();
        for (int k = 1; ok && k < len; ++k)
            if ((static_cast<unsigned char>(in[i + k]) & 0xC0) != 0x80) ok = false;
        if (!ok || (len == 1 && (c < 0x20 || c == 0x7F))) { ++i; continue; }   // buang byte rusak / kontrol
        out.append(in, i, static_cast<size_t>(len));
        i += static_cast<size_t>(len);
        ++chars;
    }
    out = trim(out);
    return out.empty() ? fallback : out;
}

void sanitize(PartyConfig& cfg) {
    const PartyConfig& d = defaultsRef();
    for (int i = 0; i < kRosterSize; ++i) {
        CharCfg& c = cfg.chars[i];
        c.key  = d.chars[i].key;
        const int ri = roleIndex(c.role.c_str());
        c.role = ri >= 0 ? std::string(roleName(ri)) : d.chars[i].role;   // class tak dikenal = bawaan
        c.name = sanitizeText(c.name, kNameMaxChars, d.chars[i].name);
        for (int j = 0; j < 2; ++j) sanitizeSkill(c.skills[j], d.chars[i].skills[j]);
    }
}

Party buildParty(const PartyConfig& in) {
    PartyConfig cfg = in;
    sanitize(cfg);
    const PartyConfig& def = defaultsRef();

    Party p = makeDefaultParty();
    for (int i = 0; i < kRosterSize; ++i) {
        Combatant& m = i < 3 ? p.front[i] : p.back[i - 3];
        const CharCfg& c = cfg.chars[i];
        m.name = c.name;                       // m.key tetap nama bawaan
        for (int j = 0; j < 2; ++j) {
            Skill& s = m.skills[j];
            const SkillCfg& k  = c.skills[j];
            const SkillCfg& dk = def.chars[i].skills[j];

            s.name    = intern(k.name);
            s.sp_cost = k.sp_cost;
            s.bonus   = k.bonus;
            s.refreshBonus();
            if (k.kind == SkillKind::Attack) { s.hits = k.hits; s.power = k.power; }
            if (k.kind == SkillKind::HealDP) { s.power = k.power; }
            if (k.kind == SkillKind::Attack || k.kind == SkillKind::Support) {
                s.fx.type  = k.fx_type;
                s.fx.value = k.fx_value;
                s.fx.turns = k.fx_turns;
                s.fx.scope = k.fx_scope;
            }
            // Deskripsi bawaan menyebut efek aslinya; kalau efek itu diganti, tulisan lama menyesatkan.
            if (dk.fx_type != EffectType::None &&
                (k.fx_type != dk.fx_type || k.fx_scope != dk.fx_scope))
                s.description = intern("Custom effect.");
            m.uses_left[j] = s.max_uses > 0 ? s.max_uses : -1;
        }
        setRole(m, c.role.c_str());            // hanya label; tidak memengaruhi skill
    }
    return p;
}

bool load(const std::string& path, PartyConfig& cfg) {
    cfg = defaults();
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    std::string data;
    char buf[1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0 && data.size() < 65536) data.append(buf, n);
    std::fclose(f);

    int cur = -1;   // karakter yang sedang dibaca; -1 = section tidak dikenal (baris diabaikan)
    size_t pos = 0;
    while (pos <= data.size()) {
        size_t eol = data.find('\n', pos);
        if (eol == std::string::npos) eol = data.size();
        const std::string line = trim(data.substr(pos, eol - pos));    // trim juga membuang '\r'
        pos = eol + 1;

        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            const std::string sec = lower(trim(line.substr(1, line.size() - 2)));
            cur = -1;
            for (int i = 0; i < kRosterSize; ++i)
                if (lower(cfg.chars[i].key) == sec) { cur = i; break; }
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos || cur < 0) continue;
        applyKey(cfg.chars[cur], lower(trim(line.substr(0, eq))), trim(line.substr(eq + 1)));
    }
    sanitize(cfg);
    return true;
}

bool save(const std::string& path, const PartyConfig& in) {
    PartyConfig cfg = in;
    sanitize(cfg);
    ensureParentDir(path);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    std::fprintf(f, "# Tehe-RPG-inko: custom party\n");
    std::fprintf(f, "# Bisa diedit lewat layar Customize (tekan SELECT saat dialog) atau langsung di sini.\n");
    std::fprintf(f, "# role: attacker breaker blaster healer buffer debuffer\n");
    std::fprintf(f, "#   Hanya label, tanpa efek. Bonus diatur per skill lewat skillN.bonus.\n");
    std::fprintf(f, "# bonus: none hp dp dev (hp = damage HP +30%%, dp = damage DP +30%%, dev = devastation x4 (skill EX x5))\n");
    std::fprintf(f, "# fx: none atk_up def_up dev_up def_down atk_down\n");
    std::fprintf(f, "# fx_scope: self front party (buff) | enemy (debuff, otomatis)\n");
    std::fprintf(f, "# Nilai di luar batas dijepit otomatis saat game dimuat.\n\n");
    for (const CharCfg& c : cfg.chars) {
        std::fprintf(f, "[%s]\nname=%s\nrole=%s\n", lower(c.key).c_str(), c.name.c_str(), c.role.c_str());
        for (int j = 0; j < 2; ++j) {
            const SkillCfg& s = c.skills[j];
            const int n = j + 1;
            std::fprintf(f, "skill%d.name=%s\nskill%d.sp=%d\n", n, s.name.c_str(), n, s.sp_cost);
            if (s.kind == SkillKind::Attack) std::fprintf(f, "skill%d.hits=%d\n", n, s.hits);
            if (s.kind != SkillKind::Support) std::fprintf(f, "skill%d.power=%d\n", n, s.power);
            if (s.kind == SkillKind::Attack) std::fprintf(f, "skill%d.bonus=%s\n", n, bonusKey(s.bonus));
            if (s.kind != SkillKind::HealDP) {
                std::fprintf(f, "skill%d.fx=%s\nskill%d.fx_value=%d\nskill%d.fx_turns=%d\nskill%d.fx_scope=%s\n",
                             n, fxKey(s.fx_type), n, s.fx_value, n, s.fx_turns, n, scopeKey(s.fx_scope));
            }
        }
        std::fprintf(f, "\n");
    }
    const bool ok = std::ferror(f) == 0;
    return std::fclose(f) == 0 && ok;
}

void resetAll(PartyConfig& cfg) { cfg = defaults(); }

void resetChar(PartyConfig& cfg, int i) {
    if (i < 0 || i >= kRosterSize) return;
    cfg.chars[i] = defaultsRef().chars[i];
}

void resetSkill(PartyConfig& cfg, int i, int j) {
    if (i < 0 || i >= kRosterSize || j < 0 || j > 1) return;
    cfg.chars[i].skills[j] = defaultsRef().chars[i].skills[j];
}

bool isModified(const PartyConfig& cfg, int i) {
    return i >= 0 && i < kRosterSize && cfg.chars[i] != defaultsRef().chars[i];
}

bool isModified(const PartyConfig& cfg, int i, int j) {
    return i >= 0 && i < kRosterSize && j >= 0 && j <= 1 &&
           cfg.chars[i].skills[j] != defaultsRef().chars[i].skills[j];
}


// ================= class =================

void stepRole(CharCfg& c, int dir) {
    if (dir != 1 && dir != -1) return;
    const int idx = std::max(0, roleIndex(c.role.c_str()));
    c.role = roleName((idx + dir + kRoleCount) % kRoleCount);
}

std::string bonusText(const SkillCfg& s) {
    if (s.kind != SkillKind::Attack) return "";
    char buf[24];
    switch (s.bonus) {
    case SkillBonus::HpEff:
        std::snprintf(buf, sizeof buf, "HP +%d%%", kHpEffPct - 100);
        return buf;
    case SkillBonus::DpEff:
        std::snprintf(buf, sizeof buf, "DP +%d%%", kDpEffPct - 100);
        return buf;
    case SkillBonus::Devastation:
        std::snprintf(buf, sizeof buf, "DEV x%d", s.ex ? kBlasterSignatureDevMult : kBlasterDevMult);
        return buf;
    default:
        return "";
    }
}


// ================= bantu layar Customize =================

bool fieldVisible(const SkillCfg& s, SkillField f) {
    const bool atk = s.kind == SkillKind::Attack;
    const bool heal = s.kind == SkillKind::HealDP;
    const bool sup = s.kind == SkillKind::Support;
    const bool hasFx = (atk || sup) && s.fx_type != EffectType::None;
    switch (f) {
    case SkillField::SpCost:  return true;
    case SkillField::Hits:    return atk;
    case SkillField::Power:   return atk || heal;
    case SkillField::Bonus:   return atk;
    case SkillField::FxType:  return atk || sup;
    case SkillField::FxValue:
    case SkillField::FxTurns: return hasFx;
    case SkillField::FxScope: return hasFx && rangeOf(s.fx_type).buff;
    }
    return false;
}

const char* fieldLabel(const SkillCfg& s, SkillField f) {
    switch (f) {
    case SkillField::SpCost:  return "SP Cost";
    case SkillField::Hits:    return "Hits";
    case SkillField::Power:   return s.kind == SkillKind::HealDP ? "DP Restored" : "Power";
    case SkillField::Bonus:   return "Bonus";
    case SkillField::FxType:  return "Effect";
    case SkillField::FxValue: return "Amount";
    case SkillField::FxTurns: return "Duration";
    case SkillField::FxScope: return "Target";
    }
    return "";
}

std::string fieldText(const SkillCfg& s, SkillField f) {
    char buf[24];
    switch (f) {
    case SkillField::SpCost:  std::snprintf(buf, sizeof buf, "%d", s.sp_cost); return buf;
    case SkillField::Hits:    std::snprintf(buf, sizeof buf, "%d", s.hits);    return buf;
    case SkillField::Power:   std::snprintf(buf, sizeof buf, "%d", s.power);   return buf;
    case SkillField::Bonus:   return s.bonus == SkillBonus::None ? std::string("None") : bonusText(s);
    case SkillField::FxType:  return fxLabel(s.fx_type);
    case SkillField::FxValue:
        std::snprintf(buf, sizeof buf, s.fx_type == EffectType::DevUp ? "%d" : "%d%%", s.fx_value);
        return buf;
    case SkillField::FxTurns:
        if (s.fx_turns <= 0) return "Once";
        std::snprintf(buf, sizeof buf, s.fx_turns == 1 ? "%d turn" : "%d turns", s.fx_turns);
        return buf;
    case SkillField::FxScope: return scopeLabel(s.fx_scope);
    }
    return "";
}

void stepField(SkillCfg& s, SkillField f, int dir) {
    if (dir != 1 && dir != -1) return;
    if (!fieldVisible(s, f)) return;

    switch (f) {
    case SkillField::SpCost: s.sp_cost = clampv(s.sp_cost + dir, 0, kSpCostMax); break;
    case SkillField::Hits:   s.hits    = clampv(s.hits + dir, kHitsMin, kHitsMax); break;
    case SkillField::Power:
        s.power = clampv(s.power + dir, 1, s.kind == SkillKind::HealDP ? kHealPowerMax : kAttackPowerMax);
        break;
    case SkillField::Bonus:
        s.bonus = static_cast<SkillBonus>((static_cast<int>(s.bonus) + dir + kSkillBonusCount) % kSkillBonusCount);
        break;
    case SkillField::FxType: {
        int idx = 0;
        for (int i = 0; i < kFxOrderCount; ++i) if (kFxOrder[i] == s.fx_type) idx = i;
        do { idx = (idx + dir + kFxOrderCount) % kFxOrderCount; }
        while (s.kind == SkillKind::Support && kFxOrder[idx] == EffectType::None);   // Support wajib punya efek
        presetFx(s, kFxOrder[idx]);
        break;
    }
    case SkillField::FxValue: {
        const FxRange r = rangeOf(s.fx_type);
        s.fx_value = clampv(s.fx_value + dir * r.vstep, r.vmin, r.vmax);
        break;
    }
    case SkillField::FxTurns: {
        const FxRange r = rangeOf(s.fx_type);
        s.fx_turns = clampv(s.fx_turns + dir, r.tmin, r.tmax);
        break;
    }
    case SkillField::FxScope: {
        static const EffectScope kBuffScopes[] = { EffectScope::Self, EffectScope::AllFront, EffectScope::AllParty };
        int idx = 0;
        for (int i = 0; i < 3; ++i) if (kBuffScopes[i] == s.fx_scope) idx = i;
        s.fx_scope = kBuffScopes[(idx + dir + 3) % 3];
        break;
    }
    }
    normalizeFx(s);
}

}  // namespace party_config
