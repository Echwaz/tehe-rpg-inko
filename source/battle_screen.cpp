#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "assets.h"
#include "battle3d.h"
#include "circle_points.h"
#include "screens.h"
#include "sfx.h"

namespace {

enum ButtonId {
    BtnSlot0     = 10,     // 10..12 : pilih slot
    BtnSwapMode  = 20,
    BtnExecute   = 30,
    BtnOverdrive = 35,
    BtnAttack    = 40,     // 40 = Attack, 41..42 = skill 0..1
    BtnSkill0    = 41,
    BtnConfirm   = 50,
    BtnRestart   = 60
};

const u32 kDimTint     = C2D_Color32(  0,   0,   0, 255);
const u32 kRingIdle    = C2D_Color32(255, 255, 255, 150);
const u32 kTrack       = C2D_Color32(  0,   0,   0, 130);
const u32 kLineWhite   = C2D_Color32(255, 255, 255, 170);

// Lapisan (z) untuk blok lingkaran: di atas panel, di bawah ikon (kZSprite = 0.38).
const float kZCircle = 0.32f;

// Penanda kursor tombol fisik: kurung siku pink di 4 sudut kotak (bukan outline penuh).
void drawCursorBrackets(float x, float y, float w, float h, u32 color, float z, float len = 8.f) {
    const float t = 2.2f;
    drawLine(x, y, x + len, y, t, color, z);
    drawLine(x, y, x, y + len, t, color, z);
    drawLine(x + w - len, y, x + w, y, t, color, z);
    drawLine(x + w, y, x + w, y + len, t, color, z);
    drawLine(x, y + h - len, x, y + h, t, color, z);
    drawLine(x, y + h, x + len, y + h, t, color, z);
    drawLine(x + w, y + h - len, x + w, y + h, t, color, z);
    drawLine(x + w - len, y + h, x + w, y + h, t, color, z);
}

// Deteksi tekan-tahan / ketuk-dua-kali pada baris skill untuk membuka layar penjelasan.
const int kLongPressFrames = 40;   // ~0.66 detik pada 60 fps
const int kDoubleTapFrames = 24;   // ~0.4 detik pada 60 fps

// Tata letak layar bawah (320x240)
const float kSlotR = 24.f, kSlotCy = 33.f;
float slotCx(int i) { return 60.f + i * 100.f; }
const float kListX = 12.f, kListW = 296.f, kRowH = 32.f;
float rowY(int k) { return 66.f + k * 35.f; }
const float kSwapCx = 40.f, kSwapCy = 209.f, kSwapR = 27.f;
const float kExecCx = 280.f, kExecCy = 209.f, kExecR = 29.f;
const float kOdCx = 160.f, kOdCy = 209.f, kOdR = 25.f;

// Warna per stage OD (1/2/3 bar), sama dipakai untuk tombol dan gauge di layar atas.
const u32 kOdLevel2Color = C2D_Color32( 45, 212, 191, 255);   // teal (bukan colors::cyan yang lebih biru)
const u32 kOdLevel3Color = C2D_Color32(198, 237,  99, 255);
u32 odStageColor(int bars) {
    if (bars >= 3) return kOdLevel3Color;
    if (bars == 2) return kOdLevel2Color;
    return colors::pink;
}

// Sama seperti drawBarGrad (ui.h), tapi mengisi dari KANAN ke KIRI: anchor tetap di tepi kanan
// track, isian tumbuh ke kiri. 'tip' = warna di ujung isian yang sedang tumbuh (kiri, terang),
// 'base' = warna di tepi kanan yang diam (redup).
void drawBarGradRTL(float x, float y, float w, float h, int cur, int max, u32 tip, u32 base,
                    u32 track) {
    C2D_DrawRectSolid(x, y, kZBar, w, h, track);
    if (max > 0 && cur > 0) {
        float r = static_cast<float>(cur) / static_cast<float>(max);
        if (r > 1.f) r = 1.f;
        const float fw = w * r;
        C2D_DrawRectangle(x + w - fw, y, kZBar + 0.02f, fw, h, tip, base, tip, base);
    }
}

// Progres bar Overdrive yang sedang diisi (menuju bar berikutnya, bukan yang sudah siap).
int odBarProgress(const Battle& battle) {
    const int bars = battle.odBarsReady();
    if (bars >= kOdMaxBars) return kOdHitsPerBar;
    return std::min(kOdHitsPerBar, std::max(0, battle.odGauge() - bars * kOdHitsPerBar));
}

// Badge lingkaran neon overdrive (dipakai di gauge layar atas DAN tombol layar bawah, supaya
// keduanya bergaya sama): cincin (pijar kalau 'glow'), isi kaca, segitiga terbalik bersudut
// tumpul di dalamnya dengan titik di tengah tiap sisi (atas, kiri-bawah, kanan-bawah) yang
// menyala satu per satu sesuai 'bars', lalu angka 'bars' di tengah (disembunyikan kalau 0).
void drawOdBadge(TextRenderer& text, float cx, float cy, float r, u32 ringColor, u32 fill,
                 int bars, bool glow, float textScale, float z = kZCircle) {
    drawRing(cx, cy, r, ringColor, z, glow);
    drawDisc(cx, cy, r - 3.f, fill, z + 0.01f);

    const float tx = r * 0.61f, topY = cy - r * 0.35f, apexY = cy + r * 0.565f;
    const float lx = cx - tx, rx = cx + tx;
    const u32 triFill = (ringColor & 0x00FFFFFFu) | (46u << 24);
    C2D_DrawTriangle(lx, topY, triFill, rx, topY, triFill, cx, apexY, triFill, z + 0.015f);

    const float sw = std::max(2.f, r * 0.11f);
    drawLine(lx, topY, rx, topY, sw, ringColor, z + 0.02f);
    drawLine(lx, topY, cx, apexY, sw, ringColor, z + 0.02f);
    drawLine(rx, topY, cx, apexY, sw, ringColor, z + 0.02f);
    // Cakram kecil di tiap sudut supaya sambungan garisnya membundar, bukan lancip.
    const float capR = sw * 0.5f;
    drawDisc(lx, topY, capR, ringColor, z + 0.021f);
    drawDisc(rx, topY, capR, ringColor, z + 0.021f);
    drawDisc(cx, apexY, capR, ringColor, z + 0.021f);

    const float dotR = r * 0.12f;
    const u32 dimDot = mixColor(fill, ringColor, 0.5f);
    drawDisc(cx, topY, dotR, bars >= 1 ? ringColor : dimDot, z + 0.03f);
    drawDisc((lx + cx) * 0.5f, (topY + apexY) * 0.5f, dotR, bars >= 2 ? ringColor : dimDot,
             z + 0.03f);
    drawDisc((rx + cx) * 0.5f, (topY + apexY) * 0.5f, dotR, bars >= 3 ? ringColor : dimDot,
             z + 0.03f);

    if (bars > 0) {
        char buf[4];
        std::snprintf(buf, sizeof buf, "%d", bars);
        text.drawCentered(buf, cx, cy, textScale, colors::white);
    }
}

// Geometri layar swap (pojok kiri atas kotak sentuh 52x52)
const float kSwapTile = 52.f;
float swapX(int id) { return 53.f + (id % 3) * 81.f; }
float swapY(int id) { return id < 3 ? 22.f : 108.f; }
const float kZDrag = 0.70f;      // ikon yang diseret: di atas semua teks (kZText = 0.60)

void drawBottomBackdrop() {
    if (assets::drawBottomBg(kZBack)) return;
    const u32 top = C2D_Color32(16, 44, 64, 255);
    const u32 bot = C2D_Color32(7, 19, 33, 255);
    C2D_DrawRectangle(0, 0, kZBack, kBottomWidth, kScreenHeight, top, top, bot, bot);
}

// Campuran 50% dua warna (untuk meniru tint pada gambar cadangan).
u32 mix50(u32 a, u32 b) { return mixColor(a, b, 0.5f); }

// Ikon bulat berpusat di (cx, cy) dengan diameter d. Ikon harus berupa gambar yang sudah
// dipotong bulat (sudut transparan). Tanpa gambar: lingkaran berwarna dengan huruf depan nama.
void drawCharIcon(TextRenderer& text, const Combatant& c, float cx, float cy, float d, float z,
                  u32 tint) {
    if (assets::drawIcon(assets::charFromName(c.key), cx - d * 0.5f, cy - d * 0.5f, d, d, z, tint))
        return;
    drawDisc(cx, cy, d * 0.5f, tint ? mix50(colors::ally, tint) : colors::ally,
             std::min(z, kZText - 0.02f));
    const char initial[2] = { c.key.empty() ? '?' : c.key[0], '\0' };   // huruf pengenal tetap, bukan nama kustom (bisa UTF-8)
    text.drawCentered(initial, cx, cy, d / 60.f, colors::white);
}

const char* roleTag(const char* role) {
    struct { const char* role; const char* tag; } kTags[] = {
        { "Attacker", "ATK" }, { "Breaker", "BRK" },  { "Healer", "HLR" },
        { "Buffer", "BUF" },   { "Debuffer", "DBF" }, { "Blaster", "BLS" },
    };
    for (const auto& t : kTags)
        if (std::strcmp(role, t.role) == 0) return t.tag;
    return "";
}

// Badge status: ikon kotak membulat kecil dari segitiga dan garis, tanpa aset gambar
const float kBadgeR = 7.5f;
const float kZBadge = 0.42f;   // di atas ikon potret (kZSprite = 0.38), di bawah teks (0.60)

// rim = warna garis tepi (kode warna jenis efek), fill = warna isi (selalu gelap seragam).
// scale membesarkan seluruh badge dari titik pusatnya tanpa mengubah kBadgeR global
// (dipakai baris debuff bos supaya lebih besar dari badge lain).
void badgeBase(float cx, float cy, u32 rim, u32 fill, float scale = 1.f) {
    const float s = kBadgeR * 2.f * scale;
    drawRoundRect(cx - s * 0.5f, cy - s * 0.5f, s, s, s * 0.3f, rim, kZBadge);
    const float si = s - 2.2f;
    drawRoundRect(cx - si * 0.5f, cy - si * 0.5f, si, si, si * 0.3f, fill, kZBadge + 0.005f);
}

const u32 kBadgeDark = C2D_Color32(20, 18, 34, 255);

// Setiap simpul (termasuk ujung atas/bawah) diberi disc sebesar setengah tebal garis supaya
// siku antar segmen menyatu mulus (drawLine sendiri memotong rata/tidak membulat di ujungnya,
// jadi tanpa disc ini siku bagian dalam tiap belokan tampak bertakik).
void drawZigzag(float cx, float topY, float amp, float step, int segs, float t, u32 color, float z) {
    float x = cx, y = topY, sign = 1.f;
    drawDisc(x, y, t * 0.5f, color, z);
    for (int i = 0; i < segs; ++i) {
        const float nx = cx + sign * amp, ny = y + step;
        drawLine(x, y, nx, ny, t, color, z);
        drawDisc(nx, ny, t * 0.5f, color, z);
        x = nx;
        y = ny;
        sign = -sign;
    }
}

// ATK+: pedang perak dan panah naik cyan.
void drawBadgeAtkUp(float cx, float cy) {
    badgeBase(cx, cy, colors::pink, kBadgeDark);
    const float z = kZBadge + 0.01f;
    const float hx = cx - 3.6f, hy = cy + 3.4f;
    const float tx = cx + 2.6f, ty = cy - 4.6f;
    drawLine(hx, hy, tx, ty, 1.3f, colors::white, z);

    const float dx = tx - hx, dy = ty - hy, len = std::sqrt(dx * dx + dy * dy);
    const float ux = dx / len, uy = dy / len, px = -uy, py = ux;
    const float gcx = hx + ux * 1.6f, gcy = hy + uy * 1.6f;
    drawLine(gcx - px * 2.f, gcy - py * 2.f, gcx + px * 2.f, gcy + py * 2.f, 1.f, colors::white, z);
    drawLine(hx, hy, hx - ux * 1.2f, hy - uy * 1.2f, 1.4f, colors::grey, z);

    const float ax = cx + 3.4f, ay = cy + 3.f;
    drawLine(ax, ay + 0.6f, ax, ay - 2.8f, 1.1f, colors::cyanLight, z);
    C2D_DrawTriangle(ax - 2.f, ay - 0.6f, colors::cyanLight, ax + 2.f, ay - 0.6f, colors::cyanLight,
                     ax, ay - 3.6f, colors::cyanLight, z);
}

inline float snapPx(float v) { return std::floor(v) + 0.5f; }

// DEF-: perisai perak bergaris silang dan panah turun merah muda.
void drawBadgeDefDown(float cx, float cy, float scale = 1.f) {
    badgeBase(cx, cy, colors::cyan, kBadgeDark, scale);
    const float z = kZBadge + 0.01f;
    const float kShield = 1.3f;
    const float s = scale * kShield;
    const float ox  = snapPx(cx - 2.2f * scale);
    const float hw  = std::round(2.2f * s);
    const float ax0 = ox - hw, ax1 = ox + hw;
    const float ay0 = snapPx(cy - 1.0f * scale - 2.7f * s);
    const float ay1 = ay0 + std::round(2.6f * s);
    const float tipY = ay0 + 5.8f * s;

    drawLine(ax0, ay0, ax1, ay0, 1.f, colors::white, z);
    drawLine(ax1, ay0, ax1, ay1, 1.f, colors::white, z);
    drawLine(ax1, ay1, ox, tipY, 1.f, colors::white, z);
    drawLine(ox, tipY, ax0, ay1, 1.f, colors::white, z);
    drawLine(ax0, ay1, ax0, ay0, 1.f, colors::white, z);

    drawLine(ox, ay0, ox, tipY - 0.5f * s, 1.f, colors::white, z);
    const float midY = snapPx(ay0 + 1.8f * s);
    drawLine(ax0, midY, ax1, midY, 1.f, colors::white, z);

    const float bx = cx + 3.6f * scale, by = cy + 1.4f * scale;
    drawLine(bx, by - 3.2f * scale, bx, by + 0.6f * scale, 1.1f, colors::pinkLight, z);
    C2D_DrawTriangle(bx - 2.f * scale, by + 0.4f * scale, colors::pinkLight,
                     bx + 2.f * scale, by + 0.4f * scale, colors::pinkLight,
                     bx, by + 3.4f * scale, colors::pinkLight, z);
}

void drawBadgeStun(float cx, float cy, float scale = 1.f) {
    badgeBase(cx, cy, colors::broken, kBadgeDark, scale);
    const u32 wave = C2D_Color32(224, 233, 170, 255);
    const float z = kZBadge + 0.01f;
    drawZigzag(cx - 3.6f * scale, cy - 4.6f * scale, 1.7f * scale, 1.8f * scale, 5, 0.9f, wave, z);
    drawZigzag(cx,                cy - 4.6f * scale, 1.7f * scale, 1.8f * scale, 5, 0.9f, wave, z);
    drawZigzag(cx + 3.6f * scale, cy - 4.6f * scale, 1.7f * scale, 1.8f * scale, 5, 0.9f, wave, z);
}

// Jumlah tumpukan dan sisa giliran terpanjang untuk satu jenis efek.
void effectStats(const Combatant& c, EffectType t, int& n, int& turns) {
    n = 0;
    turns = 0;
    for (const StatusEffect& e : c.effects) {
        if (e.type != t) continue;
        ++n;
        turns = std::max(turns, e.turns);
    }
}

struct UnitPos { float x0, cx, cy, r; };
UnitPos unitPos(int i) {
    const float x0 = 6.f + i * 132.f;
    return { x0, x0 + 34.f, 202.f, 27.f };
}

// Bagian teks/bar/ikon satu anggota party (lapisan atas). Cincinnya digambar terpisah di blok lingkaran.
// dpDisp/hpDisp = nilai bar yang sudah dianimasikan (lihat AnimatedBar di ui.h); angka teks tetap
// memakai nilai asli (c.dp) supaya tetap gampang dibaca, cuma bar-nya yang meluncur halus.
void drawPartyUnit(TextRenderer& text, const Combatant& c, int i, float dx, int dpDisp, int hpDisp) {
    const UnitPos u = unitPos(i);
    drawCharIcon(text, c, u.cx + dx, u.cy, (u.r - 2.4f) * 2.f, kZSprite, c.broken ? colors::broken : 0);

    char buf[32];
    // Tag peran dan SP sengaja diletakkan menimpa tepi lingkaran ikon (bukan mengambang jauh
    // di atasnya), supaya keduanya terasa menempel pada karakternya.
    if (c.broken) text.drawShadow("BROKEN", u.x0 + 2, u.cy - 30, 0.34f, colors::broken);
    else          text.drawShadow(roleTag(c.role), u.x0 + 2, u.cy - 30, 0.34f, colors::pinkLight);
    std::snprintf(buf, sizeof buf, "%d", c.sp);
    text.drawOutlined(buf, u.x0 + 46, u.cy - 35, 0.6f, colors::white, colors::pinkDeep, 1.5f);

    const float tx = u.x0 + 72;
    // Nama bisa diganti pemain (Custom Party) dan bisa lebar (kana/kanji): kecilkan supaya tidak
    // menabrak kotak anggota di sebelahnya. Ruang yang tersedia sekitar 60 px.
    float nameScale = 0.42f;
    const float nameW = text.width(c.name.c_str(), nameScale);
    if (nameW > 58.f) nameScale = std::max(0.3f, nameScale * 58.f / nameW);
    text.drawShadow(c.name.c_str(), tx, u.cy - 25, nameScale, colors::white);
    // Angka DP disembunyikan saat 0 (musuh sedang break tidak dianggap "DP 0" secara eksplisit);
    // bar-nya tetap digambar kosong sebagai penanda visual.
    if (c.dp > 0) {
        std::snprintf(buf, sizeof buf, "DP %d", c.dp);
        text.drawShadow(buf, tx, u.cy - 11, 0.36f, colors::cyanLight);
    }
    drawBarGrad(tx, u.cy + 4, 52, 4, dpDisp, c.max_dp, colors::cyan, colors::cyanLight, kTrack);
    drawBarGrad(tx, u.cy + 11, 52, 4, hpDisp, c.max_hp, colors::pink, colors::pinkLight, kTrack);

    // ATK+ tampil sebagai badge di tepi kanan-bawah cincin potret (ikut bergetar bersama potret).
    // Tumpukan (maks kMaxEffectStacks) tampil sebagai beberapa icon berjajar.
    // Badge hilang saat karakter menyerang.
    int atkN, atkTurns;
    effectStats(c, EffectType::AtkUp, atkN, atkTurns);
    if (atkN > 0) {
        float bx = u.cx + dx + 19.f;
        const float by = u.cy + 19.f;
        for (int k = 0; k < atkN; ++k) {
            drawBadgeAtkUp(bx, by);
            bx -= 2.f * kBadgeR + 2.f;   // stack ke KIRI supaya tidak menimpa bar DP/HP di kanan
        }
    }

    const std::string chips = effectSummary(c, effectBit(EffectType::AtkUp));
    if (!chips.empty()) text.drawShadow(chips.c_str(), tx, u.cy + 17, 0.28f, colors::cyanLight);
}

}  // namespace


