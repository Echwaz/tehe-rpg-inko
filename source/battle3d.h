#pragma once
#include <citro3d.h>

namespace battle3d {
enum class Camera { Battle, Prologue };
// Hanya state visual; aturan battle tetap di combat.cpp.
struct SceneState {
    Camera camera = Camera::Battle;
    int frame = 0;
    int awakeningFrame = -1; // negatif di luar awakening
    int defeatFrame = -1;    // negatif sebelum cutscene kekalahan; ditahan di Duration saat Victory
    float hit = 0.f;
    float attack = 0.f;
    bool awakened = false;
    bool broken = false;
    bool stunned = false;
    bool defeated = false;
};

bool init();
void shutdown();
bool enabled();
// Dipanggil di dalam frame, sebelum objek Citro2D apa pun diantre.
// Memulihkan atau menangkap arena statis (warna + depth), lalu menggambar monster.
// Kalau snapshot VRAM tidak bisa dialokasikan, arena digambar langsung.
// Pemanggil memulihkan Citro2D sesudahnya.
bool renderTop(C3D_RenderTarget* top, const SceneState& state);
// Posisi piksel dada model, diperbarui renderTop untuk efek battle.
void enemyAnchor(float& x, float& y);
} // namespace battle3d
