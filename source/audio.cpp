#include "audio.h"

#include <3ds.h>

#include <cstring>

#include "bgm_decoder.h"

namespace audio {
namespace {

// Lokasi lagu di kartu SD, disediakan PEMAIN sendiri (tidak ditanam ke .3dsx/.cia).
// Urutannya mengikuti enum Track. Folder sama dengan ikon karakter, lihat assets.cpp.
const char* const kPaths[] = { nullptr, "sdmc:/3ds/Tehe-RPG-inko/assets/bgm_dialogue.ogg",
                                          "sdmc:/3ds/Tehe-RPG-inko/assets/bgm_battle.ogg",
                                          "sdmc:/3ds/Tehe-RPG-inko/assets/bgm_awaken.ogg" };

const float kMasterVolume = 0.8f;
const float kFadeInMs     = 600.f;
const float kFadeOutMs    = 400.f;

const int    kChannel      = 0;
const int    kNumBufs      = 4;
const int    kFramesPerBuf = 4096;     // frame per buffer (kira-kira 0,13 detik pada 32 kHz)
const size_t kBufBytes     = kFramesPerBuf * 2 * sizeof(s16);   // ukuran terbesar: stereo
const size_t kStackBytes   = 64 * 1024;

enum class Fade { None, In, Out };

// Data thread audio (hanya disentuh thread audio, kecuali yang bertanda volatile)
LoopingOgg     g_dec;
ndspWaveBuf    g_wb[kNumBufs];
s16*           g_pcm[kNumBufs] = {};
Track          g_cur      = Track::None;   // lagu yang terbuka (atau yang gagal dibuka, supaya tidak dicoba terus)
Fade           g_fade     = Fade::None;

u64            g_fadeAt   = 0;
float          g_gain     = -1.f;          // volume yang terakhir dikirim ke NDSP

// Dibagi antar thread
volatile int   g_req  = 0;
volatile bool  g_quit = false;
LightEvent     g_event;
Thread         g_thread = nullptr;
bool           g_ready  = false;           // ndspInit sudah berhasil dan belum di-shutdown

void onDspFrame(void*) {
    if (!g_quit) LightEvent_Signal(&g_event);
}

void setGain(float g) {
    if (g == g_gain) return;
    g_gain = g;
    float mix[12];
    std::memset(mix, 0, sizeof mix);
    mix[0] = g;
    mix[1] = g;
    ndspChnSetMix(kChannel, mix);
}

void closeTrack() {
    if (g_dec.isOpen()) {
        ndspChnReset(kChannel);
        svcSleepThread(20000000LL);        // beri DSP satu-dua frame untuk melepas buffer lama
        g_dec.close();
    }
    g_cur  = Track::None;
    g_fade = Fade::None;
    g_gain = -1.f;
}

bool startTrack(Track t, u64 now) {
    if (!g_dec.open(kPaths[static_cast<int>(t)])) return false;
    ndspChnReset(kChannel);
    ndspChnSetInterp(kChannel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(kChannel, static_cast<float>(g_dec.rate()));
    ndspChnSetFormat(kChannel, g_dec.channels() == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    g_gain = -1.f;
    setGain(0.f);                          // mulai senyap, naik lewat fade-in
    for (int i = 0; i < kNumBufs; ++i) std::memset(&g_wb[i], 0, sizeof g_wb[i]);
    g_fade   = Fade::In;
    g_fadeAt = now;
    return true;
}

// Fade dan pergantian lagu. Dipanggil tiap putaran thread audio.
void tick() {
    const Track want = static_cast<Track>(g_req);
    const u64   now  = osGetTime();

    if (g_dec.isOpen() && g_fade == Fade::Out) {
        const float t = static_cast<float>(now - g_fadeAt) / kFadeOutMs;
        if (t < 1.f) {
            setGain(kMasterVolume * (1.f - t));
            return;
        }
        closeTrack();
    }

    if (!g_dec.isOpen()) {
        if (want != g_cur) {
            g_cur = want;                  // dicatat dulu: kalau gagal dibuka, tidak dicoba tiap putaran
            if (want != Track::None) startTrack(want, now);
        }
        return;
    }

    if (want != g_cur) {
        g_fade   = Fade::Out;
        g_fadeAt = now;
        return;
    }

    if (g_fade == Fade::In) {
        float t = static_cast<float>(now - g_fadeAt) / kFadeInMs;
        if (t >= 1.f) { t = 1.f; g_fade = Fade::None; }
        setGain(kMasterVolume * t);
    }
}

void refill() {
    for (int i = 0; i < kNumBufs; ++i) {
        ndspWaveBuf& wb = g_wb[i];
        if (wb.status != NDSP_WBUF_FREE && wb.status != NDSP_WBUF_DONE) continue;

        const int frames = g_dec.read(g_pcm[i], kFramesPerBuf);
        if (frames <= 0) {
            const Track dead = g_cur;
            closeTrack();
            g_cur = dead;
            return;
        }
        wb.data_pcm16 = g_pcm[i];
        wb.nsamples   = static_cast<u32>(frames);
        wb.looping    = false;
        DSP_FlushDataCache(g_pcm[i], static_cast<u32>(frames) * g_dec.channels() * sizeof(s16));
        ndspChnWaveBufAdd(kChannel, &wb);
    }
}

void audioThread(void*) {
    while (!g_quit) {
        tick();
        if (g_dec.isOpen()) refill();
        LightEvent_WaitTimeout(&g_event, 20000000LL);   // bangun tiap frame DSP, atau paling lama 20 ms
    }
}

}  // namespace

bool init() {
    if (g_ready) return true;
    const Result rc = ndspInit();
    if (R_FAILED(rc)) return false;                  // tanpa firmware DSP (dspfirm.cdc): mode senyap
    g_ready = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    for (int i = 0; i < kNumBufs; ++i) {
        g_pcm[i] = static_cast<s16*>(linearAlloc(kBufBytes));
        if (!g_pcm[i]) { shutdown(); return false; }
        std::memset(&g_wb[i], 0, sizeof g_wb[i]);
    }

    LightEvent_Init(&g_event, RESET_ONESHOT);
    g_quit = false;
    g_req  = static_cast<int>(Track::None);
    ndspSetCallback(onDspFrame, nullptr);

    // Dekode di core sistem (core 1) supaya tidak mengganggu thread utama. Di 3DS lama ini butuh
    // jatah waktu CPU dari sistem; kalau gagal, jatuh ke core aplikasi dengan prioritas lebih rendah.
    APT_SetAppCpuTimeLimit(30);
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    g_thread = threadCreate(audioThread, nullptr, kStackBytes, prio - 1, 1, false);
    if (!g_thread) g_thread = threadCreate(audioThread, nullptr, kStackBytes, prio + 1, 0, false);
    if (!g_thread) { shutdown(); return false; }
    return true;
}

void shutdown() {
    if (!g_ready) return;
    g_ready = false;
    if (g_thread) {
        g_quit = true;
        LightEvent_Signal(&g_event);
        threadJoin(g_thread, U64_MAX);
        threadFree(g_thread);
        g_thread = nullptr;
    }
    g_dec.close();
    ndspChnReset(kChannel);
    for (int i = 0; i < kNumBufs; ++i) {
        if (g_pcm[i]) { linearFree(g_pcm[i]); g_pcm[i] = nullptr; }
    }
    ndspExit();
}

void play(Track t) {
    if (!g_thread) return;
    g_req = static_cast<int>(t);
    LightEvent_Signal(&g_event);
}

}  // namespace audio
