#pragma once
#include <algorithm>
#include <cmath>

// Timeline deterministik bersama: 5 detik pada update 60 Hz.
namespace awakening {
constexpr int Duration = 300;
constexpr int Reveal = 156;
inline float ramp(float frame, float begin, float end) {
    float t = std::max(0.f, std::min(1.f, (frame - begin) / (end - begin)));
    return t * t * (3.f - 2.f * t);
}
struct Pose {
    float camera = 0.f, orbit = 0.f, height = 1.f, tilt = 0.f;
    float energy = 0.f, flash = 0.f, shake = 0.f;
    bool revealed = false;
};
inline Pose sample(int frame) {
    Pose p;
    if (frame < 0) return p;
    float f = static_cast<float>(frame);
    p.camera = ramp(f, 0, 50) * (1.f - ramp(f, 246, Duration));
    p.orbit = std::sin(f * 0.012f) * p.camera;
    p.revealed = frame >= Reveal;
    p.height = p.revealed ? 0.35f + 0.65f * ramp(f, Reveal, 205)
                          : 1.f - 0.65f * ramp(f, 0, 48);
    p.tilt = p.revealed ? 0.f : -0.16f * ramp(f, 0, 48);
    p.energy = ramp(f, 66, Reveal) * (1.f - ramp(f, 200, 242));
    p.flash = p.revealed ? 1.f - ramp(f, Reveal, Reveal + 18) : 0.f;
    p.shake = p.energy * 0.035f * std::sin(f * 2.3f);
    return p;
}
} // namespace awakening