namespace {

// Titik tumbukan serangan ke bos di layar atas. Posisi cadangan bila proyeksi model 3D tidak tersedia.
const float kBossFxX = 270.f, kBossFxY = 80.f;

const float kOdBadgeR  = 18.f;
const float kOdBadgeCy = 24.f;
const float kOdBadgeCx = kTopWidth - 6.f - kOdBadgeR;
const float kOdBarH = 12.f;
const float kOdBarY = kOdBadgeCy - kOdBarH * 0.5f;
const float kOdBarW = 100.f;
const float kOdBarX = kOdBadgeCx - kOdBadgeR + 8.f - kOdBarW;    // nempel & dikit tertimpa badge

const int   kFxLife       = 60;
const int   kNumberLife   = 38;
const int   kBreakLife    = 40;
const int   kRingLife     = 22;
const int   kScreenFlash  = 10;
const int   kAllyFlash    = 8;
const int   kShakeLife    = 10;
const int   kSlashLife    = 10;
const int   kShotTravel   = 7;
const int   kShotSpark    = 7;
const int   kShotLife     = kShotTravel + kShotSpark;
const float kZFx          = 0.65f; // bentuk efek: di atas semua teks (kZText = 0.60)

// Yuki dan Tsukasa pakai senjata jarak jauh (busur/senapan), jadi hit-nya berupa peluru yang
// (dicek lewat Combatant::key, bukan name, supaya tetap benar setelah pemain mengganti nama)
// melesat dari potret ke bos, bukan goresan di tempat seperti karakter jarak dekat.
bool isRangedAttacker(const std::string& name) { return name == "Yuki" || name == "Tsukasa"; }
u32  rangedColor(const std::string& name) { return name == "Yuki" ? colors::cyanLight : colors::yellow; }

float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
float easeOut(float t) { t = clamp01(t); return 1.f - (1.f - t) * (1.f - t); }

u32 withAlpha(u32 c, float a) {          // ganti kanal alfa warna C2D (ABGR)
    const int v = static_cast<int>(clamp01(a / 255.f) * 255.f);
    return (c & 0x00FFFFFFu) | (static_cast<u32>(v) << 24);
}

// Aura Overdrive: gradasi warna stage dari empat tepi layar atas ke tengah (alfa memudar ke 0),
// berdenyut pelan lewat 'frame'. Hanya persegi bergradasi, tanpa lingkaran, jadi aman untuk
// blok lingkaran citro2d.
void drawOdAura(int frame, u32 color, float z) {
    const float pulse = 0.5f + 0.5f * std::sin(frame * 0.09f);
    const u32 edge  = withAlpha(color, 105.f + 85.f * pulse);
    const u32 clear = withAlpha(color, 0.f);
    const float tw = kTopWidth, th = 240.f;
    const float sideW = 30.f + 6.f * pulse, capH = 22.f + 4.f * pulse;
    C2D_DrawRectangle(0,           0, z, sideW, th, edge, clear, edge, clear);
    C2D_DrawRectangle(tw - sideW,  0, z, sideW, th, clear, edge, clear, edge);
    C2D_DrawRectangle(0,           0, z, tw, capH,  edge, edge, clear, clear);
    C2D_DrawRectangle(0, th - capH,   z, tw, capH,  clear, clear, edge, edge);
}

// Poligon dan cincin dari segitiga (bukan C2D_DrawCircle), supaya blok lingkaran tetap satu
// dan efek ini bisa digambar paling akhir di atas teks.
void fxFan(float cx, float cy, float r, u32 c, float z) {
    const int n = 16;
    const auto* points = circlePoints(n);
    for (int i = 0; i < n; ++i) {
        C2D_DrawTriangle(cx, cy, c, cx + r * points[i].x, cy + r * points[i].y, c,
                         cx + r * points[i + 1].x, cy + r * points[i + 1].y, c, z);
    }
}

void fxRing(float cx, float cy, float r, float thick, u32 c, float z) {
    const int n = 28;
    const auto* points = circlePoints(n);
    const float ro = r + thick * 0.5f, ri = std::max(0.f, r - thick * 0.5f);
    for (int i = 0; i < n; ++i) {
        const float c0 = points[i].x, s0 = points[i].y;
        const float c1 = points[i + 1].x, s1 = points[i + 1].y;
        C2D_DrawTriangle(cx + ro * c0, cy + ro * s0, c, cx + ri * c0, cy + ri * s0, c,
                         cx + ro * c1, cy + ro * s1, c, z);
        C2D_DrawTriangle(cx + ro * c1, cy + ro * s1, c, cx + ri * c0, cy + ri * s0, c,
                         cx + ri * c1, cy + ri * s1, c, z);
    }
}

// Geser horizontal angka supaya hit beruntun ke target yang sama tidak menumpuk di titik yang sama.
float hitJitter(const FxEvent& e, float amp) {
    static const float kJit[5] = { 0.f, -0.9f, 0.8f, -0.4f, 1.f };
    return kJit[(e.delay / kFxHitGap) % 5] * amp;
}
float bossJitter(const FxEvent& e) { return hitJitter(e, 20.f); }

}  // namespace

