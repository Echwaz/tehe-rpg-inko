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
    BtnPlus0  = 40                   // 40 + SkillField
};

// Halaman Roster: 3 kolom x 2 baris kotak karakter.
const float kTileW = 96.f, kTileH = 80.f;
float tileX(int i) { return 10.f + (i % 3) * 104.f; }
float tileY(int i) { return 34.f + (i / 3) * 88.f; }

// Halaman Roster dan Character: tombol bawah.
const float kFootY = 206.f, kFootH = 28.f;
// Halaman Skill: baris nilai dan tombol bawah.
const float kRowY0 = 40.f, kRowStep = 24.f, kRowH = 22.f;
const float kSkillFootY = 210.f, kSkillFootH = 26.f;
const int   kToastFrames = 150;

float rowY(int visibleIndex) { return kRowY0 + visibleIndex * kRowStep; }

// Kecilkan skala teks supaya lebarnya tidak melebihi maxW (nama kustom bisa panjang atau lebar).
float fitScale(TextRenderer& text, const std::string& s, float base, float maxW) {
    const float w = text.width(s.c_str(), base);
    return w > maxW ? std::max(0.25f, base * maxW / w) : base;
}

std::string fxText(const SkillCfg& s) {
    SkillEffect fx;
    fx.type  = s.fx_type;
    fx.value = s.fx_value;
    fx.turns = s.fx_turns;
    fx.scope = s.fx_scope;
    return effectLabel(fx);
}

// Ringkasan satu baris, mis. "9 hits x 6  ATK+40%(1t) team".
std::string skillLine(const SkillCfg& s) {
    char buf[48];
    std::string out;
    if (s.kind == SkillKind::Attack) {
        std::snprintf(buf, sizeof buf, "%d hit%s x %d", s.hits, s.hits == 1 ? "" : "s", s.power);
        out = buf;
    } else if (s.kind == SkillKind::HealDP) {
        std::snprintf(buf, sizeof buf, "Heal %d DP", s.power);
        out = buf;
    }
    const std::string fx = fxText(s);
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
            buttons_.add(BtnMinus0 + f, 160, rowY(k), 32, kRowH, "-", canStep(s, field, -1), colors::pink, 0.6f);
            buttons_.add(BtnPlus0 + f, 268, rowY(k), 32, kRowH, "+", canStep(s, field, +1), colors::pink, 0.6f);
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
        const float x = 6.f + (i % 3) * 132.f, y = 26.f + (i / 3) * 100.f;
        const bool selected = page_ != Page::Roster && i == ch_;
        drawPanel(x, y, 128, 96, mixColor(colors::bg, colors::pink, selected ? 0.25f : 0.08f),
                  selected ? colors::pink : (isModified(cfg_, i) ? colors::pinkLight : colors::cyan));

        text.drawShadow(c.name.c_str(), x + 8, y + 5, fitScale(text, c.name, 0.5f, 112.f), colors::white);
        text.drawShadow(c.role.c_str(), x + 8, y + 22, 0.3f, colors::grey);
        for (int j = 0; j < 2; ++j) {
            const SkillCfg& s = c.skills[j];
            const float sy = y + 38.f + j * 29.f;
            text.drawShadow(s.name.c_str(), x + 8, sy, fitScale(text, s.name, 0.36f, 112.f), colors::white);
            const std::string line = skillLine(s);
            text.drawShadow(line.c_str(), x + 8, sy + 13, fitScale(text, line, 0.28f, 112.f), colors::cyanLight);
        }
    }

    if (frame_ - toastFrame_ < kToastFrames && !toast_.empty()) {
        text.drawCentered(toast_.c_str(), 200, 230, 0.36f, colors::pinkLight);
    } else {
        const char* hint = page_ == Page::Roster ? "Tap a character to edit. Done saves your party."
                         : page_ == Page::Character ? "Tap a skill to edit it, or Rename the character."
                                                    : "Use - and + to change values. B goes back.";
        text.drawCentered(hint, 200, 230, 0.32f, colors::grey);
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

        drawPanel(x, y, kTileW, kTileH,
                  mixColor(colors::bg, colors::pink, buttons_.pressed(BtnTile0 + i) ? 0.35f : 0.14f),
                  modified ? colors::pink : colors::cyan);
        text.drawShadow(i < 3 ? "FRONT" : "BACK", x + 8, y + 4, 0.26f, colors::grey);
        drawIconFor(text, c, cx, y + 27, 40.f);
        text.drawCentered(c.name.c_str(), cx, y + 56, fitScale(text, c.name, 0.42f, 86.f), colors::white);
        text.drawCentered(c.role.c_str(), cx, y + 70, 0.28f, modified ? colors::pinkLight : colors::grey);
    }
}

void CustomizeScreen::drawCharacter(TextRenderer& text) const {
    const CharCfg& c = cfg_.chars[ch_];
    drawIconFor(text, c, 34, 30, 44.f);
    text.drawShadow(c.name.c_str(), 66, 12, fitScale(text, c.name, 0.6f, 146.f), colors::white);
    text.drawShadow(c.role.c_str(), 66, 34, 0.34f, colors::pinkLight);

    for (int j = 0; j < 2; ++j) {
        const SkillCfg& s = c.skills[j];
        const bool down = buttons_.pressed(BtnSkill0 + j);
        const float off = down ? 2.f : 0.f;
        const float y = 64.f + j * 48.f;
        drawPill(10, y + off, 300, 42 - off,
                 mixColor(colors::bg, colors::pink, down ? 0.35f : 0.14f),
                 isModified(cfg_, ch_, j) ? colors::pink : colors::cyan, kZPanel);
        text.drawShadow(s.name.c_str(), 26, y + 5 + off, fitScale(text, s.name, 0.5f, 180.f), colors::white);
        char buf[16];
        std::snprintf(buf, sizeof buf, "-%d SP", s.sp_cost);
        text.drawRight(buf, 296, y + 7 + off, 0.42f, colors::pinkLight);
        const std::string line = skillLine(s);
        text.drawShadow(line.c_str(), 26, y + 24 + off, fitScale(text, line, 0.34f, 260.f), colors::cyanLight);
    }
    text.drawCentered("Tap a skill to edit it", 160, 172, 0.34f, colors::grey);
}

void CustomizeScreen::drawSkill(TextRenderer& text) const {
    const CharCfg& c = cfg_.chars[ch_];
    const SkillCfg& s = c.skills[sk_];

    text.drawShadow(s.name.c_str(), 12, 6, fitScale(text, s.name, 0.55f, 210.f), colors::white);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s  -  skill %d", c.name.c_str(), sk_ + 1);
    text.drawShadow(buf, 12, 26, 0.32f, colors::grey);

    int k = 0;
    for (int f = 0; f < kSkillFieldCount; ++f) {
        const SkillField field = static_cast<SkillField>(f);
        if (!fieldVisible(s, field)) continue;
        const float y = rowY(k++);
        drawPill(8, y, 304, kRowH, mixColor(colors::bg, colors::cyan, 0.10f),
                 mixColor(colors::bg, colors::cyan, 0.35f), kZPanel - 0.02f);
        text.drawShadow(fieldLabel(s, field), 18, y + 4, 0.4f, colors::white);
        text.drawCentered(fieldText(s, field).c_str(), 230, y + kRowH * 0.5f, 0.42f, colors::yellow);
    }
}
