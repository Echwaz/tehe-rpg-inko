#include "icon_loader.h"

#include "third_party/lodepng.h"

namespace icon_loader {
namespace {

// Nearest-neighbor sampling: cukup untuk ikon kecil (128x128 atau lebih kecil),
// dan menghindari perlu library resize terpisah. sw/sh > 0 sudah dijamin pemanggil.
void resizeNearest(const std::vector<uint8_t>& src, unsigned sw, unsigned sh,
                    int dst, std::vector<uint8_t>& out) {
    out.assign(static_cast<size_t>(dst) * dst * 4, 0);
    for (int y = 0; y < dst; ++y) {
        const unsigned sy = static_cast<unsigned>((static_cast<uint64_t>(y) * sh) / dst);
        for (int x = 0; x < dst; ++x) {
            const unsigned sx = static_cast<unsigned>((static_cast<uint64_t>(x) * sw) / dst);
            const uint8_t* s = &src[(static_cast<size_t>(sy) * sw + sx) * 4];
            uint8_t* d = &out[(static_cast<size_t>(y) * dst + x) * 4];
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            d[3] = s[3];
        }
    }
}

}  // namespace

bool loadResized(const std::string& path, int canvasSize, std::vector<uint8_t>& outRgba) {
    outRgba.clear();
    if (canvasSize <= 0) return false;

    std::vector<uint8_t> raw;
    unsigned w = 0, h = 0;
    // lodepng::decode baca dari path lewat stdio biasa: jalan untuk "sdmc:/..."
    // (kartu SD) di 3DS maupun path biasa saat dites di PC.
    const unsigned err = lodepng::decode(raw, w, h, path);
    if (err != 0 || w == 0 || h == 0) return false;   // berkas tidak ada / bukan PNG valid

    resizeNearest(raw, w, h, canvasSize, outRgba);
    return true;
}

void copyToGpuRgba8(const uint8_t* rgba, uint8_t* abgr, std::size_t pixelCount) {
    for (std::size_t i = 0; i < pixelCount; ++i) {
        abgr[4 * i    ] = rgba[4 * i + 3];
        abgr[4 * i + 1] = rgba[4 * i + 2];
        abgr[4 * i + 2] = rgba[4 * i + 1];
        abgr[4 * i + 3] = rgba[4 * i    ];
    }
}

}  // namespace icon_loader