float BattleScreen::allyShakeX(int slot) const {
    float dx = 0.f;
    for (const FxItem& it : fx_) {
        const FxEvent& e = it.ev;
        if (e.kind != FxKind::Damage || e.on_enemy || e.slot != slot) continue;
        const int age = frame_ - it.start;
        if (age < 0 || age >= kShakeLife) continue;
        const float amp = 3.f * (1.f - static_cast<float>(age) / kShakeLife);
        dx += ((age / 2) % 2 == 0) ? amp : -amp;
    }
    return dx;
}

void BattleScreen::drawFx(TextRenderer& text, bool scene3d) const {
    float bossX = kBossFxX, bossY = kBossFxY;
    if (scene3d) battle3d::enemyAnchor(bossX, bossY);
    if (fx_.empty()) return;

    // Tiap event dibunyikan sekali, tepat di frame pertama ia tampil
    for (const FxItem& it : fx_) {
        const FxEvent& e = it.ev;
        if (frame_ - it.start != 0) continue;
        switch (e.kind) {
        case FxKind::Damage:
            if (e.on_enemy) {
                const bool ranged = e.attacker_slot >= 0 && e.attacker_slot < 3 &&
                    isRangedAttacker(battle_.party().front[e.attacker_slot].key);
                sfx::play(ranged ? sfx::Id::HitRanged : sfx::Id::HitMelee);
            } else {
                sfx::play(sfx::Id::AllyHit);
            }
            break;
        case FxKind::Heal:  sfx::play(sfx::Id::Heal);  break;
        case FxKind::Break: sfx::play(sfx::Id::Break); break;
        case FxKind::Stun:  sfx::play(sfx::Id::Stun);  break;
        }
    }

    for (const FxItem& it : fx_) {
        const FxEvent& e = it.ev;
        const int age = frame_ - it.start;
        if (age < 0 || e.kind == FxKind::Stun) continue;   // Stun tidak punya angka/teks

        float ax, ay;
        if (e.on_enemy) {
            ax = bossX + (e.kind == FxKind::Damage ? bossJitter(e) : 0.f);
            ay = bossY;
        } else {
            const UnitPos u = unitPos(e.slot);
            ax = u.cx + (e.kind == FxKind::Damage ? hitJitter(e, 14.f) : 0.f);
            ay = e.kind == FxKind::Break ? u.cy - 3.f : u.cy - u.r - 8.f;
        }

        char buf[24];
        u32 fill, outline;
        float base, rise;
        int life;
        if (e.kind == FxKind::Damage) {
            std::snprintf(buf, sizeof buf, "%d", e.dp > 0 ? e.dp : e.hp);
            fill = colors::white;
            outline = e.dp > 0 ? colors::cyan : colors::pink;
            base = 0.7f;
            rise = e.on_enemy ? 26.f : 20.f;
            life = kNumberLife;
        } else if (e.kind == FxKind::Heal) {
            std::snprintf(buf, sizeof buf, "+%d", e.dp);
            fill = colors::cyanLight;
            outline = C2D_Color32(20, 90, 110, 255);
            base = 0.7f;
            rise = 20.f;
            life = kNumberLife;
        } else {
            std::snprintf(buf, sizeof buf, "BREAK!");
            fill = colors::broken;
            outline = C2D_Color32(110, 40, 0, 255);
            base = e.on_enemy ? 0.85f : 0.6f;
            rise = e.on_enemy ? 12.f : 0.f;
            if (e.on_enemy) ay -= 28.f;
            life = kBreakLife;
        }
        if (age >= life) continue;

        // Muncul membesar sedikit lalu menyusut ke ukuran normal, melayang naik, lalu memudar.
        float f = 1.f;
        if (age < 4)      f = 0.6f + 0.6f * age / 4.f;
        else if (age < 8) f = 1.2f - 0.2f * (age - 4) / 4.f;
        const float t = static_cast<float>(age) / life;
        const float alpha = (t < 0.6f) ? 255.f : 255.f * (1.f - (t - 0.6f) / 0.4f);

        const float sc = base * f;
        const float w = text.width(buf, sc);
        const float y = ay - rise * easeOut(t) - sc * 15.f;
        const float x = std::max(4.f, std::min(kTopWidth - 4.f - w, ax - w * 0.5f));
        text.drawOutlined(buf, x, y, sc, withAlpha(fill, alpha), withAlpha(outline, alpha), 1.5f);
    }

    for (const FxItem& it : fx_) {
        const FxEvent& e = it.ev;
        const int age = frame_ - it.start;
        if (age < 0) continue;

        const bool ranged = e.attacker_slot >= 0 && e.attacker_slot < 3 &&
            isRangedAttacker(battle_.party().front[e.attacker_slot].key);

        if (e.kind == FxKind::Damage && e.on_enemy && ranged && age < kShotLife) {
            const UnitPos u = unitPos(e.attacker_slot);
            const float tx = bossX + bossJitter(e), ty = bossY;
            const u32 col = rangedColor(battle_.party().front[e.attacker_slot].key);
            if (age < kShotTravel) {
                const float t = easeOut(static_cast<float>(age) / kShotTravel);
                const float tailT = std::max(0.f, t - 0.35f);
                const float hx = u.cx + (tx - u.cx) * t,       hy = u.cy + (ty - u.cy) * t;
                const float sx = u.cx + (tx - u.cx) * tailT,   sy = u.cy + (ty - u.cy) * tailT;
                drawLine(sx, sy, hx, hy, 4.5f, withAlpha(col, 120.f), kZFx);
                drawLine(sx, sy, hx, hy, 1.6f, withAlpha(colors::white, 255.f), kZFx + 0.01f);
            } else {
                const int sparkAge = age - kShotTravel;
                const float t = static_cast<float>(sparkAge) / kShotSpark;
                fxRing(tx, ty, 3.f + 12.f * easeOut(t), 1.6f - 1.f * t,
                       withAlpha(col, 230.f * (1.f - t)), kZFx);
            }
        } else if (e.kind == FxKind::Damage && e.on_enemy && !ranged && age < kSlashLife) {
            const float dir = ((e.delay / kFxHitGap) % 2 == 0) ? 1.f : -1.f;
            const float cx = bossX + bossJitter(e), cy = bossY;
            const float half = 6.f + 26.f * easeOut(age / 4.f);
            const float dx = 0.82f * half, dy = -0.57f * half * dir;
            const float a = age < 3 ? 255.f : 255.f * (1.f - (age - 3) / static_cast<float>(kSlashLife - 3));
            drawLine(cx - dx, cy - dy, cx + dx, cy + dy, 4.f, withAlpha(colors::pink, a * 0.6f), kZFx);
            drawLine(cx - dx, cy - dy, cx + dx, cy + dy, 1.8f, withAlpha(colors::white, a), kZFx + 0.01f);
        } else if (e.kind == FxKind::Damage && !e.on_enemy && age < kAllyFlash) {
            const UnitPos u = unitPos(e.slot);
            fxFan(u.cx + allyShakeX(e.slot), u.cy, u.r - 2.4f,
                  withAlpha(colors::white, 150.f * (1.f - static_cast<float>(age) / kAllyFlash)), kZFx);
        } else if (e.kind == FxKind::Break) {
            float cx = bossX, cy = bossY;
            if (!e.on_enemy) { const UnitPos u = unitPos(e.slot); cx = u.cx; cy = u.cy; }
            if (age < kRingLife) {
                const float t = static_cast<float>(age) / kRingLife;
                fxRing(cx, cy, 8.f + 38.f * easeOut(t), 2.6f - 1.8f * t,
                       withAlpha(colors::broken, 230.f * (1.f - t)), kZFx);
            }
            if (e.on_enemy && age < kScreenFlash) {
                C2D_DrawRectSolid(0, 0, kZFx + 0.02f, kTopWidth, kScreenHeight,
                                  withAlpha(colors::white, 110.f * (1.f - static_cast<float>(age) / kScreenFlash)));
            }
        }
    }
}


