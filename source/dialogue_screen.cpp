#include <cmath>
#include <cstdio>

#include "assets.h"
#include "battle3d.h"
#include "screens.h"

namespace {

enum ButtonId {
    BtnLog = 1, BtnSkip, BtnHide,                   // deretan tombol saat dialog berjalan
    BtnDeadZone, BtnTapArea,                        // zona mati di sekitar deretan tombol, dan area ketuk = lanjut
    BtnLogUp = 10, BtnLogDown, BtnLogClose          // tampilan Log
};

const int   kLogSlots         = 4;     // jumlah baris dialog yang tampil sekaligus di Log
const float kLogSlotHeight    = 40.f;

const u32 kBand   = C2D_Color32( 18,  18,  22, 178);
const float kBandY = 164.f;

const u32 kRingIdle  = C2D_Color32(255, 255, 255, 150);
const float kZCircle = 0.32f;                                  // blok lingkaran: di atas panel, di bawah ikon

// Tata letak layar bawah (320x240)
// Deretan tiga lingkaran polos (tanpa label) di kiri bawah, dijangkau jempol kiri.
// Titik awal dan garis tengahnya sejajar tombol Swap di layar battle (cx=40, cy=209).
const float kIconR = 22.f, kIconCy = 209.f;
float iconCx(int i) { return 40.f + i * 58.f; }                 // LOG, SKIP, HIDE
// Zona mati di sekeliling deretan: ketukan yang meleset ke sela tombol tidak dianggap "lanjut".
const float kGroupX = 10.f, kGroupY = 179.f, kGroupW = 176.f, kGroupH = 61.f;
const int   kHintLines = 2;    // petunjuk "ketuk layar untuk lanjut" hanya tampil di dua baris pertama

void quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, u32 c,
          float z) {
    C2D_DrawTriangle(x0, y0, c, x1, y1, c, x2, y2, c, z);
    C2D_DrawTriangle(x0, y0, c, x2, y2, c, x3, y3, c, z);
}

void iconBook(float cx, float cy, float k, u32 pageLine) {
    quad(cx - 13 * k, cy - 9 * k, cx - 1 * k, cy - 6 * k, cx - 1 * k, cy + 9 * k, cx - 13 * k, cy + 6 * k,
         colors::white, kZIcon);
    quad(cx + 1 * k, cy - 6 * k, cx + 13 * k, cy - 9 * k, cx + 13 * k, cy + 6 * k, cx + 1 * k, cy + 9 * k,
         colors::white, kZIcon);
    for (int i = 0; i < 2; ++i) {
        drawLine(cx - 10 * k, cy + (-3 + i * 5) * k, cx - 4 * k, cy + (-2 + i * 5) * k, 1.4f * k,
                 pageLine, kZIcon + 0.01f);
        drawLine(cx + 4 * k, cy + (-2 + i * 5) * k, cx + 10 * k, cy + (-3 + i * 5) * k, 1.4f * k,
                 pageLine, kZIcon + 0.01f);
    }
}

void iconSkip(float cx, float cy, float k) {
    // Kotak pembatas dipusatkan tepat di cx, lalu digeser sedikit ke kanan: segitiga yang runcing ke
    // kanan terlihat lebih "berat" di kiri, jadi tanpa koreksi ini ikon tampak miring ke kiri.
    const float w = 10.f * k, h = 9.f * k, nudge = 0.8f;
    for (int i = 0; i < 2; ++i) {
        const float x = cx - w + i * w + nudge;
        C2D_DrawTriangle(x, cy - h, colors::white, x, cy + h, colors::white, x + w, cy,
                         colors::white, kZIcon);
    }
}

// Poligon bulat dari kipas segitiga. Sengaja bukan C2D_DrawCircle: lingkaran memakai mode GPU
// terpisah, sedangkan ikon harus tetap di lapisan 3 (segitiga dan teks saja).
void polyFan(float cx, float cy, float r, u32 c, float z) {
    const int n = 16;
    for (int i = 0; i < n; ++i) {
        const float a0 = 6.2831853f * i / n, a1 = 6.2831853f * (i + 1) / n;
        C2D_DrawTriangle(cx, cy, c, cx + r * std::cos(a0), cy + r * std::sin(a0), c,
                         cx + r * std::cos(a1), cy + r * std::sin(a1), c, z);
    }
}

