#include <algorithm>
#include <cstdio>
#include <string>

#include "assets.h"
#include "screens.h"

using namespace party_config;

namespace {

enum ButtonId {
    BtnTile0 = 1,                    // 1..6: karakter di halaman Roster
    BtnDone = 10, BtnResetAll, BtnBack, BtnResetChar, BtnResetSkill, BtnRename,
    BtnSkill0 = 20,                  // 20, 21: dua skill di halaman Character
    BtnMinus0 = 30,                  // 30 + SkillField
    BtnPlus0  = 40,                  // 40 + SkillField
    BtnClassMinus = 50, BtnClassPlus  // halaman Character: ganti class
};

// Halaman Roster: 3 kolom x 2 baris kotak karakter.
const float kTileW = 94.f, kTileH = 80.f;
float tileX(int i) { return 10.f + (i % 3) * 103.f; }   
float tileY(int i) { return 32.f + (i / 3) * 86.f; }

// Halaman Roster dan Character: tombol bawah.
const float kFootY = 206.f, kFootH = 28.f;
// Halaman Character: baris pemilih class (di bawah dua skill).
const float kClassRowY = 160.f;
// Halaman Skill: baris nilai dan tombol bawah.
const float kRowY0 = 40.f, kRowStep = 21.f, kRowH = 19.f;   // 8 baris nilai muat di atas tombol bawah
const float kSkillFootY = 210.f, kSkillFootH = 26.f;
const int   kToastFrames = 150;

float rowY(int visibleIndex) { return kRowY0 + visibleIndex * kRowStep; }

// Tombol bulat di ujung kiri/kanan tiap baris nilai. Angka memakai - dan +, pilihan (Class,
// Bonus, Effect, Target) memakai panah kiri/kanan. Digambar sendiri (bukan lewat ButtonGroup::draw).
const float kBtnR = 8.5f;                       // radius gambar; lebih kecil dari tinggi baris
const float kBtnHit = 22.f;                     // kotak area sentuh (lingkaran r = 11), lebih lega dari gambar
const float kBtnCxL = 176.f, kBtnCxR = 284.f;

enum class Glyph { Minus, Plus, Left, Right };

const u32 kRingIdle = C2D_Color32(255, 255, 255, 150);
u32 cardFill(bool chosen, bool pressed = false, bool enabled = true) {
    return mixColor(colors::bg, colors::pink, pressed ? 0.34f : (chosen ? 0.26f : (enabled ? 0.10f : 0.04f)));
}
u32 cardBorder(bool chosen, bool enabled = true) {
    return chosen ? colors::pink : mixColor(colors::bg, colors::white, enabled ? 0.45f : 0.2f);
}

const float kCardRadius = 12.f;
void drawRoundCard(float x, float y, float w, float h, u32 fill, u32 border) {
    drawRoundRect(x, y, w, h, kCardRadius, border, kZPanel);
    drawRoundRect(x + 1.6f, y + 1.6f, w - 3.2f, h - 3.2f, kCardRadius - 1.6f, fill, kZPanel + 0.01f);
}

bool isChoiceField(SkillField f) {
    return f == SkillField::Bonus || f == SkillField::FxType || f == SkillField::FxScope;
}

void addRoundButton(ButtonGroup& g, int id, float cx, float cy, bool enabled) {
    g.add(id, cx - kBtnHit * 0.5f, cy - kBtnHit * 0.5f, kBtnHit, kBtnHit, "", enabled, 0, 0.6f, true);
}

void drawRoundButton(float cx, float cy, bool enabled, bool pressed, Glyph g) {
    cy += pressed ? 1.5f : 0.f;
    const u32 accent = enabled ? colors::pink : colors::grey;
    const u32 fill = pressed ? colors::pinkDeep : mixColor(colors::bg, accent, enabled ? 0.35f : 0.06f);
    drawDisc(cx, cy, kBtnR, accent, kZPanel);
    drawDisc(cx, cy, kBtnR - 1.6f, fill, kZPanel + 0.01f);

    const u32 ink = enabled ? colors::white : colors::grey;
    const float z = kZPanel + 0.02f;
    switch (g) {
    case Glyph::Minus:
        C2D_DrawRectSolid(cx - 4.f, cy - 1.f, z, 8.f, 2.f, ink);
        break;
    case Glyph::Plus:
        C2D_DrawRectSolid(cx - 4.f, cy - 1.f, z, 8.f, 2.f, ink);
        C2D_DrawRectSolid(cx - 1.f, cy - 4.f, z, 2.f, 8.f, ink);
        break;
    case Glyph::Left:
        C2D_DrawTriangle(cx + 2.5f, cy - 4.5f, ink, cx + 2.5f, cy + 4.5f, ink, cx - 3.5f, cy, ink, z);
        break;
    case Glyph::Right:
        C2D_DrawTriangle(cx - 2.5f, cy - 4.5f, ink, cx - 2.5f, cy + 4.5f, ink, cx + 3.5f, cy, ink, z);
        break;
    }
}

// Kecilkan skala teks supaya lebarnya tidak melebihi maxW (nama kustom bisa panjang atau lebar).
float fitScale(TextRenderer& text, const std::string& s, float base, float maxW) {
    const float w = text.width(s.c_str(), base);
    return w > maxW ? std::max(0.25f, base * maxW / w) : base;
}

std::string fxText(const SkillCfg& s, bool showTarget = true) {
    SkillEffect fx;
    fx.type  = s.fx_type;
    fx.value = s.fx_value;
    fx.turns = s.fx_turns;
    // Target (team/enemy) sudah jelas dari jenis buff/debuff, jadi bisa disembunyikan
    // di ringkasan roster. Scope Self tidak menambahkan akhiran apa pun.
    fx.scope = showTarget ? s.fx_scope : EffectScope::Self;
    return effectLabel(fx);
}

// Ringkasan satu baris, mis. "9 hits x 6  ATK+40%(1t) team" (showTarget=false: tanpa "team"/"enemy").
std::string skillLine(const SkillCfg& s, bool showTarget = true) {
    char buf[48];
    std::string out;
    if (s.kind == SkillKind::Attack) {
        std::snprintf(buf, sizeof buf, "%d hit%s x %d", s.hits, s.hits == 1 ? "" : "s", s.power);
        out = buf;
    } else if (s.kind == SkillKind::HealDP) {
        std::snprintf(buf, sizeof buf, "Heal %d DP", s.power);
        out = buf;
    }
    const std::string fx = fxText(s, showTarget);
    if (!fx.empty()) {
        if (!out.empty()) out += "  ";
        out += fx;
    }
    return out;
}

// Apakah satu langkah ke arah dir masih mengubah nilai (untuk menonaktifkan tombol +/-).
bool canStep(const SkillCfg& s, SkillField f, int dir) {
    SkillCfg t = s;
    stepField(t, f, dir);
    return t != s;
}

// Papan ketik sistem. false jika dibatalkan. Memblokir sampai pemain selesai.
bool askText(const char* hint, const std::string& initial, int maxChars, std::string& out) {
    SwkbdState kb;
    swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, maxChars);
    swkbdSetHintText(&kb, hint);
    swkbdSetInitialText(&kb, initial.c_str());
    swkbdSetButton(&kb, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&kb, SWKBD_BUTTON_RIGHT, "OK", true);
    swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    char buf[160] = {};
    const SwkbdButton pressed = swkbdInputText(&kb, buf, sizeof buf);
    if (pressed != SWKBD_BUTTON_RIGHT) return false;
    out = buf;
    return true;
}

