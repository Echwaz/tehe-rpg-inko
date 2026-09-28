#pragma once
#include <cstddef>
#include <cstdint>
using s16 = std::int16_t;
using u32 = std::uint32_t;
enum { NDSP_INTERP_LINEAR, NDSP_FORMAT_MONO_PCM16, NDSP_FORMAT_STEREO_PCM16 };
struct ndspWaveBuf {
    s16* data_pcm16;
    u32 nsamples;
    bool looping;
};
void* linearAlloc(std::size_t);
void linearFree(void*);
void DSP_FlushDataCache(void*, u32);
void ndspChnReset(int);
bool ndspChnIsPlaying(int);
void ndspChnSetInterp(int, int);
void ndspChnSetRate(int, float);
void ndspChnSetFormat(int, int);
void ndspChnSetMix(int, float*);
void ndspChnWaveBufAdd(int, ndspWaveBuf*);