// Mata almond: putih, iris merah muda, pupil gelap. Dicoret diagonal saat kotak dialog
// disembunyikan (label SHOW).
void iconEye(float cx, float cy, float k, u32 iris, u32 pupil, bool slashed) {
    const float hw = 14.f * k, hh = 8.5f * k;
    const int n = 14;
    for (int i = 0; i < n; ++i) {
        const float t0 = -1.f + 2.f * i / n, t1 = -1.f + 2.f * (i + 1) / n;
        const float h0 = hh * std::cos(1.5707963f * t0), h1 = hh * std::cos(1.5707963f * t1);
        quad(cx + hw * t0, cy - h0, cx + hw * t1, cy - h1, cx + hw * t1, cy + h1, cx + hw * t0, cy + h0,
             colors::white, kZIcon);
    }
    polyFan(cx, cy, 5.2f * k, iris, kZIcon + 0.01f);
    polyFan(cx, cy, 2.2f * k, pupil, kZIcon + 0.02f);
    if (slashed) {
        drawLine(cx - 12 * k, cy + 9 * k, cx + 12 * k, cy - 9 * k, 5.f * k, pupil, kZIcon + 0.03f);
        drawLine(cx - 12 * k, cy + 9 * k, cx + 12 * k, cy - 9 * k, 2.4f * k, colors::white,
                 kZIcon + 0.04f);
    }
}

void iconArrow(float cx, float cy, bool up, u32 color) {
    if (up)
        C2D_DrawTriangle(cx - 10, cy + 7, color, cx + 10, cy + 7, color, cx, cy - 8, color, kZIcon);
    else
        C2D_DrawTriangle(cx - 10, cy - 7, color, cx + 10, cy - 7, color, cx, cy + 8, color, kZIcon);
}

}  // namespace

void DialogueScreen::enter() {
    scene_.load(makePrologueScript());
    buttons_.reset();
    mode_ = Mode::Normal;
    windowHidden_ = false;
    logTop_ = 0;
    frame_ = 0;
}

void DialogueScreen::openLog() {
    mode_ = Mode::Log;
    const std::size_t n = scene_.historyCount();
    logTop_ = (n > static_cast<std::size_t>(kLogSlots)) ? n - kLogSlots : 0;   // mulai dari yang terbaru
}

void DialogueScreen::update(const TouchState& touch, u32 keysDown) {
    ++frame_;

    if (mode_ == Mode::Normal) {
        scene_.update();
    }

    buttons_.begin();
    if (mode_ == Mode::Normal) {
        // Area sentuh lingkaran/pil (warna 0): tombolnya digambar sendiri di drawTray().
        buttons_.add(BtnLog,  iconCx(0) - kIconR, kIconCy - kIconR, kIconR * 2, kIconR * 2, "",
                     true, 0, 0.6f, true);
        buttons_.add(BtnSkip, iconCx(1) - kIconR, kIconCy - kIconR, kIconR * 2, kIconR * 2, "",
                     true, 0, 0.6f, true);
        buttons_.add(BtnHide, iconCx(2) - kIconR, kIconCy - kIconR, kIconR * 2, kIconR * 2, "",
                     true, 0, 0.6f, true);
        // Urutan penting: hitId() memilih tombol pertama yang cocok. Zona mati dulu, baru area ketuk.
        buttons_.add(BtnDeadZone, kGroupX, kGroupY, kGroupW, kGroupH, "", true, 0);
        buttons_.add(BtnTapArea, 0, 0, kBottomWidth, kScreenHeight, "", true, 0);
    } else {
        const std::size_t n = scene_.historyCount();
        buttons_.add(BtnLogUp,   262, 20, 52, 52, "", logTop_ > 0, 0, 0.6f, true);
        buttons_.add(BtnLogDown, 262, 110, 52, 52, "", logTop_ + kLogSlots < n, 0, 0.6f, true);
        buttons_.add(BtnLogClose, 8, 184, 304, 44, "", true, 0);
    }

    const int hit = buttons_.update(touch);
    auto advance = [this]() {
        if (windowHidden_) windowHidden_ = false;            // ketuk pertama: tampilkan kembali kotaknya
        else               scene_.advance();
    };

    if (mode_ == Mode::Normal) {
        if (keysDown & KEY_A) advance();
        if (keysDown & KEY_B) windowHidden_ = !windowHidden_;
        if (keysDown & KEY_X) scene_.skipAll();
        if (keysDown & KEY_Y) openLog();
    } else {
        const bool up   = (keysDown & (KEY_DUP   | KEY_CPAD_UP))   != 0;
        const bool downK = (keysDown & (KEY_DDOWN | KEY_CPAD_DOWN)) != 0;
        if (up && logTop_ > 0) --logTop_;
        if (downK) ++logTop_;
        if (keysDown & (KEY_B | KEY_A)) mode_ = Mode::Normal;
    }

    switch (hit) {
    case BtnLog:      openLog(); break;
    case BtnHide:     windowHidden_ = !windowHidden_; break;
    case BtnSkip:     scene_.skipAll(); break;
    case BtnTapArea:  advance(); break;
    case BtnLogUp:    if (logTop_ > 0) --logTop_; break;
    case BtnLogDown:  ++logTop_; break;
    case BtnLogClose: mode_ = Mode::Normal; break;
    default: break;
    }
}

bool DialogueScreen::drawScene3d(C3D_RenderTarget* target) const {
    battle3d::SceneState state;
    state.camera = battle3d::Camera::Prologue;
    state.frame = frame_;
    return battle3d::renderTop(target, state);
}

