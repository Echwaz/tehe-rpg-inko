#pragma once
#include "defeat_cutscene.h"

// Kunci cache hanya mencakup kamera arena yang statis, tidak pernah aktor beranimasi.
namespace cinematic_view {
enum class Shot { Battle, Prologue, Closeup, TreeWide, Moving };
inline Shot select(bool old3ds, bool prologue, int awakeningFrame, int defeatFrame) {
    if (prologue) return Shot::Prologue;
    if (defeatFrame >= 0) {
        if (old3ds)
            return defeatFrame < defeat_cutscene::TreeStart ? Shot::Closeup : Shot::TreeWide;
        // Kamera bergerak sudah benar-benar diam di frame 340, termasuk getarannya.
        return defeatFrame >= 340 ? Shot::TreeWide : Shot::Moving;
    }
    if (awakeningFrame >= 0) return old3ds ? Shot::Closeup : Shot::Moving;
    return Shot::Battle;
}
inline bool cacheable(Shot shot) { return shot != Shot::Moving; }
} // namespace cinematic_view