// Ikon karakter di tengah (cx, cy) berukuran d. Tanpa gambar: lingkaran dengan huruf depan.
void drawIconFor(TextRenderer& text, const CharCfg& c, float cx, float cy, float d) {
    if (assets::drawIcon(assets::charFromName(c.key), cx - d * 0.5f, cy - d * 0.5f, d, d, kZIcon)) return;
    drawDisc(cx, cy, d * 0.5f, colors::ally, kZSprite);
    const char initial[2] = { c.key.empty() ? '?' : c.key[0], '\0' };
    text.drawCentered(initial, cx, cy, d / 60.f, colors::white);
}

}  // namespace


void CustomizeScreen::enter(const PartyConfig& cfg) {
    cfg_ = cfg;
    sanitize(cfg_);
    page_ = Page::Roster;
    ch_ = 0;
    sk_ = 0;
    exit_ = false;
    confirmReset_ = false;
    frame_ = 0;
    toast_.clear();
    toastFrame_ = -1000;
    buttons_.reset();
    buildButtons();
}

void CustomizeScreen::say(const char* msg) {
    toast_ = msg;
    toastFrame_ = frame_;
}

void CustomizeScreen::buildButtons() {
    buttons_.begin();
    buttons_.useBattleStyle(true);     
    switch (page_) {
    case Page::Roster:
        for (int i = 0; i < kRosterSize; ++i)
            buttons_.add(BtnTile0 + i, tileX(i), tileY(i), kTileW, kTileH, "", true, 0);
        buttons_.add(BtnResetAll, 10, kFootY, 96, kFootH, confirmReset_ ? "Sure?" : "Reset All",
                     true, colors::cyan, 0.45f);
        buttons_.add(BtnDone, 214, kFootY, 96, kFootH, "Done", true, colors::pink, 0.5f);
        break;

    case Page::Character:
        buttons_.add(BtnRename, 218, 14, 92, 30, "Rename", true, colors::cyan, 0.45f);
        buttons_.add(BtnSkill0, 10, 64, 300, 42, "", true, 0);
        buttons_.add(BtnSkill0 + 1, 10, 112, 300, 42, "", true, 0);
        addRoundButton(buttons_, BtnClassMinus, kBtnCxL, kClassRowY + kRowH * 0.5f, true);
        addRoundButton(buttons_, BtnClassPlus,  kBtnCxR, kClassRowY + kRowH * 0.5f, true);
        buttons_.add(BtnResetChar, 10, kFootY, 96, kFootH, "Reset", true, colors::cyan, 0.45f);
        buttons_.add(BtnBack, 214, kFootY, 96, kFootH, "Back", true, colors::pink, 0.5f);
        break;

    case Page::Skill: {
        const SkillCfg& s = cfg_.chars[ch_].skills[sk_];
        buttons_.add(BtnRename, 232, 6, 78, 24, "Rename", true, colors::cyan, 0.4f);
        int k = 0;
        for (int f = 0; f < kSkillFieldCount; ++f) {
            const SkillField field = static_cast<SkillField>(f);
            if (!fieldVisible(s, field)) continue;
            addRoundButton(buttons_, BtnMinus0 + f, kBtnCxL, rowY(k) + kRowH * 0.5f, canStep(s, field, -1));
            addRoundButton(buttons_, BtnPlus0 + f,  kBtnCxR, rowY(k) + kRowH * 0.5f, canStep(s, field, +1));
            ++k;
        }
        buttons_.add(BtnResetSkill, 10, kSkillFootY, 96, kSkillFootH, "Reset", true, colors::cyan, 0.45f);
        buttons_.add(BtnBack, 214, kSkillFootY, 96, kSkillFootH, "Back", true, colors::pink, 0.5f);
        break;
    }
    }
}