void BattleScreen::enter() {
    battle_.start();
    buttons_.reset();
    menu_ = Menu::Plan;
    slot_ = 0;
    restart_ = false;
    resetSwapState();
    frame_ = 0;
    fx_.clear();
    bossBroken_ = bossStunned_ = false;
    detailSkillIdx_ = -1;
    gestureRow_ = -1;
    gestureStartFrame_ = -1;
    gestureHoldFired_ = false;
    lastTapRow_ = -1;
    lastTapFrame_ = -1000;
    suppressNextSkillHit_ = false;
    menuCursor_ = 0;
    usingButtons_ = false;
    keyHoldRow_ = -1;
    keyHoldStartFrame_ = -1;
    keyHoldFired_ = false;

    for (int i = 0; i < 3; ++i) {
        allyDpAnim_[i].snap(battle_.party().front[i].dp);
        allyHpAnim_[i].snap(battle_.party().front[i].hp);
    }
    enemyDpAnim_.snap(battle_.enemy().dp);
    enemyHpAnim_.snap(battle_.enemy().hp);
    odAnim_.snap(0);

    buildButtons();
}

void BattleScreen::resetSwapState() {
    selected_ = -1;
    dragFrom_ = -1;
    dragging_ = false;
    swapCursor_ = 0;
    confirmFocus_ = false;
}

void BattleScreen::buildButtons() {
    buttons_.begin();
    const Combatant& c = battle_.party().front[slot_];

    switch (battle_.phase()) {
    case BattlePhase::Planning:
        if (menu_ == Menu::Plan) {
            // Lingkaran slot, baris aksi, dan Execute digambar sendiri (warna 0): hanya area sentuh.
            for (int i = 0; i < 3; ++i)
                buttons_.add(BtnSlot0 + i, slotCx(i) - 27, kSlotCy - 27, 54, 54, "", true, 0, 0.6f, true);
            buttons_.add(BtnAttack, kListX, rowY(0), kListW, kRowH, "", true, 0);
            for (int j = 0; j < 2; ++j) {
                if (!c.skills[j].valid()) continue;
                buttons_.add(BtnSkill0 + j, kListX, rowY(1 + j), kListW, kRowH, "",
                             battle_.canUseSkill(slot_, j), 0);
            }
            buttons_.add(BtnSwapMode, kSwapCx - kSwapR, kSwapCy - kSwapR, kSwapR * 2, kSwapR * 2, "",
                         true, 0, 0.6f, true);
            buttons_.add(BtnExecute, kExecCx - kExecR, kExecCy - kExecR, kExecR * 2, kExecR * 2, "",
                         true, 0, 0.6f, true);
            buttons_.add(BtnOverdrive, kOdCx - kOdR, kOdCy - kOdR, kOdR * 2, kOdR * 2, "",
                         battle_.odBarsReady() > 0 && !battle_.odActive(), 0, 0.6f, true);
        } else if (menu_ == Menu::Swap) {
            buttons_.add(BtnConfirm, 12, 190, 296, 40, "Confirm", true, colors::pink, 0.6f);
        }
        // Menu::SkillDetail: tidak ada tombol; ketukan di mana pun menutup layar (lihat update()).
        break;
    case BattlePhase::Victory:
    case BattlePhase::Defeat:
        buttons_.add(BtnRestart, 16, 110, 288, 56, "Play Again", true, colors::pink, 0.65f);
        break;
    case BattlePhase::Executing:
    case BattlePhase::EnemyTurn:
    case BattlePhase::Dying:
        break;
    case BattlePhase::Awakening:
        break;
    }
}

void BattleScreen::handle(int id) {
    if (id >= BtnSlot0 && id < BtnSlot0 + 3) {
        slot_ = id - BtnSlot0;
    } else if (id == BtnAttack) {
        battle_.setCommand(slot_, CommandType::Attack);
    } else if (id >= BtnSkill0 && id < BtnSkill0 + 2) {
        battle_.setCommand(slot_, CommandType::Skill, id - BtnSkill0);
    } else if (id == BtnSwapMode) {
        menu_ = Menu::Swap;
        resetSwapState();
    } else if (id == BtnConfirm) {
        menu_ = Menu::Plan;
        resetSwapState();
    } else if (id == BtnExecute) {
        battle_.execute();
        menu_ = Menu::Plan;
    } else if (id == BtnOverdrive) {
        if (battle_.activateOverdrive()) sfx::play(sfx::Id::Overdrive);
    } else if (id == BtnRestart) {
        restart_ = true;
    }
}


const Combatant& BattleScreen::tileCombatant(int id) const {
    return id < 3 ? battle_.party().front[id] : battle_.party().back[id - 3];
}

int BattleScreen::tileAt(int px, int py) const {
    for (int id = 0; id < 6; ++id) {
        const float x = swapX(id), y = swapY(id);
        // Area sentuh sedikit lebih besar dari kotaknya supaya mudah dikenai jari.
        if (px >= x - 4 && px < x + kSwapTile + 4 && py >= y - 4 && py < y + kSwapTile + 4)
            return id;
    }
    return -1;
}

