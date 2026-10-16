#include "icon_loader.h"
#include <cstdio>

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
    if (canvasSize <= 0 || static_cast<unsigned>(canvasSize) > kMaxSourceDimension) return false;

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    if (std::fseek(f, 0, SEEK_END) != 0) { std::fclose(f); return false; }
    const long bytes = std::ftell(f);
    if (bytes < 33 || static_cast<unsigned long>(bytes) > kMaxFileBytes ||
        std::fseek(f, 0, SEEK_SET) != 0) { std::fclose(f); return false; }
    std::vector<unsigned char> encoded(static_cast<std::size_t>(bytes));
    const bool readOk = std::fread(encoded.data(), 1, encoded.size(), f) == encoded.size();
    std::fclose(f);
    if (!readOk) return false;

    std::vector<uint8_t> raw;
    unsigned w = 0, h = 0;
    lodepng::State header;
    if (lodepng_inspect(&w, &h, &header, encoded.data(), encoded.size()) != 0 ||
        w == 0 || h == 0 || w > kMaxSourceDimension || h > kMaxSourceDimension) return false;
    lodepng::State decoder;
    decoder.decoder.read_text_chunks = 0;
    decoder.decoder.max_icc_size = 4096;
    const unsigned err = lodepng::decode(raw, w, h, decoder, encoded);
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
