#include <3ds.h>
#include "../source/sfx.h"
#include "../source/fx_audio.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <map>

static std::map<void*, std::size_t> allocations;
static std::map<void*, u32> flushed;
static float rates[4] = {};
static int formats[4] = {};
static int queued = 0;
static float gains[4] = {};
void* linearAlloc(std::size_t n) {
    void* p = std::malloc(n);
    assert(p);
    allocations[p] = n;
    return p;
}
void linearFree(void* p) {
    assert(allocations.erase(p) == 1);
    flushed.erase(p);
    std::free(p);
}
void DSP_FlushDataCache(void* p, u32 bytes) {
    assert(allocations.count(p) && bytes > 0 && bytes <= allocations.at(p));
    flushed[p] = bytes;
}
static void channel(int ch) { assert(ch >= 1 && ch <= 3); } // never reset BGM channel 0
void ndspChnReset(int ch) { channel(ch); rates[ch] = 0; }
bool ndspChnIsPlaying(int ch) { channel(ch); return false; }
void ndspChnSetInterp(int ch, int interp) { channel(ch); assert(interp == NDSP_INTERP_LINEAR); }
void ndspChnSetRate(int ch, float rate) {
    channel(ch);
    assert(rate > 0);
    rates[ch] = rate;
}
void ndspChnSetFormat(int ch, int format) { channel(ch); formats[ch] = format; }
void ndspChnSetMix(int ch, float* mix) {
    channel(ch);
    assert(mix[0] >= 0 && mix[0] <= 0.9f && mix[1] == mix[0]);
    gains[ch] = mix[0];
}
void ndspChnWaveBufAdd(int ch, ndspWaveBuf* wb) {
    channel(ch);
    assert(rates[ch] > 0 && wb->nsamples > 0 && !wb->looping);
    assert(formats[ch] == NDSP_FORMAT_MONO_PCM16 || formats[ch] == NDSP_FORMAT_STEREO_PCM16);
    const int channels = formats[ch] == NDSP_FORMAT_STEREO_PCM16 ? 2 : 1;
    const std::size_t samples = wb->nsamples * channels;
    assert(flushed.at(wb->data_pcm16) == samples * sizeof(s16));
    bool audible = false;
    for (std::size_t i = 0; i < samples; ++i) audible |= wb->data_pcm16[i] != 0;
    assert(audible);
    ++queued;
}
int main() {
    assert(!sfx::init(false));
    sfx::play(sfx::Id::HitMelee);
    assert(queued == 0 && allocations.empty());
    assert(sfx::init(true));
    const int count = static_cast<int>(sfx::Id::Count);
    assert(allocations.size() == static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        sfx::play(static_cast<sfx::Id>(i));
        assert(queued == i + 1);
    }
    FxEvent e;
    e.kind = FxKind::Damage;
    e.on_enemy = true;
    e.style = hit_presentation::Style::Cascade;
    fx_audio::play(e, false);
    const float normalRate = rates[1], normalGain = gains[1];
    e.finisher = true;
    fx_audio::play(e, false);
    assert(rates[1] < normalRate && gains[1] > normalGain && gains[1] <= 0.9f);
    int before = queued;
    e.breaks_target = true;
    fx_audio::play(e, false);
    assert(queued == before); // final-hit sound is replaced by the simultaneous Break
    e.kind = FxKind::Break;
    fx_audio::play(e, false);
    assert(queued == before + 1 && gains[1] <= 0.9f);
    sfx::play(sfx::Id::HitMelee, 100.f, 100.f);
    assert(gains[1] == 0.9f);
    const float cappedHighRate = rates[1];
    sfx::play(sfx::Id::HitMelee, 0.1f, 0.f);
    assert(rates[1] == cappedHighRate * 0.25f && gains[1] == 0.f);
    const int finalQueued = queued;
    sfx::shutdown();
    assert(allocations.empty() && flushed.empty());
    sfx::play(sfx::Id::HitMelee);
    assert(queued == finalQueued);
    std::puts("PASS: real Ogg SFX, valid buffers, bounded accents, and a single combined Break/finisher sound");
}