void BattleScreen::doSwap(int a, int b) {
    const bool aFront = a < 3, bFront = b < 3;
    if (aFront && bFront)        battle_.swapFront(a, b);
    else if (!aFront && !bFront) battle_.swapBack(a - 3, b - 3);
    else                         battle_.swapSlot(aFront ? a : b, (aFront ? b : a) - 3);

    // Identitas anggota di tiap slot front bisa berubah lewat swap, jadi bar-nya di-snap
    // langsung ke nilai anggota yang baru (bukan dianimasikan, biar tidak seolah HP/DP-nya
    // yang berubah).
    for (int i = 0; i < 3; ++i) {
        allyDpAnim_[i].snap(battle_.party().front[i].dp);
        allyHpAnim_[i].snap(battle_.party().front[i].hp);
    }
}

void BattleScreen::updateSwap(const TouchState& t) {
    const int kMoveThreshold = 8;    // piksel; lebih kecil dari ini dianggap ketukan

    if (t.began) {
        const int id = tileAt(t.x, t.y);
        if (id >= 0) {
            dragFrom_ = id;
            dragging_ = false;
            dragStartX_ = dragX_ = t.x;
            dragStartY_ = dragY_ = t.y;
        }
    }
    if (dragFrom_ < 0) return;

    if (t.held) {
        dragX_ = t.x;
        dragY_ = t.y;
        if (!dragging_ && (std::abs(t.x - dragStartX_) > kMoveThreshold ||
                           std::abs(t.y - dragStartY_) > kMoveThreshold)) {
            dragging_ = true;
            selected_ = -1;              // mulai menyeret: hanya kotak yang diseret yang disorot
        }
    }
    if (t.ended) {
        const int target = tileAt(t.x, t.y);
        if (dragging_) {
            if (target >= 0 && target != dragFrom_) doSwap(dragFrom_, target);
            selected_ = -1;
        } else if (selected_ < 0) {
            selected_ = dragFrom_;
        } else if (selected_ == dragFrom_) {
            selected_ = -1;
        } else {
            doSwap(selected_, dragFrom_);
            selected_ = -1;
        }
        dragFrom_ = -1;
        dragging_ = false;
    } else if (!t.held) {
        dragFrom_ = -1;
        dragging_ = false;
    }
}


int BattleScreen::skillRowAt(int px, int py) const {
    const Combatant& c = battle_.party().front[slot_];
    for (int j = 0; j < 2; ++j) {
        if (!c.skills[j].valid()) continue;
        const float y = rowY(1 + j);
        if (px >= kListX && px < kListX + kListW && py >= y && py < y + kRowH) return j;
    }
    return -1;
}

void BattleScreen::openSkillDetail(int skillIdx) {
    detailSkillIdx_ = skillIdx;
    menu_ = Menu::SkillDetail;
    suppressNextSkillHit_ = true;   // release yang memicu gesture ini jangan ikut mengubah aksi
}

// Tekan-tahan (>= kLongPressFrames) atau ketuk dua kali (<= kDoubleTapFrames) baris skill yang
// sama membuka layar penjelasan skill itu. Ketukan tunggal biasa tetap memilih aksi seperti biasa.
void BattleScreen::updateSkillGestures(const TouchState& t) {
    const int row = (t.began || t.held) ? skillRowAt(t.x, t.y) : -1;

    if (t.began) {
        gestureRow_ = row;
        gestureStartFrame_ = frame_;
        gestureHoldFired_ = false;
    } else if (t.held && gestureRow_ >= 0) {
        if (!gestureHoldFired_ && row == gestureRow_ &&
            frame_ - gestureStartFrame_ >= kLongPressFrames) {
            gestureHoldFired_ = true;
            openSkillDetail(gestureRow_);
        }
    } else if (t.ended) {
        // Hanya dihitung sebagai ketukan yang sah jika lepasan masih di baris yang sama dengan
        // awal sentuhan (sama seperti aturan tombol biasa: geser keluar baris = batal).
        const int endRow = skillRowAt(t.x, t.y);
        if (gestureRow_ >= 0 && !gestureHoldFired_ && endRow == gestureRow_) {
            if (lastTapRow_ == gestureRow_ && frame_ - lastTapFrame_ <= kDoubleTapFrames) {
                openSkillDetail(gestureRow_);
                lastTapRow_ = -1;                     // cegah ketukan ketiga dianggap dobel lagi
            } else {
                lastTapRow_ = gestureRow_;
                lastTapFrame_ = frame_;
            }
        }
        gestureRow_ = -1;
    } else if (!t.held) {
        gestureRow_ = -1;
    }
}


void BattleScreen::update(const TouchState& touch, u32 keysDown, u32 keysHeld) {
    ++frame_;
    if (touch.began) usingButtons_ = false;   // sentuhan baru = kursor tombol fisik disembunyikan
    const int previousDeathFrame = battle_.defeatFrame();
    battle_.update();
    // Dipicu sekali dari update (bukan draw), supaya redraw tidak memutar ulang audio.
    const unsigned deathSounds = defeat_cutscene::soundCues(previousDeathFrame, battle_.defeatFrame());
    if (deathSounds & defeat_cutscene::PressureCue) sfx::play(sfx::Id::DefeatPressure);
    if (deathSounds & defeat_cutscene::ShatterCue)  sfx::play(sfx::Id::DefeatShatter);
    if (deathSounds & defeat_cutscene::RootsCue)    sfx::play(sfx::Id::DefeatRoots);
    if (deathSounds & defeat_cutscene::SettleCue)   sfx::play(sfx::Id::DefeatSettle);
    for (const FxEvent& e : battle_.takeFx()) fx_.push_back(FxItem(e, frame_ + e.delay));

    // Gerakkan bar yang dianimasikan sedikit menuju nilai sebenarnya tiap frame (lihat
    // AnimatedBar di ui.h), supaya perubahan DP/HP/Overdrive terasa meluncur, bukan patah.
    for (int i = 0; i < 3; ++i) {
        allyDpAnim_[i].update(battle_.party().front[i].dp);
        allyHpAnim_[i].update(battle_.party().front[i].hp);
    }
    enemyDpAnim_.update(battle_.enemy().dp);
    enemyHpAnim_.update(battle_.enemy().hp);
    odAnim_.update(odBarProgress(battle_));

    // Latar bos: NYALA hanya lewat event yang sudah mulai tampil (jadi pergantian gambar tepat
    // bersamaan dengan kilatan/efeknya, tidak mendahului hit beruntun). MATI langsung mengikuti
    // logika. Break otomatis membuat stun, jadi event Break menyalakan keduanya.
    for (FxItem& it : fx_) {
        if (it.applied || !it.ev.on_enemy || frame_ < it.start) continue;
        if (it.ev.kind == FxKind::Break) { bossBroken_ = true; bossStunned_ = true; it.applied = true; }
        else if (it.ev.kind == FxKind::Stun) { bossStunned_ = true; it.applied = true; }
    }
    if (!battle_.enemy().stunned) bossStunned_ = false;
    if (!battle_.enemy().broken)  bossBroken_  = false;
    fx_.erase(std::remove_if(fx_.begin(), fx_.end(),
                             [this](const FxItem& it) { return frame_ - it.start >= kFxLife; }),
              fx_.end());
    if (battle_.phase() != BattlePhase::Planning) {
        menu_ = Menu::Plan;
        resetSwapState();
        detailSkillIdx_ = -1;
    }

    buildButtons();
    if (menu_ == Menu::Swap) {
        updateSwap(touch);
    } else if (menu_ == Menu::Plan) {
        updateSkillGestures(touch);
    } else if (menu_ == Menu::SkillDetail) {
        // Layar penjelasan ditutup oleh SENTUHAN BARU (began), bukan oleh lepasan sentuhan yang
        // baru saja membukanya (yang masih dalam siklus sentuh yang sama untuk gesture tahan).
        if (touch.began) {
            menu_ = Menu::Plan;
            detailSkillIdx_ = -1;
        }
    }

    // Tombol fisik diproses di sini, SEBELUM hit-test sentuh: harus tetap jalan tiap frame walau
    // tidak ada sentuhan sama sekali (hit-test di bawah early-return begitu tidak ada sentuhan).
    updateButtonsInput(keysDown, keysHeld);
    buildButtons();   // tombol fisik bisa mengubah menu/phase, susun ulang sebelum hit-test sentuh

    const int hit = buttons_.update(touch);
    if (hit == ButtonGroup::kNone) return;
    if (suppressNextSkillHit_) {
        suppressNextSkillHit_ = false;
        if (hit == BtnSkill0 || hit == BtnSkill0 + 1) return;
    }

    handle(hit);
    buildButtons();
}


bool BattleScreen::planRowValid(int row) const {
    if (row == 0) return true;
    const Combatant& c = battle_.party().front[slot_];
    return c.skills[row - 1].valid();
}

