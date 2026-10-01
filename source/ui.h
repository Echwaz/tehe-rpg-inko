#pragma once
// Input sentuh, tombol layar bawah, render teks Citro2D, helper gambar.
#include <3ds.h>
#include <citro2d.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <map>

constexpr float kTopWidth     = 400.f;
constexpr float kBottomWidth  = 320.f;
constexpr float kScreenHeight = 240.f;

// Lapisan kedalaman (z). Nilai lebih besar = digambar di atas.
constexpr float kZBack  = 0.00f;
constexpr float kZPanel = 0.30f;
constexpr float kZFill  = 0.35f;
constexpr float kZSprite = 0.38f;   // di atas isi panel, di bawah bar dan teks
constexpr float kZBar   = 0.40f;
constexpr float kZIcon  = 0.50f;   // ikon di atas bar, di bawah teks
constexpr float kZText  = 0.60f;

namespace colors {
const u32 bg        = C2D_Color32(  7,  19,  33, 255);
const u32 panel     = C2D_Color32( 16,  44,  64, 255);
const u32 cyan      = C2D_Color32( 76, 189, 232, 255);
const u32 yellow    = C2D_Color32(248, 219,  87, 255);
const u32 pink      = C2D_Color32(240,  82, 176, 255);   // warna aksi utama (cincin, Execute, HP)
const u32 pinkLight = C2D_Color32(255, 176, 222, 255);
const u32 pinkDeep  = C2D_Color32(120,  30,  90, 255);
const u32 cyanLight = C2D_Color32(178, 236, 255, 255);
const u32 disc      = C2D_Color32( 18,  10,  30, 255);   // dasar gelap di dalam cincin potret
const u32 danger    = pink;
const u32 purple    = C2D_Color32(124,  59, 230, 255);
const u32 white     = C2D_Color32(242, 247, 250, 255);
const u32 grey      = C2D_Color32(139, 169, 184, 255);
const u32 black     = C2D_Color32(  0,   0,   0, 255);
const u32 btn       = pink;
const u32 glass     = C2D_Color32(  7,  19,  33, 195);
const u32 line      = C2D_Color32( 76, 189, 232, 150);
const u32 tileBg    = C2D_Color32( 12,  34,  50, 255);
const u32 barBg     = C2D_Color32( 14,  28,  42, 255);
const u32 dp        = cyan;
const u32 hp        = pink;
const u32 broken    = C2D_Color32(255, 150,  40, 255);
const u32 ally      = C2D_Color32( 70, 120, 220, 255);
}  // namespace colors


struct TouchState {
    bool began = false;
    bool held  = false;
    bool ended = false;
    int  x = 0, y = 0;    // posisi terakhir yang diketahui (tetap valid saat ended)

    void update();        // panggil setelah hidScanInput()
};


class TextRenderer {
public:
    bool init(size_t maxGlyphs = 4096);
    void shutdown();
    void beginFrame();   // panggil sekali setelah C3D_FrameBegin

    // (x, y) = pojok kiri atas. wrap > 0 mengaktifkan word wrap pada lebar tsb.
    void draw(const char* str, float x, float y, float scale, u32 color, float wrap = 0.f);
    // (cx, cy) = titik tengah teks (horizontal dan vertikal).
    void drawCentered(const char* str, float cx, float cy, float scale, u32 color);
    // Lebar teks (piksel) pada skala tertentu.
    float width(const char* str, float scale);
    // (xr, y) = pojok kanan atas: teks rata kanan.
    void drawRight(const char* str, float xr, float y, float scale, u32 color);
    // Teks dengan bayangan tipis (lebih murah dari garis tepi): dipakai untuk teks kecil di atas gambar.
    void drawShadow(const char* str, float x, float y, float scale, u32 color, float wrap = 0.f);
    // Teks dengan garis tepi (mis. nama bos di atas gambar). thick = tebal garis tepi (piksel).
    void drawOutlined(const char* str, float x, float y, float scale, u32 color, u32 outline,
                      float thick = 2.f);

private:
    C2D_Text parse(const char* str);
    C2D_TextBuf buf_ = nullptr;
    C2D_TextBuf cacheBuf_ = nullptr;
    std::map<std::string, C2D_Text> cache_;
    size_t cacheCapacity_ = 0;
    bool resetCache_ = false;
};


struct Button {
    int         id;
    float       x, y, w, h;
    std::string label;
    bool        enabled;
    u32         color;
    float       scale;
    bool        circle;     // true: area sentuh berupa lingkaran di tengah kotak (x, y, w, h)

