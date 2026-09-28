#pragma once

// Pembaca Ogg Vorbis yang mengulang dari awal saat file habis (musik latar).
// Hanya bergantung pada libvorbisidec (Tremor) dan stdio, tanpa API 3DS, jadi logikanya bisa
// dites di PC. Keluaran: PCM 16-bit interleaved, mono atau stereo.

#include <cstdint>
#include <cstdio>

#include <tremor/ivorbisfile.h>

class LoopingOgg {
public:
    LoopingOgg() = default;
    ~LoopingOgg() { close(); }
    LoopingOgg(const LoopingOgg&) = delete;
    LoopingOgg& operator=(const LoopingOgg&) = delete;

    bool open(const char* path);       // false bila file tidak ada, rusak, atau bukan mono/stereo
    void close();
    bool isOpen() const { return open_; }
    int  channels() const { return channels_; }
    long rate() const { return rate_; }

    // Mengisi 'out' dengan sampai maxFrames frame (satu frame = satu sampel per kanal). Kalau file
    // habis, otomatis mengulang dari awal. Hasilnya kurang dari maxFrames hanya bila terjadi galat.
    int read(int16_t* out, int maxFrames);

    // Total frame di seluruh file. Tidak dipakai untuk BGM (yang streaming), tapi berguna untuk
    // pra-muat SFX pendek sekali putar ke memori penuh sebelum diputar. -1 kalau tidak diketahui.
    long totalFrames() const;

private:
    OggVorbis_File vf_;
    bool open_     = false;
    int  channels_ = 0;
    long rate_     = 0;
};