void BattleScreen::updateButtonsInput(u32 keysDown, u32 keysHeld) {
    const bool up    = (keysDown & (KEY_DUP    | KEY_CPAD_UP))    != 0;
    const bool down  = (keysDown & (KEY_DDOWN  | KEY_CPAD_DOWN))  != 0;
    const bool left  = (keysDown & (KEY_DLEFT  | KEY_CPAD_LEFT))  != 0;
    const bool right = (keysDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) != 0;

    // Setiap tombol navigasi/aksi yang ditekan = kursor tombol fisik jadi terlihat lagi.
    if (keysDown & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT |
                     KEY_CPAD_UP | KEY_CPAD_DOWN | KEY_CPAD_LEFT | KEY_CPAD_RIGHT |
                     KEY_L | KEY_R | KEY_A | KEY_B | KEY_X | KEY_Y))
        usingButtons_ = true;

    if (battle_.phase() == BattlePhase::Victory || battle_.phase() == BattlePhase::Defeat) {
        if (keysDown & KEY_A) handle(BtnRestart);
        return;
    }
    if (battle_.phase() != BattlePhase::Planning) return;

    if (menu_ == Menu::Plan) {
        if (!planRowValid(menuCursor_)) menuCursor_ = 0;

        if (keysDown & KEY_L) { slot_ = (slot_ + 2) % 3; if (!planRowValid(menuCursor_)) menuCursor_ = 0; }
        if (keysDown & KEY_R) { slot_ = (slot_ + 1) % 3; if (!planRowValid(menuCursor_)) menuCursor_ = 0; }

        if (up)   do { menuCursor_ = (menuCursor_ + 2) % 3; } while (!planRowValid(menuCursor_));
        if (down) do { menuCursor_ = (menuCursor_ + 1) % 3; } while (!planRowValid(menuCursor_));

        // Tahan A di baris skill = buka detail (setara tekan-tahan sentuh). Baris Attack tidak
        // punya layar detail, jadi hanya dipantau untuk baris 1/2 (skill).
        if (keysDown & KEY_A) {
            keyHoldRow_ = menuCursor_;
            keyHoldStartFrame_ = frame_;
            keyHoldFired_ = false;
        } else if ((keysHeld & KEY_A) && keyHoldRow_ >= 0) {
            if (!keyHoldFired_ && keyHoldRow_ > 0 && keyHoldRow_ == menuCursor_ &&
                frame_ - keyHoldStartFrame_ >= kLongPressFrames) {
                keyHoldFired_ = true;
                openSkillDetail(keyHoldRow_ - 1);
            }
        } else if (!(keysHeld & KEY_A)) {
            if (keyHoldRow_ >= 0 && !keyHoldFired_) {
                if (keyHoldRow_ == 0) handle(BtnAttack);
                else                  handle(BtnSkill0 + (keyHoldRow_ - 1));
            }
            keyHoldRow_ = -1;
        }

        if (keysDown & KEY_X) handle(BtnExecute);
        if (keysDown & KEY_Y) handle(BtnSwapMode);
    } else if (menu_ == Menu::Swap) {
        if (!confirmFocus_) {
            if (left)  swapCursor_ = (swapCursor_ % 3 == 0) ? swapCursor_ + 2 : swapCursor_ - 1;
            if (right) swapCursor_ = (swapCursor_ % 3 == 2) ? swapCursor_ - 2 : swapCursor_ + 1;
            if (up   && swapCursor_ >= 3) swapCursor_ -= 3;
            if (down) { if (swapCursor_ < 3) swapCursor_ += 3; else confirmFocus_ = true; }

            if (keysDown & KEY_A) {
                if (selected_ < 0)                 selected_ = swapCursor_;
                else if (selected_ == swapCursor_) selected_ = -1;
                else { doSwap(selected_, swapCursor_); selected_ = -1; }
            }
        } else {
            if (up) confirmFocus_ = false;
            if (keysDown & KEY_A) handle(BtnConfirm);
        }
        if (keysDown & (KEY_Y | KEY_B)) { menu_ = Menu::Plan; resetSwapState(); }
    } else if (menu_ == Menu::SkillDetail) {
        if (keysDown & (KEY_A | KEY_B)) {
            menu_ = Menu::Plan;
            detailSkillIdx_ = -1;
        }
    }
}


bool BattleScreen::drawScene3d(C3D_RenderTarget* target) const {
    battle3d::SceneState state;
    state.frame = frame_;
    state.awakeningFrame = battle_.awakeningFrame();
    state.defeatFrame = battle_.defeatFrame();
    state.awakened = battle_.awakened();
    state.broken = bossBroken_;
    state.stunned = bossStunned_;
    state.defeated = battle_.enemy().hp <= 0;
    // Tahan pose sebelum cutscene sampai angka damage terakhir selesai memudar.
    if (battle_.phase() == BattlePhase::Awakening || battle_.phase() == BattlePhase::Dying)
        state.defeated = false;
    for (const FxItem& item : fx_) {
        const int age = frame_ - item.start;
        if (age < 0) {
            if (item.ev.on_enemy && item.ev.kind == FxKind::Damage) state.defeated = false;
            continue;
        }
        if (item.ev.kind != FxKind::Damage) continue;
        if (item.ev.on_enemy && age < 10)
            state.hit = std::max(state.hit, 1.f - age / 10.f);
        if (!item.ev.on_enemy && age < 18)
            state.attack = std::max(state.attack, std::sin(3.14159265f * age / 18.f));
    }
    return battle3d::renderTop(target, state);
}

