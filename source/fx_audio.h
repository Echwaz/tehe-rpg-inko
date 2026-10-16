#pragma once
#include "combat.h"
#include "sfx.h"

namespace fx_audio {
inline void play(const FxEvent& e, bool ranged) {
    const auto accent = hit_presentation::impact(e.style, e.finisher);
    switch (e.kind) {
    case FxKind::Damage:
        // The simultaneous Break owns the accent, avoiding two loud transients.
        if (e.finisher && e.breaks_target) return;
        sfx::play(e.on_enemy ? (ranged ? sfx::Id::HitRanged : sfx::Id::HitMelee) : sfx::Id::AllyHit,
                  accent.pitch, accent.gain);
        break;
    case FxKind::Break: sfx::play(sfx::Id::Break, e.finisher ? 0.88f : 1.f); break;
    case FxKind::Heal:  sfx::play(sfx::Id::Heal); break;
    case FxKind::Stun:  sfx::play(sfx::Id::Stun); break;
    }
}
}
