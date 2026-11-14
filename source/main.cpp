#include <3ds.h>
#include <citro2d.h>

#include <cstdlib>
#include <cstdio>
#include <ctime>

#include "assets.h"
#include "audio.h"
#include "battle3d.h"
#include "party_config.h"
#include "screens.h"
#include "sfx.h"
#include "ui.h"

enum class GameState { Dialogue, Customize, Explore, Battle };

// Party kustom (nama karakter, nilai dan efek skill). Dibuat/diubah lewat layar Customize.
static const char* const kPartyPath = "sdmc:/3ds/Tehe-RPG-inko/party.cfg";

static void drawCityTop(TextRenderer& text,const exploration::World& world,bool scene3d) {
    C2D_DrawRectSolid(4,4,kZPanel,190,23,colors::glass);
    text.drawShadow("CITY OUTSKIRTS",10,7,.45f,colors::white);
    if(!scene3d)text.drawCentered("City assets unavailable",200,115,.55f,colors::white);
    else if(world.atBoss()) {
        C2D_DrawRectSolid(65,207,kZPanel,270,26,colors::glass);
        text.drawCentered("A: Enter Hellspider battle",200,211,.45f,colors::pinkLight);
    }
    for(const auto& e:world.enemies)if(e.alive&&e.chasing) {
        text.drawRight("SKITTER ALERT",390,8,.4f,colors::pinkLight);break;
    }
}
static void drawCityBottom(TextRenderer& text,const exploration::World& world) {
    assets::drawBottomBg(kZBack);
    text.draw("Reach Hellspider",12,8,.55f,colors::white);
    char status[48];std::snprintf(status,sizeof status,"Skitter defeated: %d / 3",world.cleared());
    text.draw(status,12,31,.4f,colors::cyanLight);
    auto px=[](float x){return 137.f+x*3.2f;};
    auto py=[](float z){return 53.f+(z+70.f)*.86f;};
    auto road=[&](float x,float z,float xx,float zz) {
        C2D_DrawLine(px(x),py(z),colors::grey,px(xx),py(zz),colors::grey,5,kZFill);
    };
    for(unsigned i=1;i<sizeof exploration_data::road/sizeof exploration_data::road[0];++i) {
        const auto a=exploration_data::road[i-1],b=exploration_data::road[i];road(a.x,a.z,b.x,b.z);
    }
    road(2,-8,17,-8);road(17,-8,17,-32);road(17,-32,.45f,-32);
    C2D_DrawCircleSolid(px(-2),py(-66),kZIcon,4,colors::pink);
    for(const auto& e:world.enemies)if(e.alive)
        C2D_DrawCircleSolid(px(e.position.x),py(e.position.z),kZIcon,3,e.chasing?colors::pink:colors::yellow);
    C2D_DrawCircleSolid(px(world.player.x),py(world.player.z),kZText,3.5f,colors::cyan);
    text.draw("Circle Pad / D-Pad: Move",12,169,.42f,colors::white);
    text.draw("Hold B: Run    L / R: Camera",12,190,.42f,colors::white);
    text.draw("X: Recenter    A: Enter boss",12,211,.42f,colors::white);
}

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
    battle3d::init(top);
    assets::init();
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    TouchState     touch;
    DialogueScreen dialogue;
    BattleScreen   battle;
    CustomizeScreen customize;
    GameState      state = GameState::Dialogue;
    exploration::World city;
    bool hasBattle=false;
    int cityFrame=0;
    u64 lastTick=osGetTime();
    BattlePhase    prevBattlePhase = BattlePhase::Planning;
    const bool audioReady = audio::init();
    sfx::init(audioReady);
    // Berkas belum ada = roster bawaan. Berkas rusak tidak fatal: nilainya dijepit/diganti bawaan.
    party_config::PartyConfig partyCfg;
    party_config::load(kPartyPath, partyCfg);
    dialogue.enter();
    audio::play(audio::Track::Dialogue);

    while (aptMainLoop()) {
        hidScanInput();
        const u32 kDown = hidKeysDown();
        if (kDown & KEY_START) {
            // Jangan buang hasil edit kalau app ditutup di tengah layar Customize.
            if (state != GameState::Customize || party_config::save(kPartyPath, customize.config())) break;
            customize.saveFailed();
        }
        touch.update();
        const u32 kHeld = hidKeysHeld();
        const u64 now=osGetTime();
        const float dt=std::min(.05f,static_cast<float>(now-lastTick)/1000.f);
        lastTick=now;

        switch (state) {
        case GameState::Dialogue:
            dialogue.update(touch, kDown);
            if (kDown & KEY_SELECT) {
                customize.enter(partyCfg);
                state = GameState::Customize;
            } else if (dialogue.done()) {
                battle.setPartyConfig(partyCfg);
                city.reset();hasBattle=false;cityFrame=0;
                state = GameState::Explore;
                audio::play(audio::Track::Dialogue);
            }
            break;
        case GameState::Customize:
            customize.update(touch, kDown);
            if (customize.wantsExit()) {
                if (party_config::save(kPartyPath, customize.config())) {
                    partyCfg = customize.config();
                    state = GameState::Dialogue;
                } else {
                    customize.saveFailed();
                }
            }
            break;
        case GameState::Explore: {
            circlePosition pad;hidCircleRead(&pad);
            float x=std::abs(pad.dx)>18?pad.dx/154.f:0.f;
            float forward=std::abs(pad.dy)>18?pad.dy/154.f:0.f;
            if(kHeld&KEY_DLEFT)x=-1;
            if(kHeld&KEY_DRIGHT)x=1;
            if(kHeld&KEY_DUP)forward=1;
            if(kHeld&KEY_DDOWN)forward=-1;
            if(kDown&KEY_X)city.cameraYaw=city.heading;
            city.update(x,forward,((kHeld&KEY_R)?1.f:0.f)-((kHeld&KEY_L)?1.f:0.f),
                        kHeld&KEY_B,kDown&KEY_A,dt);
            ++cityFrame;
            if(city.pending>=0) {
                const bool skitter=city.pending<3;
                battle.enter(skitter?EnemyKind::Skitter:EnemyKind::Hellspider,hasBattle,skitter);
                hasBattle=true;state=GameState::Battle;prevBattlePhase=battle.phase();
                audio::play(audio::Track::Battle);
            }
            break;
        }
        case GameState::Battle:
            battle.update(touch, kDown, kHeld);
            {
                // Begitu masuk fase Awakening, Track::Awaken terus main sebagai tema bos fase 2
                // sampai fase Dying. Encounter baru kembali memakai Track::Battle.
                const BattlePhase curPhase = battle.phase();
                if (curPhase != prevBattlePhase) {
                    if (curPhase == BattlePhase::Planning && prevBattlePhase == BattlePhase::Victory)
                        audio::play(audio::Track::Battle);
                    else if (curPhase == BattlePhase::Awakening) audio::play(audio::Track::Awaken);
                    // BGM di-fade saat hit terakhir, sebelum penghancuran. SFX tetap aktif.
                    else if (curPhase == BattlePhase::Dying) audio::play(audio::Track::None);
                    prevBattlePhase = curPhase;
                }
            }
            if(battle.wantsMap()) {
                city.victory();state=GameState::Explore;audio::play(audio::Track::Dialogue);
            } else if (battle.wantsRestart()) {
                dialogue.enter();
                state = GameState::Dialogue;
                audio::play(audio::Track::Dialogue);
            }
            break;
        }

        if(state==GameState::Explore)battle3d::prepareExploration(top);
        else if (state != GameState::Customize)
            battle3d::prepareScene(top, state == GameState::Dialogue ? EnemyKind::Hellspider : battle.enemyKind());
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        text.beginFrame();

        const bool scene3d = state == GameState::Dialogue  ? dialogue.drawScene3d(top)
                           : state == GameState::Customize ? false
                           : state == GameState::Explore ? battle3d::renderExploration(top,city,cityFrame)
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
        if (state == GameState::Dialogue)       dialogue.drawTop(text);
        else if (state == GameState::Customize) customize.drawTop(text);
        else if (state == GameState::Explore) drawCityTop(text,city,scene3d);
        else                                    battle.drawTop(text, scene3d);

        C2D_TargetClear(bottom, colors::panel);
        C2D_SceneBegin(bottom);
        if (state == GameState::Dialogue) {
            dialogue.drawBottom(text);
            text.drawRight("SELECT: Custom Party", 312, 229, 0.3f, colors::grey);
        } else if (state == GameState::Customize) {
            customize.drawBottom(text);
        } else if (state == GameState::Explore) {
            drawCityBottom(text,city);
        } else {
            battle.drawBottom(text);
        }

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