void CustomizeScreen::goBack() {
    confirmReset_ = false;
    if (page_ == Page::Skill)          page_ = Page::Character;
    else if (page_ == Page::Character) page_ = Page::Roster;
    else                               exit_ = true;
    buildButtons();
}

void CustomizeScreen::rename() {
    std::string out;
    if (page_ == Page::Skill) {
        SkillCfg& s = cfg_.chars[ch_].skills[sk_];
        if (askText("Skill name", s.name, kSkillNameMaxChars, out))
            s.name = sanitizeText(out, kSkillNameMaxChars, s.name);
    } else {
        CharCfg& c = cfg_.chars[ch_];
        if (askText("Character name", c.name, kNameMaxChars, out))
            c.name = sanitizeText(out, kNameMaxChars, c.name);
    }
}

void CustomizeScreen::handle(int id) {
    if (id != BtnResetAll) confirmReset_ = false;

    if (id >= BtnTile0 && id < BtnTile0 + kRosterSize) {
        ch_ = id - BtnTile0;
        page_ = Page::Character;
        buildButtons();
        return;
    }
    if (id == BtnSkill0 || id == BtnSkill0 + 1) {
        sk_ = id - BtnSkill0;
        page_ = Page::Skill;
        buildButtons();
        return;
    }
    if (id == BtnClassMinus || id == BtnClassPlus) {
        stepRole(cfg_.chars[ch_], id == BtnClassPlus ? +1 : -1);   // hanya label, skill tidak berubah
        return;
    }
    if (id >= BtnMinus0 && id < BtnMinus0 + kSkillFieldCount) {
        stepField(cfg_.chars[ch_].skills[sk_], static_cast<SkillField>(id - BtnMinus0), -1);
        buildButtons();       // kolom yang muncul bisa berubah (mis. efek dari None ke ATK+)
        return;
    }
    if (id >= BtnPlus0 && id < BtnPlus0 + kSkillFieldCount) {
        stepField(cfg_.chars[ch_].skills[sk_], static_cast<SkillField>(id - BtnPlus0), +1);
        buildButtons();
        return;
    }

    switch (id) {
    case BtnDone:
        exit_ = true;
        break;
    case BtnBack:
        goBack();
        break;
    case BtnRename:
        rename();
        break;
    case BtnResetAll:
        if (confirmReset_) { resetAll(cfg_); confirmReset_ = false; say("All characters reset"); }
        else               { confirmReset_ = true; }
        buildButtons();
        break;
    case BtnResetChar:
        resetChar(cfg_, ch_);
        say("Character reset");
        break;
    case BtnResetSkill:
        resetSkill(cfg_, ch_, sk_);
        say("Skill reset");
        buildButtons();
        break;
    default:
        break;
    }
}

