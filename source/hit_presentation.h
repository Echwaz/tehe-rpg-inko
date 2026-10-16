#pragma once
#include <algorithm>
#include <string>

namespace hit_presentation {
enum class Style { Regular, Cascade, Escapade };
constexpr int HitGap = 9;
constexpr int FinishRecovery = 8;

inline Style select(const std::string& characterKey, int skillIndex) {
    if (skillIndex != 0) return Style::Regular;
    if (characterKey == "Ruka") return Style::Cascade;
    if (characterKey == "Karen") return Style::Escapade;
    return Style::Regular;
}

inline int delay(Style style, int hit, int plannedHits) {
    if (style == Style::Regular) return hit * HitGap;
    if (plannedHits <= 1) return 0;
    static const int cascade[] = {0, 11, 21, 30, 38, 45, 51, 56, 72};
    static const int escapade[] = {0, 7, 14, 21, 28, 35, 42, 49, 56, 81};
    const int* beats = style == Style::Cascade ? cascade : escapade;
    const int last = style == Style::Cascade ? 8 : 9;
    hit = std::max(0, std::min(hit, plannedHits - 1));
    // Resample the rhythm when Custom Party changes the hit count. Full combos
    // keep their original total hit span; early kills never wait for missing hits.
    const int position = hit * last;
    const int low = position / (plannedHits - 1);
    if (low == last) return (plannedHits - 1) * HitGap;
    const int fraction = position % (plannedHits - 1);
    const int beat = beats[low] * (plannedHits - 1) +
                     (beats[low + 1] - beats[low]) * fraction;
    return beat * HitGap / beats[last];
}

struct Impact {
    float pitch, gain, recoil, slashScale;
    int recoilFrames;
};

inline Impact impact(Style style, bool finisher) {
    if (style == Style::Regular) return {1.f, 1.f, 1.f, 1.f, 10};
    if (finisher) {
        if (style == Style::Cascade) return {0.80f, 1.f, 1.45f, 1.30f, 16};
        return {0.74f, 1.f, 1.60f, 1.40f, 18};
    }
    if (style == Style::Cascade) return {1.10f, 0.72f, 0.65f, 0.90f, 8};
    return {0.98f, 0.68f, 0.60f, 0.85f, 7};
}
}