void BattleScreen::drawTop(TextRenderer& text, bool scene3d) const {
    const int deathFrame = battle_.defeatFrame();
    if (deathFrame >= 0) {
        // Kehancuran 3D dan pohon putih memenuhi layar atas, termasuk saat Victory.
        // Tanpa judul, angka damage, atau HUD battle di atas urutan ini.
        const defeat_cutscene::Pose death = defeat_cutscene::sample(deathFrame);
        if (death.flash > 0.f)
            C2D_DrawRectSolid(0, 0, kZFx, 400, 240,
                             withAlpha(colors::white, 190.f * death.flash));
        const float bars = 18.f * death.camera;
        C2D_DrawRectSolid(0, 0, kZFx + 0.03f, 400, bars, colors::black);
        C2D_DrawRectSolid(0, 240 - bars, kZFx + 0.03f, 400, bars, colors::black);
        return;
    }
    // Cutscene awakening: partikel energi + kilat + letterbox, gantikan HUD battle biasa
    // selama awakening::Duration frame (lihat awakening.h dan Battle::awakeningFrame()).
    const int cutFrame = battle_.awakeningFrame();
    if (cutFrame >= 0) {
        const awakening::Pose cut = awakening::sample(cutFrame);
        float x = 200.f, y = 110.f;
        if (scene3d) battle3d::enemyAnchor(x, y);
        if (cut.energy > 0.f) {
            for (int i = 0; i < 12; ++i) {
                const float angle = i * 0.523599f + cutFrame * 0.035f;
                const float radius = 18.f + std::fmod(cutFrame * 1.2f + i * 13.f, 80.f);
                drawDisc(x + std::cos(angle) * radius, y + std::sin(angle) * radius * 0.55f,
                         1.5f, withAlpha(colors::pinkLight, cut.energy * 220.f), kZFx);
            }
            fxRing(x, y, 24.f + cut.energy * 48.f, 2.f,
                   withAlpha(colors::pink, cut.energy * 180.f), kZFx);
        }
        if (cut.flash > 0.f)
            C2D_DrawRectSolid(0, 0, kZFx + 0.02f, 400, 240,
                             withAlpha(colors::white, cut.flash * 210.f));
        const float bar = 22.f * cut.camera;
        C2D_DrawRectSolid(0, 0, kZFx + 0.03f, 400, bar, C2D_Color32(0, 0, 0, 255));
        C2D_DrawRectSolid(0, 240 - bar, kZFx + 0.03f, 400, bar, C2D_Color32(0, 0, 0, 255));
        return;
    }
    const Party& p = battle_.party();
    const Combatant& e = battle_.enemy();
    const BattlePhase ph = battle_.phase();
    char buf[128];

    // Gradasi gelap tipis di kiri atas (HUD bos) dan di bawah (pesan + party) agar teks terbaca.
    const u32 clear = C2D_Color32(0, 0, 0, 0);
    C2D_DrawRectangle(0, 0, kZBack + 0.02f, 240, 110, C2D_Color32(0, 0, 0, 150), clear,
                      C2D_Color32(0, 0, 0, 70), clear);
    C2D_DrawRectangle(0, 112, kZBack + 0.02f, kTopWidth, 128, clear, clear, C2D_Color32(0, 0, 0, 190),
                      C2D_Color32(0, 0, 0, 190));

    // Aura Overdrive di tepi layar, selama OD aktif (hilang begitu giliran OD habis).
    if (battle_.odActive())
        drawOdAura(frame_, odStageColor(battle_.odLevel()), kZBack + 0.03f);

    drawLine(14, 34, 196, 34, 1.2f, kLineWhite, kZPanel);
    drawLine(14, 113, 122, 113, 1.2f, kLineWhite, kZPanel);

    drawDisc(196, 34, 2.6f, colors::pink, kZCircle);
    drawDisc(14, 113, 2.6f, colors::pink, kZCircle);
    for (int i = 0; i < 3; ++i) {
        const bool highlight = (ph == BattlePhase::Executing && i == battle_.activeSlot()) ||
                               (ph == BattlePhase::Planning &&
                                (menu_ == Menu::Plan || menu_ == Menu::SkillDetail) && i == slot_);
        const UnitPos u = unitPos(i);
        drawRing(u.cx + allyShakeX(i), u.cy, u.r, highlight ? colors::pink : kRingIdle, kZCircle, highlight);
    }

    // HUD bos, tanpa panel. Angka DP/HP bos disembunyikan; cyan = DP, merah muda = HP.
    text.drawOutlined(e.name.c_str(), 14, 8, 0.62f, colors::white, colors::black, 2.f);
    drawBarGrad(14, 40, 182, 5, enemyDpAnim_.display(), e.max_dp, colors::cyan, colors::cyanLight, kTrack);
    drawBarGrad(14, 48, 182, 6, enemyHpAnim_.display(), e.max_hp, colors::pink, colors::pinkLight, kTrack);

    // Counter devastation rate: "cur%/max%", nempel di ujung kanan-bawah bar HP musuh.
    // Hanya tampil saat devastation rate sudah naik dari nilai dasarnya (100%).
    if (e.devastation > 100) {
        std::snprintf(buf, sizeof buf, "%d%%/%d%%", e.devastation, e.max_devastation);
        constexpr float kDevScale = 0.42f;
        const float devW = text.width(buf, kDevScale);
        text.drawShadow(buf, 14 + 182 - devW, 48 + 6 + 2, kDevScale, colors::white);
    }

    // Baris icon status bos: stun, lalu DEF- (angka di sebelahnya = tumpukan dan sisa giliran).
    // Digeser naik ke tepat di bawah bar HP dan diperbesar (kBadgeScale) supaya lebih menonjol.
    // Efek lain yang belum punya icon tetap tampil sebagai teks di ujung baris.
    constexpr float kBadgeScale = 1.4f;
    const float badgeR = kBadgeR * kBadgeScale;
    const float bcy = 48.f + 6.f + 2.f + badgeR;
    float bx = 14.f;
    if (bossStunned_) {
        drawBadgeStun(bx + badgeR, bcy, kBadgeScale);
        bx += 2.f * badgeR + 6.f;
    }
    int dfN, dfTurns;
    effectStats(e, EffectType::DefDown, dfN, dfTurns);
    if (dfN > 0) {
        for (int i = 0; i < dfN; ++i) {
            drawBadgeDefDown(bx + badgeR, bcy, kBadgeScale);
            bx += 2.f * badgeR + 3.f;
        }
        bx += 5.f;
    }
    const std::string debuffs = effectSummary(e, effectBit(EffectType::DefDown));
    if (!debuffs.empty()) text.drawShadow(debuffs.c_str(), bx, bcy - 6.f, 0.3f, colors::pinkLight);

    // TURN di kiri tengah. Counter OD "sisa/total" (misal 2/2, 2/3) muncul selama OD aktif dan
    // hilang saat sisa giliran 0 (OD selesai). Sisa termasuk giliran yang sedang berjalan. Counter
    // selalu kuning (satu warna untuk semua stage), nomor TURN tetap putih.
    std::snprintf(buf, sizeof buf, "%d", battle_.round());
    text.drawShadow("TURN", 14, 93, 0.36f, colors::white);
    const bool odOn = battle_.odActive() && battle_.odTurnsRemaining() > 0;
    const float turnNumX = 14 + text.width("TURN", 0.36f) + 6;
    text.drawShadow(buf, turnNumX, 85, 0.62f, colors::white);
    if (odOn) {
        char odBuf[16];
        std::snprintf(odBuf, sizeof odBuf, "%d/%d", battle_.odTurnsRemaining(), battle_.odTurnsTotal());
        text.drawShadow(odBuf, turnNumX + text.width(buf, 0.62f) + 6, 88, 0.42f, colors::yellow);
    }

    // Gauge Overdrive: badge di pojok kanan atas (angka = bar siap pakai) dan bar progres di kirinya,
    // terisi dari kanan ke kiri dengan warna stage BERIKUTNYA. Track abu-abu polos sebelum ada bar;
    // setelah itu track memakai warna level yang sudah dicapai.
    {
        const int bars = battle_.odBarsReady();
        const bool ready = bars > 0;
        const u32 badgeColor = bars > 0 ? odStageColor(bars) : kRingIdle;
        const u32 barColor = odStageColor(bars + 1);
        const u32 trackColor = bars > 0 ? badgeColor : kTrack;

        drawBarGradRTL(kOdBarX, kOdBarY, kOdBarW, kOdBarH, odAnim_.display(), kOdHitsPerBar,
                       barColor, mixColor(colors::bg, barColor, 0.4f), trackColor);
        drawOdBadge(text, kOdBadgeCx, kOdBadgeCy, kOdBadgeR, badgeColor,
                   mixColor(colors::bg, badgeColor, 0.35f), bars, ready, 0.55f,
                   kZBar + 0.03f);   // di atas bar OD, supaya badge nggak ketiban pas tumpang tindih
    }

    for (int i = 0; i < 3; ++i)
        drawPartyUnit(text, p.front[i], i, allyShakeX(i), allyDpAnim_[i].display(), allyHpAnim_[i].display());

    drawFx(text, scene3d);     // angka damage dan efek, paling akhir supaya di atas segalanya
}


// Perencanaan: pilih slot (lingkaran, atas), lalu langsung pilih Attack/skill slot itu (pil).
void BattleScreen::drawPlanning(TextRenderer& text) const {
    const Party& p = battle_.party();
    const Combatant& c = p.front[slot_];
    const Command& cmd = battle_.command(slot_);

    for (int k = 0; k < 3; ++k) {
        const int j = k - 1;
        if (j >= 0 && !c.skills[j].valid()) continue;
        const bool isAttack = (k == 0);
        const bool enabled = isAttack || battle_.canUseSkill(slot_, j);
        const bool chosen = isAttack ? (cmd.type == CommandType::Attack)
                                     : (cmd.type == CommandType::Skill && cmd.skill == j);
        const int id = isAttack ? BtnAttack : BtnSkill0 + j;

        const float amt = buttons_.pressed(id) ? 0.34f : (chosen ? 0.26f : (enabled ? 0.10f : 0.04f));
        drawPill(kListX, rowY(k), kListW, kRowH, mixColor(colors::bg, colors::pink, amt),
                 chosen ? colors::pink : mixColor(colors::bg, colors::white, enabled ? 0.45f : 0.2f),
                 kZPanel);
        // Sorot tipis putih: baris yang sedang disorot kursor tombol fisik (L/R + D-Pad/Circle Pad).
        if (k == menuCursor_ && usingButtons_)
            drawCursorBrackets(kListX - 2, rowY(k) - 2, kListW + 4, kRowH + 4, colors::pink, kZPanel + 0.02f);
    }

    for (int i = 0; i < 3; ++i) {
        const bool sel = (i == slot_);
        drawRing(slotCx(i), kSlotCy, kSlotR, sel ? colors::pink : kRingIdle, kZCircle, sel);
    }
    const bool swapDown = buttons_.pressed(BtnSwapMode);
    drawRing(kSwapCx, kSwapCy, kSwapR, colors::pink, kZCircle, false);
    drawDisc(kSwapCx, kSwapCy, kSwapR - 2.4f, swapDown ? colors::pinkDeep : mixColor(colors::bg, colors::pink, 0.35f),
             kZCircle + 0.02f);

    const bool execDown = buttons_.pressed(BtnExecute);
    drawRing(kExecCx, kExecCy, kExecR, colors::pink, kZCircle, true);
    drawDisc(kExecCx, kExecCy, kExecR - 2.4f, execDown ? colors::pinkDeep : colors::pink,
             kZCircle + 0.02f);
    if (!execDown) drawDisc(kExecCx, kExecCy, kExecR - 6.f, (colors::pinkLight & 0x00FFFFFFu) | (60u << 24),
                            kZCircle + 0.03f);

    // Tombol Overdrive: redup dan tanpa pijar kalau belum ada bar; begitu siap, warnanya ikut
    // stage yang sedang siap dipakai. Badge-nya sama gayanya dengan gauge di layar atas
    // (drawOdBadge), supaya keduanya kelihatan satu bahasa visual.
    const int odBars = battle_.odBarsReady();
    const bool odReady = odBars > 0 && !battle_.odActive();   // sedang OD: tombol redup, tidak bisa dipakai
    const bool odDown = buttons_.pressed(BtnOverdrive);
    const u32 odColor = odReady ? odStageColor(odBars) : kRingIdle;
    drawOdBadge(text, kOdCx, kOdCy, kOdR, odColor,
               mixColor(colors::bg, odColor, odDown ? 0.7f : 0.35f), odBars, odReady, 0.7f);

    // Tag peran dan SP sengaja tidak diulang di sini; keduanya sudah ada di kartu layar atas.
    for (int i = 0; i < 3; ++i) {
        const Combatant& m = p.front[i];
        drawCharIcon(text, m, slotCx(i), kSlotCy, (kSlotR - 2.4f) * 2.f, kZSprite,
                     m.broken ? colors::broken : 0);
    }

    // Rincian skill (hit, gimmick, sisa pemakaian) sengaja tidak ditampilkan di sini; tekan-tahan
    // atau ketuk dua kali baris skill untuk membuka layar penjelasannya. Biaya SP tetap ditampilkan
    // di kanan baris karena itu yang paling menentukan bisa/tidaknya skill dipakai sekarang.
    for (int k = 0; k < 3; ++k) {
        const int j = k - 1;
        if (j >= 0 && !c.skills[j].valid()) continue;
        const bool isAttack = (k == 0);
        const bool enabled = isAttack || battle_.canUseSkill(slot_, j);
        const float y = rowY(k);
        const u32 textColor = enabled ? colors::white : colors::grey;
        text.drawShadow(isAttack ? "Attack" : c.skills[j].name, kListX + 20, y + 7, 0.5f, textColor);
        if (!isAttack) {
            char buf[16];
            std::snprintf(buf, sizeof buf, "-%d SP", c.skills[j].sp_cost);
            text.drawRight(buf, kListX + kListW - 18, y + 10, 0.4f,
                           enabled ? colors::pinkLight : colors::grey);
        }
    }

    // Lambang Swap digeser sedikit ke atas dari pusat lingkaran supaya label "Swap" tetap muat.
    {
        const float icx = kSwapCx, icy = kSwapCy - 6.f;
        drawLine(icx - 11, icy - 6, icx + 4, icy - 6, 2.2f, colors::white, kZIcon);
        C2D_DrawTriangle(icx + 2, icy - 11, colors::white, icx + 2, icy - 1, colors::white,
                         icx + 11, icy - 6, colors::white, kZIcon);
        drawLine(icx - 4, icy + 6, icx + 11, icy + 6, 2.2f, colors::white, kZIcon);
        C2D_DrawTriangle(icx - 2, icy + 1, colors::white, icx - 2, icy + 11, colors::white,
                         icx - 11, icy + 6, colors::white, kZIcon);
    }
    text.drawCentered("Swap", kSwapCx, kSwapCy + 15, 0.34f, colors::white);

    C2D_DrawTriangle(kExecCx - 6, kExecCy - 18, colors::white, kExecCx - 6, kExecCy - 2, colors::white,
                     kExecCx + 8, kExecCy - 10, colors::white, kZIcon);
    text.drawCentered("Execute!", kExecCx, kExecCy + 12, 0.4f, colors::white);

}

