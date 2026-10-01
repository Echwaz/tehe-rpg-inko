// Tes decode + resize ikon PNG milik pemain. Berjalan di PC, tanpa devkitPro.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../source/icon_loader.h"
#include "../source/third_party/lodepng.h"

namespace {

std::string tempPngPath(const char* name) {
    return std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp") + "/" + name;
}

// Menulis PNG w x h sederhana ke disk: piksel (x,y) = warna solid berbeda per kuadran,
// supaya gampang dicek arah/orientasinya tidak tertukar setelah resize.
void writeQuadPng(const std::string& path, unsigned w, unsigned h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    for (unsigned y = 0; y < h; ++y) {
        for (unsigned x = 0; x < w; ++x) {
            unsigned char* p = &px[(static_cast<size_t>(y) * w + x) * 4];
            const bool left = x < w / 2, top = y < h / 2;
            // Kiri-atas merah, kanan-atas hijau, kiri-bawah biru, kanan-bawah putih.
            p[0] = (left && top) ? 255 : 0;
            p[1] = (!left && top) ? 255 : 0;
            p[2] = (left && !top) ? 255 : 0;
            if (!left && !top) { p[0] = p[1] = p[2] = 255; }
            p[3] = 255;
        }
    }
    const unsigned err = lodepng::encode(path, px, w, h);
    assert(err == 0 && "gagal menulis PNG sementara untuk tes");
}

void testMissingFileFails() {
    std::vector<uint8_t> out;
    const bool ok = icon_loader::loadResized("sdmc:/tidak-ada/berkas-ini.png", 64, out);
    assert(!ok);
    assert(out.empty());
}

void testResizeUpAndDown() {
    const std::string small = tempPngPath("tehe_rpg_inko_test_icon_small.png");
    const std::string big   = tempPngPath("tehe_rpg_inko_test_icon_big.png");
    writeQuadPng(small, 8, 8);     // lebih kecil dari canvas: harus diperbesar
    writeQuadPng(big, 300, 300);   // lebih besar dari canvas: harus diperkecil

    for (const auto& path : { small, big }) {
        std::vector<uint8_t> out;
        const bool ok = icon_loader::loadResized(path, 64, out);
        assert(ok);
        assert(out.size() == 64u * 64u * 4u);

        // Kuadran kiri-atas tetap merah, kanan-bawah tetap putih setelah resize.
        const uint8_t* topLeft = &out[(10u * 64u + 10u) * 4u];
        assert(topLeft[0] > 200 && topLeft[1] < 50 && topLeft[2] < 50);
        const uint8_t* bottomRight = &out[(54u * 64u + 54u) * 4u];
        assert(bottomRight[0] > 200 && bottomRight[1] > 200 && bottomRight[2] > 200);
    }
    std::remove(small.c_str());
    std::remove(big.c_str());
}

void testSquareCanvasRegardlessOfAspectRatio() {
    const std::string wide = tempPngPath("tehe_rpg_inko_test_icon_wide.png");
    writeQuadPng(wide, 100, 40);    // rasio bukan 1:1, seperti pemain yang tidak crop
    std::vector<uint8_t> out;
    const bool ok = icon_loader::loadResized(wide, 32, out);
    assert(ok);
    assert(out.size() == 32u * 32u * 4u);
    std::remove(wide.c_str());
}

void testGpuColorsAndAlpha() {
    const std::string path = tempPngPath("tehe_rpg_inko_test_icon_gpu_colors.png");
    // Red, green, blue, and an asymmetric translucent color.
    const std::vector<unsigned char> pixels = {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 17, 83, 201, 64
    };
    assert(lodepng::encode(path, pixels, 2, 2) == 0);
    std::vector<uint8_t> rgba;
    assert(icon_loader::loadResized(path, 2, rgba));
    assert(rgba == pixels);
    std::vector<uint8_t> gpu(rgba.size());
    icon_loader::copyToGpuRgba8(rgba.data(), gpu.data(), rgba.size() / 4);
    // GPU_RGBA8 packed words are 0xRRGGBBAA; read explicitly little-endian.
    const uint32_t expected[] = {0xff0000ffu, 0x00ff00ffu, 0x0000ffffu, 0x1153c940u};
    for (std::size_t i = 0; i < 4; ++i) {
        const uint8_t* p = &gpu[i * 4];
        const uint32_t color = uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
                               (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        assert(color == expected[i]);
    }
    // Fully transparent pixels retain zero alpha regardless of their RGB values.
    const uint8_t transparent[] = {19, 91, 207, 0};
    uint8_t converted[4] = {};
    icon_loader::copyToGpuRgba8(transparent, converted, 1);
    assert(converted[0] == 0 && converted[1] == 207 &&
           converted[2] == 91 && converted[3] == 19);
    std::remove(path.c_str());
}

}  // namespace

int main() {
    testGpuColorsAndAlpha();
    testMissingFileFails();
    testResizeUpAndDown();
    testSquareCanvasRegardlessOfAspectRatio();
    std::printf("icon_loader_test: semua tes lulus\n");
    return 0;
}