void CustomizeScreen::update(const TouchState& touch, u32 keysDown) {
    ++frame_;
    buildButtons();
    const int hit = buttons_.update(touch);

    if (hit != ButtonGroup::kNone) handle(hit);
    else if (keysDown & KEY_B)     goBack();
}


// ================= gambar =================

void CustomizeScreen::drawTop(TextRenderer& text) const {
    text.drawCentered("CUSTOM PARTY", 200, 12, 0.55f, colors::white);

    for (int i = 0; i < kRosterSize; ++i) {
        const CharCfg& c = cfg_.chars[i];
        const float x = 4.f + (i % 3) * 132.f, y = 26.f + (i / 3) * 100.f;  
        const bool selected = page_ != Page::Roster && i == ch_;
        const bool marked = selected || isModified(cfg_, i);
        drawRoundCard(x, y, 128, 96, cardFill(selected), cardBorder(marked));

        const float kRoleScale = 0.38f, kInnerW = 116.f;
        const float roleW = text.width(c.role.c_str(), kRoleScale);
        text.drawShadow(c.name.c_str(), x + 6, y + 4, fitScale(text, c.name, 0.55f, kInnerW - roleW - 6.f), colors::white);
        text.drawRight(c.role.c_str(), x + 6 + kInnerW, y + 9, kRoleScale, colors::grey);
        for (int j = 0; j < 2; ++j) {
            const SkillCfg& s = c.skills[j];
            const float sy = y + 28.f + j * 33.f;
            text.drawShadow(s.name.c_str(), x + 6, sy, fitScale(text, s.name, 0.44f, kInnerW), colors::white);
            std::string line = skillLine(s, false);
            const std::string bonus = bonusText(s);
            if (!bonus.empty()) line += "  " + bonus;
            text.drawShadow(line.c_str(), x + 6, sy + 16, fitScale(text, line, 0.38f, kInnerW), colors::cyanLight);
        }
    }

    if (frame_ - toastFrame_ < kToastFrames && !toast_.empty()) {
        text.drawCentered(toast_.c_str(), 200, 231, 0.42f, colors::pinkLight);
    } else {
        const char* hint = page_ == Page::Roster ? "Tap a character to edit. Done saves your party."
                         : page_ == Page::Character ? "Tap a skill to edit it, or Rename the character."
                                                    : "Use - and + for numbers, the arrows to pick an option.";
        text.drawCentered(hint, 200, 231, 0.4f, colors::grey);
    }
}

void CustomizeScreen::drawBottom(TextRenderer& text) const {
    if (!assets::drawBottomBg(kZBack)) {
        const u32 top = C2D_Color32(16, 44, 64, 255);
        const u32 bot = C2D_Color32(7, 19, 33, 255);
        C2D_DrawRectangle(0, 0, kZBack, kBottomWidth, kScreenHeight, top, top, bot, bot);
    }
    switch (page_) {
    case Page::Roster:    drawRoster(text);    break;
    case Page::Character: drawCharacter(text); break;
    case Page::Skill:     drawSkill(text);     break;
    }
    buttons_.draw(text);
}

void CustomizeScreen::drawRoster(TextRenderer& text) const {
    text.drawCentered("Customize Party", 160, 16, 0.55f, colors::white);
    for (int i = 0; i < kRosterSize; ++i) {
        const CharCfg& c = cfg_.chars[i];
        const float x = tileX(i);
        const float y = tileY(i) + (buttons_.pressed(BtnTile0 + i) ? 2.f : 0.f);
        const float cx = x + kTileW * 0.5f;
        const bool modified = isModified(cfg_, i);

        drawRoundCard(x, y, kTileW, kTileH, cardFill(modified, buttons_.pressed(BtnTile0 + i)), cardBorder(modified));
        drawRing(cx, y + 27, 22.f, modified ? colors::pink : kRingIdle, kZFill, false);  
        drawIconFor(text, c, cx, y + 27, 40.f);
        text.drawCentered(c.name.c_str(), cx, y + 58, fitScale(text, c.name, 0.44f, 86.f), colors::white);
        text.drawCentered(c.role.c_str(), cx, y + 71, 0.34f, modified ? colors::pinkLight : colors::grey);
    }
}

