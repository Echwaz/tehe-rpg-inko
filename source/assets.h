#pragma once
// Memuat dan menggambar gambar dari gfx/ (ditanam saat build), serta ikon karakter milik pemain
// dari sdmc:/3ds/Tehe-RPG-inko/assets/<nama>_icon.png. Kalau PNG pemain tidak ada atau gagal
// dibaca, dipakai ikon cadangan bawaan (lingkaran + huruf).
// Semua fungsi gambar aman dipanggil meski gambar gagal dimuat: hasilnya false, dan pemanggil
// menggambar warna polos sebagai cadangan.
#include <3ds.h>
#include <citro2d.h>

#include <string>

namespace assets {

enum CharId { kRuka, kYuki, kTama, kKaren, kMegumi, kTsukasa, kCharCount };

bool init();        // panggil setelah C2D_Init. false jika ada sheet yang gagal dimuat.
void shutdown();

// "Ruka Kayamori", "Ruka", "Karrie" -> nomor. -1 jika tidak dikenal.
int charFromName(const std::string& name);

// Ikon karakter, digambar ke kotak (x, y, w, h) dan diskalakan otomatis.
// tint != 0: gambar diwarnai (campuran 50%), dipakai untuk status BROKEN atau meredupkan.
bool drawIcon(int id, float x, float y, float w, float h, float z, u32 tint = 0);

// Latar layar bawah (320x240).
bool drawBottomBg(float z);

}  // namespace assets
