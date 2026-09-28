#include <3ds.h>
#include <citro2d.h>

#include <cstdlib>
#include <ctime>

#include "assets.h"
#include "audio.h"
#include "battle3d.h"
#include "screens.h"
#include "sfx.h"
#include "ui.h"

enum class GameState { Dialogue, Battle };

int main(int /*argc*/, char** /*argv*/) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    // Layar atas berbagi depth target antara scene 3D dan HUD 2D.
    C3D_RenderTarget* top    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    TextRenderer text;
    text.init();
    // Kegagalan init di bawah tidak fatal: tanpa lagu game jalan senyap, tanpa gambar dipakai
    // kotak berwarna, tanpa 3D layar atas dikosongkan.
    romfsInit();
    assets::init();
    battle3d::init();
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    TouchState     touch;
    DialogueScreen dialogue;
    BattleScreen   battle;
    GameState      state = GameState::Dialogue;
    BattlePhase    prevBattlePhase = BattlePhase::Planning;
    const bool audioReady = audio::init();
    sfx::init(audioReady);
    dialogue.enter();
    audio::play(audio::Track::Dialogue);

    while (aptMainLoop()) {
        hidScanInput();
        const u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;
        touch.update();
        const u32 kHeld = hidKeysHeld();

        switch (state) {
        case GameState::Dialogue:
            dialogue.update(touch, kDown);
            if (dialogue.done()) {
                battle.enter();
                state = GameState::Battle;
                prevBattlePhase = battle.phase();
                audio::play(audio::Track::Battle);
            }
            break;
        case GameState::Battle:
            battle.update(touch, kDown, kHeld);
            {
                // Begitu masuk fase Awakening, Track::Awaken terus main sebagai tema bos fase 2
                // sampai fase Dying. Tidak ada jalur balik ke Track::Battle.
                const BattlePhase curPhase = battle.phase();
                if (curPhase != prevBattlePhase) {
                    if (curPhase == BattlePhase::Awakening) audio::play(audio::Track::Awaken);
                    // BGM di-fade saat hit terakhir, sebelum penghancuran. SFX tetap aktif.
                    else if (curPhase == BattlePhase::Dying) audio::play(audio::Track::None);
                    prevBattlePhase = curPhase;
                }
            }
            if (battle.wantsRestart()) {
                dialogue.enter();
                state = GameState::Dialogue;
                audio::play(audio::Track::Dialogue);
            }
            break;
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        text.beginFrame();

        const bool scene3d = state == GameState::Dialogue
                                 ? dialogue.drawScene3d(top)
                                 : battle.drawScene3d(top);
        if (scene3d) {
            // Selesaikan perintah 3D dulu, lalu bersihkan HANYA depth untuk HUD.
            // Membersihkan warna di sini akan menghapus arena dan bos.
            C3D_FrameSplit(0);
            C3D_RenderTargetClear(top, C3D_CLEAR_DEPTH, 0, 0);
        } else {
            C2D_TargetClear(top, colors::panel);
        }
        C2D_Prepare();
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                       GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
        C2D_SceneBegin(top);
        if (state == GameState::Dialogue) dialogue.drawTop(text);
        else                              battle.drawTop(text, scene3d);

        C2D_TargetClear(bottom, colors::panel);
        C2D_SceneBegin(bottom);
        if (state == GameState::Dialogue) dialogue.drawBottom(text);
        else                              battle.drawBottom(text);

        C3D_FrameEnd(0);
    }

    // FrameBegin ikut menunggu antrean GPU (termasuk salinan snapshot) sebelum resource dibebaskan.
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C3D_FrameEnd(0); // frame kosong: tidak ada referensi model/tekstur/snapshot
    battle3d::shutdown();
    sfx::shutdown();   // dimatikan dulu sebelum audio::shutdown() melepas ndsp sepenuhnya
    audio::shutdown();
    assets::shutdown();
    text.shutdown();
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    gfxExit();
    return 0;
}