void CustomizeScreen::drawCharacter(TextRenderer& text) const {
    const CharCfg& c = cfg_.chars[ch_];
    drawRing(34, 30, 24.f, colors::pink, kZFill, false);
    drawIconFor(text, c, 34, 30, 44.f);
    text.drawShadow(c.name.c_str(), 66, 12, fitScale(text, c.name, 0.6f, 146.f), colors::white);
    text.drawShadow(c.role.c_str(), 66, 34, 0.4f, colors::pinkLight);

    for (int j = 0; j < 2; ++j) {
        const SkillCfg& s = c.skills[j];
        const bool down = buttons_.pressed(BtnSkill0 + j);
        const float off = down ? 2.f : 0.f;
        const float y = 64.f + j * 48.f;
        const bool modifiedSkill = isModified(cfg_, ch_, j);
        drawPill(10, y + off, 300, 42 - off, cardFill(modifiedSkill, down), cardBorder(modifiedSkill), kZPanel);
        text.drawShadow(s.name.c_str(), 26, y + 5 + off, fitScale(text, s.name, 0.5f, 180.f), colors::white);
        char buf[16];
        std::snprintf(buf, sizeof buf, "-%d SP", s.sp_cost);
        text.drawRight(buf, 296, y + 7 + off, 0.42f, colors::pinkLight);
        const std::string line = skillLine(s);
        const std::string bonus = bonusText(s);
        text.drawShadow(line.c_str(), 26, y + 24 + off, fitScale(text, line, 0.4f, bonus.empty() ? 260.f : 190.f),
                        colors::cyanLight);
        if (!bonus.empty()) text.drawRight(bonus.c_str(), 296, y + 24 + off, 0.4f, colors::yellow);
    }

    drawPill(8, kClassRowY, 304, kRowH, cardFill(false), cardBorder(false), kZPanel - 0.02f);
    drawRoundButton(kBtnCxL, kClassRowY + kRowH * 0.5f, true, buttons_.pressed(BtnClassMinus), Glyph::Left);
    drawRoundButton(kBtnCxR, kClassRowY + kRowH * 0.5f, true, buttons_.pressed(BtnClassPlus), Glyph::Right);
    text.drawShadow("Class", 18, kClassRowY + 3, 0.4f, colors::white);
    text.drawCentered(c.role.c_str(), 230, kClassRowY + kRowH * 0.5f, 0.42f, colors::yellow);
}

void CustomizeScreen::drawSkill(TextRenderer& text) const {
    const CharCfg& c = cfg_.chars[ch_];
    const SkillCfg& s = c.skills[sk_];

    text.drawShadow(s.name.c_str(), 12, 6, fitScale(text, s.name, 0.55f, 210.f), colors::white);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s  -  skill %d", c.name.c_str(), sk_ + 1);
    text.drawShadow(buf, 12, 26, 0.4f, colors::grey);

    int k = 0;
    for (int f = 0; f < kSkillFieldCount; ++f) {
        const SkillField field = static_cast<SkillField>(f);
        if (!fieldVisible(s, field)) continue;
        const float y = rowY(k++);
        drawPill(8, y, 304, kRowH, cardFill(false), cardBorder(false), kZPanel - 0.02f);
        // Lingkaran dulu, sebelum teks. Angka: - dan +. Pilihan (Bonus, Effect, Target): panah.
        const bool choice = isChoiceField(field);
        drawRoundButton(kBtnCxL, y + kRowH * 0.5f, canStep(s, field, -1), buttons_.pressed(BtnMinus0 + f),
                        choice ? Glyph::Left : Glyph::Minus);
        drawRoundButton(kBtnCxR, y + kRowH * 0.5f, canStep(s, field, +1), buttons_.pressed(BtnPlus0 + f),
                        choice ? Glyph::Right : Glyph::Plus);
        text.drawShadow(fieldLabel(s, field), 18, y + 3, 0.4f, colors::white);
        text.drawCentered(fieldText(s, field).c_str(), 230, y + kRowH * 0.5f, 0.42f, colors::yellow);
    }
}