void BattleScreen::drawSwapScreen(TextRenderer& text) const {
    const int hover = dragging_ ? tileAt(dragX_, dragY_) : -1;

    drawLine(66, 17, 304, 17, 1.2f, kLineWhite, kZPanel);
    drawLine(52, 103, 304, 103, 1.2f, kLineWhite, kZPanel);

    for (int id = 0; id < 6; ++id) {
        const float cx = swapX(id) + kSwapTile * 0.5f, cy = swapY(id) + kSwapTile * 0.5f;
        const float r = (id < 3) ? 26.f : 22.f;
        const bool lifted = dragging_ && id == dragFrom_;
        const bool picked = (id == selected_ || lifted);
        const bool over = (dragging_ && id == hover);
        const u32 ring = picked ? colors::pink : (over ? colors::white : kRingIdle);
        drawRing(cx, cy, r, ring, kZCircle, picked || over);
        // Sorot kursor tombol fisik: kurung pink di sudut lingkaran yang sedang disorot.
        if (id == swapCursor_ && usingButtons_ && !confirmFocus_)
            drawCursorBrackets(cx - r - 1, cy - r - 1, r * 2 + 2, r * 2 + 2,
                                colors::pink, kZCircle + 0.05f, 5.f);
    }

    text.drawShadow("FRONT", 16, 8, 0.36f, colors::pinkLight);
    text.drawShadow("BACK", 16, 94, 0.36f, colors::pinkLight);
    text.drawRight("Drag or tap two characters to swap", 304, 5, 0.3f, colors::grey);

    for (int id = 0; id < 6; ++id) {
        const Combatant& c = tileCombatant(id);
        const float cx = swapX(id) + kSwapTile * 0.5f, cy = swapY(id) + kSwapTile * 0.5f;
        const float r = (id < 3) ? 26.f : 22.f;
        const bool lifted = dragging_ && id == dragFrom_;
        drawCharIcon(text, c, cx, cy, (r - 2.4f) * 2.f, kZSprite,
                     lifted ? kDimTint : (c.broken ? colors::broken : 0));

        char buf[16];
        std::snprintf(buf, sizeof buf, "SP %d", c.sp);
        text.drawCentered(buf, cx, cy + r + 8, 0.32f, colors::white);
    }

    if (dragging_ && dragFrom_ >= 0)
        drawCharIcon(text, tileCombatant(dragFrom_), dragX_, dragY_, 58, kZDrag, 0);

    if (confirmFocus_ && usingButtons_)
        drawCursorBrackets(12 - 2, 190 - 2, 296 + 4, 40 + 4, colors::pink, kZPanel + 0.06f);
}

// Membungkus teks per kata agar tidak melebihi maxWidth px. C2D_WordWrap tidak melaporkan jumlah
// baris hasilnya, jadi dihitung sendiri supaya baris berikutnya tidak menumpuk di atas teks.
std::vector<std::string> wrapText(TextRenderer& text, const std::string& s, float scale, float maxWidth) {
    std::vector<std::string> lines;
    std::string cur;
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t next = s.find(' ', pos);
        std::string word = s.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        std::string candidate = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && text.width(candidate.c_str(), scale) > maxWidth) {
            lines.push_back(cur);
            cur = word;
        } else {
            cur = candidate;
        }
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

// Layar penjelasan satu skill: nama, biaya SP, jumlah hit/efek, dan sisa pemakaian.
void BattleScreen::drawSkillDetail(TextRenderer& text) const {
    if (detailSkillIdx_ < 0 || detailSkillIdx_ > 1) return;
    const Combatant& c = battle_.party().front[slot_];
    const Skill& s = c.skills[detailSkillIdx_];
    char buf[64];

    drawFlat(8, 8, 304, 224, 10.f, mixColor(colors::bg, colors::pink, 0.10f), kZPanel);
    drawLine(24, 46, 296, 46, 1.2f, kLineWhite, kZPanel + 0.01f);

    text.drawShadow(s.name, 22, 16, 0.6f, colors::white);
    std::snprintf(buf, sizeof buf, "-%d SP", s.sp_cost);
    text.drawRight(buf, 296, 19, 0.42f, colors::pinkLight);

    float y = 56.f;
    auto line = [&](const std::string& t) {
        text.drawShadow(t.c_str(), 22, y, 0.4f, colors::white);
        y += 20.f;
    };

    std::string desc = s.description;
    const char* tag = skillTag(s);
    if (tag[0]) { if (!desc.empty()) desc += ' '; desc += tag; }
    for (const std::string& wrapped : wrapText(text, desc, 0.4f, 276.f)) line(wrapped);

    if (s.kind == SkillKind::Attack) {
        const int mult = s.dev_mult;
        if (mult > 1) {
            std::snprintf(buf, sizeof buf, "Increases enemy devastation rate %dx faster", mult);
            line(buf);
        }
        if (s.hp_pct != 100) {
            std::snprintf(buf, sizeof buf, "Damage to HP: %+d%%", s.hp_pct - 100);
            line(buf);
        }
        if (s.dp_pct != 100) {
            std::snprintf(buf, sizeof buf, "Damage to DP: %+d%%", s.dp_pct - 100);
            line(buf);
        }
    } else if (s.kind == SkillKind::HealDP) {
        std::snprintf(buf, sizeof buf, "Restores %d DP per ally", s.power);
        line(buf);
    }

    const std::string fx = effectLabel(s.fx);
    if (!fx.empty()) line("Effect: " + fx);
    if (s.stun_chance > 0) {
        std::snprintf(buf, sizeof buf, "Chance to stun the enemy: %d%%", s.stun_chance);
        line(buf);
    }
    if (s.sp_gain > 0) {
        std::snprintf(buf, sizeof buf, "Chance to restore %d SP to self: %d%%", s.sp_gain,
                      s.sp_gain_chance);
        line(buf);
    }
    if (c.uses_left[detailSkillIdx_] >= 0) {
        std::snprintf(buf, sizeof buf, "Uses remaining this battle: %d", c.uses_left[detailSkillIdx_]);
        line(buf);
    }

    drawPill(70, 180, 180, 34, mixColor(colors::bg, colors::pink, 0.28f), colors::pink, kZFill);
    text.drawCentered("Close", 160, 197, 0.5f, colors::white);
    text.drawCentered("(tap anywhere)", 160, 218, 0.28f, colors::grey);
}

void BattleScreen::drawBottom(TextRenderer& text) const {
    drawBottomBackdrop();

    switch (battle_.phase()) {
    case BattlePhase::Planning:
        if (menu_ == Menu::Plan)             drawPlanning(text);
        else if (menu_ == Menu::Swap)        drawSwapScreen(text);
        else                                 drawSkillDetail(text);
        break;
    case BattlePhase::Executing:
        text.drawCentered("Executing action...", 160, 116, 0.55f, colors::white);
        break;
    case BattlePhase::EnemyTurn:
        text.drawCentered("Enemy turn...", 160, 116, 0.55f, colors::white);
        break;
    case BattlePhase::Dying:
        break;
    case BattlePhase::Awakening: {
        const int cf = battle_.awakeningFrame();
        const char* line = cf < 66 ? "..." :
                            (cf < awakening::Reveal ? "Something is stirring..." :
                                                       "Hellspider awakens!");
        text.drawCentered(line, 160, 116, 0.55f, colors::white);
        break;
    }
    case BattlePhase::Victory:
        text.drawOutlined("VICTORY!", 160 - text.width("VICTORY!", 0.8f) * 0.5f, 60, 0.8f, colors::white,
                          colors::pinkDeep, 2.f);
        break;
    case BattlePhase::Defeat:
        text.drawOutlined("DEFEAT...", 160 - text.width("DEFEAT...", 0.8f) * 0.5f, 60, 0.8f, colors::white,
                          colors::pinkDeep, 2.f);
        break;
    }

    buttons_.draw(text);
}