    bool contains(int px, int py) const {
        if (circle) {
            const float dx = px - (x + w * 0.5f), dy = py - (y + h * 0.5f), r = std::min(w, h) * 0.5f;
            return dx * dx + dy * dy <= r * r;
        }
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

// Tombol mode "immediate": daftar dibangun ulang tiap frame lewat begin()/add(),
// sedangkan status tekan disimpan berdasarkan id sehingga tetap terjaga.
// Tombol aktif saat jari DIANGKAT di atas tombol yang sama dengan tempat sentuhan
// dimulai (geser keluar = batal), jadi tidak ada salah tekan.
class ButtonGroup {
public:
    static constexpr int kNone = -1;

    void begin() { buttons_.clear(); }
    void reset() { buttons_.clear(); pressedId_ = kNone; inside_ = false; }
    void add(int id, float x, float y, float w, float h, const char* label,
             bool enabled = true, u32 color = colors::btn, float scale = 0.6f, bool circle = false);

    int  update(const TouchState& t);          // mengembalikan id yang aktif, atau kNone
    // Tombol berwarna 0 tidak digambar oleh draw(): pemanggil menggambarnya sendiri
    // (mis. kartu slot dengan ikon) dan memakai pressed() untuk efek tekan.
    void draw(TextRenderer& text) const;
    bool pressed(int id) const { return id == pressedId_ && inside_; }
    void useBattleStyle(bool on) { battleStyle_ = on; }

private:
    int  hitId(int px, int py) const;
    bool exists(int id) const;

    std::vector<Button> buttons_;
    int  pressedId_ = kNone;
    bool inside_    = false;
    bool battleStyle_ = false;
};


// Campuran dua warna: t = 0 -> a, t = 1 -> b. Hasilnya tidak transparan.
u32 mixColor(u32 a, u32 b, float t);

// Garis lurus dengan ketebalan t piksel.
void drawLine(float x0, float y0, float x1, float y1, float t, u32 color, float z);

// Panel bersudut patah (chamfer) dengan garis tepi tipis. Bagian isinya tidak
// saling tumpang tindih, jadi aman dipakai dengan warna isi transparan.
// brackets = tambahkan sudut terang di kiri atas dan kanan bawah.
void drawAngular(float x, float y, float w, float h, float cut, u32 fill, u32 line, float z,
                 bool brackets = false);

// Panel bersudut patah dengan sudut terang saja (tanpa garis tepi penuh): kiri atas dan kanan bawah.
void drawBracketed(float x, float y, float w, float h, float cut, u32 fill, u32 bracket, float z);
// Panel bersudut patah hanya berisi warna, tanpa garis apa pun.
void drawFlat(float x, float y, float w, float h, float cut, u32 fill, float z);

// Panel kaca gelap transparan tanpa garis tepi.
void drawGlass(float x, float y, float w, float h, float cut = 6.f);
// Panel dengan isi dan garis tepi berwarna.
void drawPanel(float x, float y, float w, float h, u32 fill, u32 border);
// Persegi bersudut membulat (sudut dari segitiga kipas, bukan lingkaran, jadi tidak memicu
// pergantian mode lingkaran citro2d). Aman untuk warna transparan.
void drawRoundRect(float x, float y, float w, float h, float r, u32 color, float z);
// Tombol/baris berbentuk pil dengan garis tepi berwarna. fill dan border tidak transparan.
void drawPill(float x, float y, float w, float h, u32 fill, u32 border, float z);

// Lingkaran digambar dari kipas segitiga (bukan C2D_DrawCircle, yang tampil sebagai kotak di
// beberapa emulator). Konvensi: kumpulkan lingkaran satu layar dalam satu blok, sebelum ikon,
// bar, dan teks (urutan lapisan naik).
void drawDisc(float cx, float cy, float r, u32 color, float z);
// Cincin potret: cincin berwarna + dasar gelap. glow = tambahkan pijar berlapis di luarnya.
// Ikon bulat digambar di atasnya (diameter 2 * (r - 2.4)).
void drawRing(float cx, float cy, float r, u32 ring, float z, bool glow);
// Bar dengan gradasi horizontal dan alur gelap.
void drawBarGrad(float x, float y, float w, float h, int cur, int max, u32 left, u32 right, u32 track);

void drawOutline(float x, float y, float w, float h, float thick, u32 color, float z);
void drawBar(float x, float y, float w, float h, int cur, int max, u32 fg, u32 bg);


// Nilai bar (DP/HP/Overdrive) yang meluncur halus menuju target sebenarnya tiap frame,
// bukan langsung melompat ke posisi baru. Asumsi dipanggil sekali per frame (C3D vsync ~60fps).
// Pakai display() sebagai pengganti 'cur' saat memanggil drawBarGrad/drawBar.
struct AnimatedBar {
    float value = 0.f;
    bool  ready = false;   // false = belum diinisialisasi: nilai pertama langsung snap, tanpa animasi

    // Panggil sekali per frame dengan nilai target (dp/hp/progress) dari data pertarungan.
    void update(int target) {
        const float t = static_cast<float>(target);
        if (!ready) { value = t; ready = true; return; }
        const float diff = t - value;
        if (diff == 0.f) return;
        // Ease-out: makin jauh jaraknya makin cepat, plus kecepatan dasar supaya perubahan
        // kecil (mis. selisih 1 poin) tetap kelihatan bergerak, bukan diam lalu tiba-tiba pindah.
        const float speed = std::fabs(diff) * 0.2f + 1.5f;
        value += (std::fabs(diff) <= speed) ? diff : (diff > 0.f ? speed : -speed);
    }

    // Lompat langsung ke nilai target tanpa animasi (mis. saat tukar formasi / battle mulai).
    void snap(int target) { value = static_cast<float>(target); ready = true; }

    int display() const { return static_cast<int>(value + 0.5f); }
};
