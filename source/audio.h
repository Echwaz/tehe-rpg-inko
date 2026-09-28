#pragma once

// Musik latar lewat NDSP (DSP 3DS). File dibaca dari kartu SD (disediakan pemain sendiri),
// didekode di thread sendiri, dan ditukar dengan fade. Kalau
// DSP atau filenya tidak ada, game tetap jalan tanpa suara.

namespace audio {

enum class Track { None = 0, Dialogue, Battle, Awaken };

bool init();             // false = mode senyap (mis. dspfirm.cdc belum ada di 3DS asli)
void shutdown();
void play(Track t);      // ganti lagu dengan fade; memanggil lagu yang sedang jalan tidak berpengaruh

}  // namespace audio
