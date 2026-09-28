#pragma once

// Efek suara pendek sekali putar untuk battle. Beda dari audio:: (BGM streaming, kanal 0): tiap
// klip didekode penuh ke memori saat init, lalu diputar lewat kanal NDSP 1..3, jadi bisa tumpang
// tindih dengan BGM dan sesama SFX. Kalau kanal penuh, SFX baru menimpa yang paling lama dipakai.
// Berkas dicari di romfs:/sfx/*.ogg. Id yang berkasnya tidak ada atau formatnya salah dilewati
// diam-diam, jadi play() untuk id itu tanpa efek, bukan crash.

namespace sfx {

enum class Id {
    HitMelee,    // senjata jarak dekat kena bos
    HitRanged,   // peluru/anak panah jarak jauh (Yuki, Tsukasa) kena bos
    AllyHit,     // sekutu kena serang musuh
    Heal,        // DP tim pulih (mis. Resupply)
    Break,       // bos BREAK!
    Stun,        // musuh kena stun
    DefeatPressure, // cutscene kekalahan mulai
    DefeatShatter,  // pecahan bos meledak
    DefeatRoots,    // pohon putih muncul
    DefeatSettle,   // debu dan ekor rendah
    Count
};

bool init(bool dspReady);   // panggil setelah audio::init(); dspReady = hasil kembalian audio::init()
void shutdown();
void play(Id id);           // tanpa efek kalau mode senyap atau berkas sfx ini tidak ada

}  // namespace sfx
