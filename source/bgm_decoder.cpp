#include "bgm_decoder.h"

bool LoopingOgg::open(const char* path) {
    close();
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    if (ov_open(f, &vf_, nullptr, 0) < 0) {       // gagal: FILE belum dimiliki Tremor, tutup sendiri
        std::fclose(f);
        return false;
    }
    const vorbis_info* vi = ov_info(&vf_, -1);
    if (!vi || vi->channels < 1 || vi->channels > 2 || vi->rate <= 0) {
        ov_clear(&vf_);                           // ov_clear ikut menutup FILE
        return false;
    }
    channels_ = vi->channels;
    rate_     = vi->rate;
    open_     = true;
    return true;
}

void LoopingOgg::close() {
    if (!open_) return;
    ov_clear(&vf_);
    open_ = false;
    channels_ = 0;
    rate_ = 0;
}

int LoopingOgg::read(int16_t* out, int maxFrames) {
    if (!open_ || maxFrames <= 0) return 0;
    const int frameBytes = channels_ * static_cast<int>(sizeof(int16_t));
    const int wantBytes  = maxFrames * frameBytes;
    int got = 0, stalls = 0;
    while (got < wantBytes) {
        int bitstream = 0;
        const long r = ov_read(&vf_, reinterpret_cast<char*>(out) + got, wantBytes - got, &bitstream);
        if (r > 0) {
            got += static_cast<int>(r);
            stalls = 0;
            continue;
        }
        if (r == 0) {                             // akhir file: ulang dari awal
            // Dua akhir file berturut-turut tanpa data di antaranya berarti file kosong: menyerah.
            if (stalls++ > 0 || ov_pcm_seek(&vf_, 0) != 0) break;
            continue;
        }
        if (r == OV_HOLE && ++stalls < 8) continue;   // data rusak sebentar: lewati
        break;
    }
    return got / frameBytes;
}

long LoopingOgg::totalFrames() const {
    if (!open_) return -1;
    const ogg_int64_t t = ov_pcm_total(const_cast<OggVorbis_File*>(&vf_), -1);
    return t < 0 ? -1 : static_cast<long>(t);
}
