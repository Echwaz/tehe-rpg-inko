#include "assets.h"

#include <cstring>
#include <string>
#include <vector>

#include "icon_loader.h"

// Dibuat otomatis oleh tex3ds saat build: konstanta nomor gambar, mis. sprites_ruka_icon_idx.
#include "bg_bottom.h"
#include "sprites.h"

// Data .t3x yang ditanam Makefile (bin2o). bin2o hanya membuat dua simbol: awal data dan
// akhir data. Konstanta "_size" hanya ada di header hasil bin2o, bukan simbol untuk linker,
// jadi ukuran dihitung dari selisih keduanya.
extern "C" {
extern const u8 sprites_t3x[];
extern const u8 sprites_t3x_end[];
extern const u8 bg_bottom_t3x[];
extern const u8 bg_bottom_t3x_end[];
}

namespace assets {
namespace {

C2D_SpriteSheet g_sprites    = nullptr;
C2D_SpriteSheet g_bgBottom   = nullptr;

// Urutan harus sama dengan enum CharId.
const size_t kIconIdx[kCharCount] = {
    sprites_ruka_icon_idx,  sprites_yuki_icon_idx,   sprites_tama_icon_idx,
    sprites_karen_icon_idx, sprites_megumi_icon_idx, sprites_tsukasa_icon_idx,
};

// Kalau PNG-nya tidak ada, dipakai ikon cadangan bawaan (g_sprites di atas).
const char* const kIconDir = "sdmc:/3ds/Tehe-RPG-inko/assets/";
const char* const kIconFile[kCharCount] = {
    "ruka_icon.png",  "yuki_icon.png",   "tama_icon.png",
    "karen_icon.png", "megumi_icon.png", "tsukasa_icon.png",
};
const int kIconCanvas = 128;   // ukuran tekstur GPU untuk tiap ikon runtime (persegi, pangkat dua)

struct RuntimeIcon {
    C3D_Tex tex{};
    Tex3DS_SubTexture subtex{};
    bool loaded = false;
};
RuntimeIcon g_runtimeIcon[kCharCount];

// Decode PNG (icon_loader, murni C++) lalu upload ke C3D_Tex lewat mesin transfer GPU,
// yang sekalian menyusun ulang datanya ke format tiled yang dibutuhkan GPU (sama seperti
// yang dilakukan tex3ds saat build, tapi di sini terjadi saat game jalan).
bool loadRuntimeIcon(int id) {
    const std::string path = std::string(kIconDir) + kIconFile[id];
    std::vector<uint8_t> rgba;
    if (!icon_loader::loadResized(path, kIconCanvas, rgba)) return false;

    u8* src = static_cast<u8*>(linearAlloc(rgba.size()));
    if (!src) return false;
    // LodePNG menghasilkan R,G,B,A, sedangkan transfer 3DS mengharapkan byte A,B,G,R.
    // Salinan langsung membuat alfa terbaca sebagai merah dan sebaliknya.
    icon_loader::copyToGpuRgba8(rgba.data(), src, rgba.size() / 4);
    GSPGPU_FlushDataCache(src, static_cast<u32>(rgba.size()));

    RuntimeIcon& icon = g_runtimeIcon[id];
    if (!C3D_TexInit(&icon.tex, kIconCanvas, kIconCanvas, GPU_RGBA8)) {
        linearFree(src);
        return false;
    }
    C3D_TexSetFilter(&icon.tex, GPU_LINEAR, GPU_LINEAR);

    const u32 flags = GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
                       GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                       GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                       GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
    C3D_SyncDisplayTransfer(reinterpret_cast<u32*>(src), GX_BUFFER_DIM(kIconCanvas, kIconCanvas),
                             static_cast<u32*>(icon.tex.data),
                             GX_BUFFER_DIM(kIconCanvas, kIconCanvas), flags);
    linearFree(src);

    icon.subtex = { static_cast<u16>(kIconCanvas), static_cast<u16>(kIconCanvas),
                     0.0f, 1.0f, 1.0f, 0.0f };
    icon.loaded = true;
    return true;
}

C2D_SpriteSheet load(const u8* begin, const u8* end) {
    return C2D_SpriteSheetLoadFromMem(begin, static_cast<size_t>(end - begin));
}

void release(C2D_SpriteSheet& sheet) {
    if (sheet) C2D_SpriteSheetFree(sheet);
    sheet = nullptr;
}

bool drawFit(C2D_SpriteSheet sheet, size_t idx, float x, float y, float w, float h, float z,
             u32 tint) {
    if (!sheet || idx >= C2D_SpriteSheetCount(sheet)) return false;
    const C2D_Image img = C2D_SpriteSheetGetImage(sheet, idx);
    const C2D_DrawParams params = { { x, y, w, h }, { 0.0f, 0.0f }, z, 0.0f };
    if (tint) {
        C2D_ImageTint t;
        C2D_PlainImageTint(&t, tint, 0.5f);
        return C2D_DrawImage(img, &params, &t);
    }
    return C2D_DrawImage(img, &params, nullptr);
}

}  // namespace

bool init() {
    g_sprites   = load(sprites_t3x, sprites_t3x_end);
    g_bgBottom  = load(bg_bottom_t3x, bg_bottom_t3x_end);

    for (int i = 0; i < kCharCount; ++i) loadRuntimeIcon(i);

    return g_sprites && g_bgBottom;
}

void shutdown() {
    release(g_sprites);
    release(g_bgBottom);
    for (auto& icon : g_runtimeIcon) {
        if (icon.loaded) { C3D_TexDelete(&icon.tex); icon.loaded = false; }
    }
}

int charFromName(const std::string& name) {
    static const struct { const char* key; int id; } kNames[] = {
        { "Ruka", kRuka },       { "Yuki", kYuki },    { "Tama", kTama },
        { "Karen", kKaren },     { "Karrie", kKaren }, { "Megumi", kMegumi },
        { "Tsukasa", kTsukasa },
    };
    for (const auto& n : kNames) {
        // Cocok di awal nama, jadi "Ruka" maupun "Ruka Kayamori" sama-sama dikenali.
        if (name.compare(0, std::strlen(n.key), n.key) == 0) return n.id;
    }
    return -1;
}

bool drawIcon(int id, float x, float y, float w, float h, float z, u32 tint) {
    if (id < 0 || id >= kCharCount) return false;

    // Ikon dari kartu SD diutamakan, kalau tidak ada dipakai cadangan bawaan.
    const RuntimeIcon& icon = g_runtimeIcon[id];
    if (icon.loaded) {
        const C2D_Image img = { const_cast<C3D_Tex*>(&icon.tex), &icon.subtex };
        const C2D_DrawParams params = { { x, y, w, h }, { 0.0f, 0.0f }, z, 0.0f };
        if (tint) {
            C2D_ImageTint t;
            C2D_PlainImageTint(&t, tint, 0.5f);
            return C2D_DrawImage(img, &params, &t);
        }
        return C2D_DrawImage(img, &params, nullptr);
    }
    return drawFit(g_sprites, kIconIdx[id], x, y, w, h, z, tint);
}

bool drawBottomBg(float z) { return drawFit(g_bgBottom, bg_bottom_idx, 0, 0, 320, 240, z, 0); }

}  // namespace assets
