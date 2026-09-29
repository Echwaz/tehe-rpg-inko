#include "sfx.h"

#include <3ds.h>

#include <cstddef>
#include <cstring>

#include "bgm_decoder.h"

namespace sfx {
namespace {

// Urutan harus sejajar dengan enum Id.
const char* const kPaths[] = {
    "romfs:/sfx/hit_melee.ogg",
    "romfs:/sfx/hit_ranged.ogg",
    "romfs:/sfx/ally_hit.ogg",
    "romfs:/sfx/heal.ogg",
    "romfs:/sfx/break.ogg",
    "romfs:/sfx/stun.ogg",
    "romfs:/sfx/defeat_pressure.ogg",
    "romfs:/sfx/defeat_shatter.ogg",
    "romfs:/sfx/defeat_roots.ogg",
    "romfs:/sfx/defeat_settle.ogg",
    "romfs:/sfx/overdrive.ogg",
};
constexpr int kCount = static_cast<int>(Id::Count);
static_assert(sizeof(kPaths) / sizeof(kPaths[0]) == kCount, "kPaths harus sejajar dengan enum Id");

const float kVolume        = 0.9f;
const int   kFirstChannel  = 1;      // kanal 0 dipakai audio:: (musik latar)
const int   kNumChannels   = 3;      // SFX yang tumpang tindih lebih dari ini menimpa yang tertua
const long  kMaxClipFrames = 48000 * 10;   // jaga-jaga: tolak file "SFX" yang ternyata sangat panjang

struct Clip {
    s16* pcm      = nullptr;
    int  frames   = 0;
    int  channels = 0;
    int  rate     = 0;
    bool loaded   = false;
};

Clip        g_clips[kCount];
ndspWaveBuf g_wb[kNumChannels];
bool        g_ready  = false;
int         g_nextCh = 0;   // giliran round-robin kalau semua kanal di kolam sedang sibuk

bool loadClip(const char* path, Clip& out) {
    LoopingOgg dec;
    if (!dec.open(path)) return false;               // berkas tidak ada / bukan ogg valid: lewati diam-diam
    const long total = dec.totalFrames();
    if (total <= 0 || total > kMaxClipFrames) { dec.close(); return false; }

    // close() mereset metadata decoder, jadi format PCM disimpan selagi decoder masih terbuka.
    const int channels = dec.channels();
    const int rate = static_cast<int>(dec.rate());
    s16* buf = static_cast<s16*>(
        linearAlloc(static_cast<size_t>(total) * channels * sizeof(s16)));
    if (!buf) { dec.close(); return false; }

    const int got = dec.read(buf, static_cast<int>(total));
    dec.close();
    if (got <= 0) { linearFree(buf); return false; }

    DSP_FlushDataCache(buf, static_cast<u32>(got) * channels * sizeof(s16));
    out.pcm      = buf;
    out.frames   = got;
    out.channels = channels;
    out.rate     = rate;
    out.loaded   = true;
    return true;
}

}  // namespace

bool init(bool dspReady) {
    if (!dspReady) return false;   // audio:: sudah mode senyap; jangan sentuh kanal NDSP sama sekali
    for (int i = 0; i < kCount; ++i) loadClip(kPaths[i], g_clips[i]);
    for (int i = 0; i < kNumChannels; ++i) std::memset(&g_wb[i], 0, sizeof g_wb[i]);
    g_ready = true;
    return true;
}

void shutdown() {
    if (!g_ready) return;
    for (int i = 0; i < kNumChannels; ++i) ndspChnReset(kFirstChannel + i);
    for (int i = 0; i < kCount; ++i) {
        if (g_clips[i].pcm) { linearFree(g_clips[i].pcm); g_clips[i].pcm = nullptr; }
        g_clips[i].loaded = false;
    }
    g_ready = false;
}

void play(Id id) {
    if (!g_ready) return;
    const Clip& c = g_clips[static_cast<int>(id)];
    if (!c.loaded) return;

    int ch = -1;
    for (int i = 0; i < kNumChannels; ++i) {
        if (!ndspChnIsPlaying(kFirstChannel + i)) { ch = kFirstChannel + i; break; }
    }
    if (ch < 0) {                              // semua kanal sibuk: timpa giliran paling lama
        ch = kFirstChannel + g_nextCh;
        g_nextCh = (g_nextCh + 1) % kNumChannels;
    }

    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, static_cast<float>(c.rate));
    ndspChnSetFormat(ch, c.channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float mix[12];
    std::memset(mix, 0, sizeof mix);
    mix[0] = kVolume;
    mix[1] = kVolume;
    ndspChnSetMix(ch, mix);

    ndspWaveBuf& wb = g_wb[ch - kFirstChannel];
    std::memset(&wb, 0, sizeof wb);
    wb.data_pcm16 = c.pcm;
    wb.nsamples   = static_cast<u32>(c.frames);
    wb.looping    = false;
    ndspChnWaveBufAdd(ch, &wb);
}

}  // namespace sfx
