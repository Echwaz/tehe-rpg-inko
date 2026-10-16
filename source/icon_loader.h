#pragma once
// Membaca PNG ikon karakter milik pemain dari kartu SD. Dipisah dari assets.cpp supaya decode +
// resize murni C++ (lodepng) dan bisa dites di PC; upload ke GPU ada di assets.cpp.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace icon_loader {
constexpr unsigned kMaxSourceDimension = 512;
constexpr std::size_t kMaxFileBytes = 2 * 1024 * 1024;

// Memuat PNG dari 'path' (mis. "sdmc:/3ds/Tehe-RPG-inko/assets/ruka_icon.png") lalu mengubah
// ukurannya (nearest-neighbor) menjadi persegi 'canvasSize' x 'canvasSize', RGBA8 dari baris atas,
// urutan byte R,G,B,A. Dimensi sumber/canvas dibatasi 512 dan berkas maksimal 2 MiB.
// Mengembalikan false (dan mengosongkan outRgba) bila berkas tidak ada, bukan PNG valid, atau
// berukuran 0 atau melampaui batas; pemanggil jatuh ke ikon cadangan.
bool loadResized(const std::string& path, int canvasSize, std::vector<uint8_t>& outRgba);

// Menyalin pixelCount piksel RGBA ke buffer terpisah (minimal pixelCount * 4 byte). GPU_RGBA8 dan
// GX_TRANSFER_FMT_RGBA8 di 3DS memakai urutan byte A,B,G,R. Posisi piksel tidak berubah;
// penyusunan tiled dikerjakan display transfer sesudahnya.
void copyToGpuRgba8(const uint8_t* rgba, uint8_t* abgr, std::size_t pixelCount);

}  // namespace icon_loader