void DialogueScreen::drawTop(TextRenderer& text) const {
    const DialogueLine* line = scene_.current();
    if (!line || windowHidden_) return;

    C2D_DrawRectSolid(0, kBandY, kZPanel, kTopWidth, kScreenHeight - kBandY, kBand);

    const float tagH = 17.f, tagX = 24.f;
    const float tagW = text.width(line->speaker.c_str(), 0.4f) + 18.f;
    C2D_DrawRectSolid(tagX, kBandY - tagH, kZPanel, tagW, tagH, kBand);
    text.draw(line->speaker.c_str(), tagX + 9, kBandY - tagH + 2, 0.4f, colors::white);

    const std::string visible = scene_.visibleText();
    text.draw(visible.c_str(), 26, kBandY + 10, 0.55f, colors::white, 350.f);

    if (scene_.lineComplete() && ((frame_ / 30) % 2 == 0)) {
        C2D_DrawTriangle(378, 224, colors::pink, 390, 224, colors::pink,
                         384, 232, colors::pink, kZText);
    }
}

// Layar bawah: deretan tiga lingkaran polos (LOG, SKIP, HIDE) di kiri bawah, disambung garis tipis.
// Tidak ada tombol lanjut: seluruh area kosong di luar deretan adalah "lanjut"
// (plus tombol A). Petunjuk singkat muncul di dua baris pertama saja.
void DialogueScreen::drawTray(TextRenderer& text) const {
    struct Tile { int id; bool active; };
    const Tile tiles[3] = {
        { BtnLog,  false },
        { BtnSkip, false },
        { BtnHide, windowHidden_ },
    };

    for (int i = 0; i < 2; ++i)
        drawLine(iconCx(i) + kIconR - 2, kIconCy, iconCx(i + 1) - kIconR + 2, kIconCy, 1.2f,
                 kRingIdle, kZPanel);

    for (int i = 0; i < 3; ++i) {
        const u32 ring = tiles[i].active ? colors::pink : kRingIdle;
        drawRing(iconCx(i), kIconCy, kIconR, ring, kZCircle, tiles[i].active);
        if (buttons_.pressed(tiles[i].id))
            drawDisc(iconCx(i), kIconCy, kIconR - 6.f, C2D_Color32(255, 255, 255, 40), kZCircle + 0.01f);
    }

    const float k = 1.1f;
    const u32 paper = mixColor(colors::disc, colors::pink, 0.3f);
    iconBook(iconCx(0), kIconCy, k, paper);
    iconSkip(iconCx(1), kIconCy, k);
    iconEye(iconCx(2), kIconCy, k, colors::pink, colors::disc, windowHidden_);

    // Petunjuk sementara untuk pemain baru; hilang setelah dua baris dialog dan tidak muncul lagi.
    if (scene_.current() && scene_.index() < static_cast<std::size_t>(kHintLines))
        text.drawCentered("Tap the screen to continue", 160, 92, 0.5f, colors::grey);
}

void DialogueScreen::drawLog(TextRenderer& text) const {
    const std::size_t n = scene_.historyCount();

    drawFlat(8, 8, 248, 168, 10.f, mixColor(colors::bg, colors::pink, 0.08f), kZPanel);
    for (int i = 0; i < kLogSlots; ++i) {
        const std::size_t idx = logTop_ + i;
        if (idx >= n) break;
        const float y = 14.f + i * kLogSlotHeight;
        text.drawShadow(scene_.lineAt(idx).speaker.c_str(), 18, y, 0.38f, colors::pinkLight);
        const std::string body = scene_.logText(idx);
        text.drawShadow(body.c_str(), 18, y + 13, 0.38f, colors::white, 228.f);
    }

    const bool canUp = logTop_ > 0;
    const bool canDown = logTop_ + kLogSlots < n;
    const float acx = 288.f;
    drawRing(acx, 46, 26.f, canUp ? colors::pink : kRingIdle, kZCircle, false);
    drawRing(acx, 136, 26.f, canDown ? colors::pink : kRingIdle, kZCircle, false);

    iconArrow(acx, 46, true, canUp ? colors::white : colors::grey);
    iconArrow(acx, 136, false, canDown ? colors::white : colors::grey);

    drawPill(8, 184, 304, 44,
             mixColor(colors::bg, colors::pink, buttons_.pressed(BtnLogClose) ? 0.4f : 0.22f),
             colors::pink, kZFill);
    text.drawCentered("Close Log", 160, 206, 0.6f, colors::white);
}

void DialogueScreen::drawBottom(TextRenderer& text) const {
    if (!assets::drawBottomBg(kZBack)) {
        const u32 top = C2D_Color32(16, 44, 64, 255);
        const u32 bot = C2D_Color32(7, 19, 33, 255);
        C2D_DrawRectangle(0, 0, kZBack, kBottomWidth, kScreenHeight, top, top, bot, bot);
    }
    if (mode_ == Mode::Log) drawLog(text);
    else                    drawTray(text);
}
