#pragma once
#include <algorithm>
#include <cmath>

namespace defeat_cutscene {
constexpr int Burst = 54;
constexpr int TreeStart = 108;
constexpr int Duration = 420;
constexpr int Settle = 300;
enum SoundCue { PressureCue = 1, ShatterCue = 2, RootsCue = 4, SettleCue = 8 };
inline unsigned soundCues(int previous, int current) {
    if (current < 0 || current <= previous) return 0;
    unsigned cues = 0;
    if (previous < 0 && current >= 0) cues |= PressureCue;
    if (previous < Burst && current >= Burst) cues |= ShatterCue;
    if (previous < TreeStart && current >= TreeStart) cues |= RootsCue;
    if (previous < Settle && current >= Settle) cues |= SettleCue;
    return cues;
}
inline float ramp(float f, float a, float b) {
    const float t = std::max(0.f, std::min(1.f, (f - a) / (b - a)));
    return t * t * (3.f - 2.f * t);
}
struct Pose {
    float camera = 0.f, scatter = 0.f, growth = 0.f, flash = 0.f, shake = 0.f;
    bool fragments = false, tree = false;
};
inline Pose sample(int frame) {
    Pose p;
    if (frame < 0) return p;
    const float f = static_cast<float>(frame);
    p.camera = ramp(f, 0, 70);
    p.fragments = frame >= Burst && frame < 156;
    p.scatter = ramp(f, Burst, 156);
    p.tree = frame >= TreeStart;
    p.growth = ramp(f, TreeStart, 300);
    p.flash = ramp(f, Burst - 8, Burst) * (1.f - ramp(f, Burst, Burst + 20));
    p.shake = std::sin(f * 2.1f) * 0.045f *
              (ramp(f, 15, Burst) * (1.f - ramp(f, Burst, 100)) +
               ramp(f, TreeStart, 140) * (1.f - ramp(f, 180, 260)));
    return p;
}
struct CameraPose {
    float x, y, z, targetX, targetY, targetZ, fov;
};
inline CameraPose camera(int frame) {
    const Pose p = sample(frame);
    const float wide = ramp(static_cast<float>(frame), 200, 340);
    // Tetap di halaman yang lapang. Zoom out secara optik, jangan mundur menembus gedung.
    return {-1.8f + 1.8f * p.camera + p.shake, 1.55f + 1.05f * wide, 7.3f,
            -1.8f * (1.f - p.camera), 2.05f + 1.15f * p.growth, 0.5f, 50.f + 10.f * wide};
}
} // namespace defeat_cutscene
