#pragma once
// Dua scene game (dialogue_screen.cpp, battle_screen.cpp), masing-masing dengan update() serta
// draw untuk layar atas (visual) dan layar bawah (sentuh).
#include "combat.h"
#include "dialogue.h"
#include "party_config.h"
#include "ui.h"

class DialogueScreen {
public:
    void enter();
    void update(const TouchState& touch, u32 keysDown);
    bool drawScene3d(C3D_RenderTarget* target) const;
    void drawTop(TextRenderer& text) const;
    void drawBottom(TextRenderer& text) const;
    bool done() const { return scene_.finished(); }

private:
    enum class Mode { Normal, Log };

    void openLog();
    void drawTray(TextRenderer& text) const;
    void drawLog(TextRenderer& text) const;

    DialogueScene scene_;
    ButtonGroup   buttons_;
    Mode          mode_         = Mode::Normal;
    bool          windowHidden_ = false;   // tombol HIDE: sembunyikan kotak dialog
    std::size_t   logTop_       = 0;
    int           frame_        = 0;
};

// Layar Custom Party: ganti nama karakter serta nilai dan efek skill (lihat party_config.h).
// Dibuka dari main.cpp lewat SELECT saat dialog. Tiga halaman: daftar karakter -> satu karakter
// -> satu skill. Perubahan disimpan ke party.cfg oleh main.cpp saat layar ditutup.
class CustomizeScreen {
public:
    void enter(const party_config::PartyConfig& cfg);
    void update(const TouchState& touch, u32 keysDown);
    void drawTop(TextRenderer& text) const;
    void drawBottom(TextRenderer& text) const;
    bool wantsExit() const { return exit_; }
    const party_config::PartyConfig& config() const { return cfg_; }

private:
    enum class Page { Roster, Character, Skill };

    void buildButtons();
    void handle(int buttonId);
    void goBack();
    void rename();
    void say(const char* msg);

    void drawRoster(TextRenderer& text) const;
    void drawCharacter(TextRenderer& text) const;
    void drawSkill(TextRenderer& text) const;

    party_config::PartyConfig cfg_;
    ButtonGroup buttons_;
    Page        page_ = Page::Roster;
    int         ch_ = 0;               // karakter terpilih 0..5 (0..2 front bawaan, 3..5 back bawaan)
    int         sk_ = 0;               // skill terpilih 0..1
    bool        exit_ = false;
    bool        confirmReset_ = false; // "Reset All" butuh dua ketukan
    int         frame_ = 0;
    std::string toast_;
    int         toastFrame_ = -1000;
};

class BattleScreen {
public:
    // Party kustom untuk battle berikutnya (dipanggil sebelum enter()). Tanpa panggilan ini
    // dipakai roster bawaan.
    void setPartyConfig(const party_config::PartyConfig& cfg) {
        battle_.setPartyOverride(party_config::buildParty(cfg));
    }
    void enter();
    void update(const TouchState& touch, u32 keysDown, u32 keysHeld);
    bool drawScene3d(C3D_RenderTarget* target) const;
    void drawTop(TextRenderer& text, bool scene3d = false) const;
    void drawBottom(TextRenderer& text) const;
    bool wantsRestart() const { return restart_; }
    BattlePhase phase() const { return battle_.phase(); }   // dipakai main.cpp untuk ganti BGM saat Awakening

private:
    // Plan: pilih aksi. Swap: atur formasi. SkillDetail: layar penjelasan satu skill,
    // dibuka dengan menekan-tahan atau mengetuk dua kali baris skill tersebut.
    enum class Menu { Plan, Swap, SkillDetail };

    void buildButtons();
    void handle(int buttonId);

    // Swap karakter dengan seret-dan-lepas (atau ketuk dua kotak).
    // Kotak 0..2 = front, 3..5 = back.
    void updateSwap(const TouchState& touch);
    void resetSwapState();
    void doSwap(int a, int b);
    int  tileAt(int px, int py) const;
    const Combatant& tileCombatant(int id) const;

    // Deteksi tekan-tahan / ketuk-dua-kali pada baris skill untuk membuka layar penjelasan.
    void updateSkillGestures(const TouchState& touch);
    int  skillRowAt(int px, int py) const;      // -1 = bukan baris skill (hanya skill, bukan Attack)
    void openSkillDetail(int skillIdx);

    // Kontrol tombol fisik (L/R/D-Pad/Circle Pad/A/B/X/Y), sebagai alternatif sentuh.
    void updateButtonsInput(u32 keysDown, u32 keysHeld);
    bool planRowValid(int row) const;   // row: 0 = Attack, 1 = Skill 0, 2 = Skill 1

    void drawPlanning(TextRenderer& text) const;
    void drawSwapScreen(TextRenderer& text) const;
    void drawSkillDetail(TextRenderer& text) const;

    // Efek visual battle (angka damage/heal, flash, cincin break). Sumbernya event dari Battle.
    struct FxItem {
        FxEvent ev;
        int     start;     // frame_ saat efek mulai tampil
        bool    applied;   // status visual bos (break/stun) sudah dinyalakan oleh event ini
        FxItem(const FxEvent& e, int s) : ev(e), start(s), applied(false) {}
    };
    void  drawFx(TextRenderer& text, bool scene3d) const;
    float allyShakeX(int slot) const;

    Battle      battle_;
    ButtonGroup buttons_;
    Menu        menu_    = Menu::Plan;
    int         slot_    = 0;
    bool        restart_ = false;

    int  selected_   = -1;
    int  dragFrom_   = -1;
    bool dragging_   = false;                  // sudah bergeser cukup jauh = seret
    int  dragStartX_ = 0, dragStartY_ = 0;
    int  dragX_      = 0, dragY_ = 0;

    // Kursor tombol fisik (paralel dengan status sentuh di atas)
    int  menuCursor_       = 0;    // baris disorot di Planning: 0 Attack, 1 Skill0, 2 Skill1
    bool usingButtons_     = false; // true selama pemain terakhir pakai tombol fisik (bukan sentuh)
    int  swapCursor_       = 0;    // kotak disorot (0..5) di layar Swap
    bool confirmFocus_     = false; // true = kursor sedang di tombol Confirm layar Swap
    int  keyHoldRow_       = -1;   // baris skill yang sedang ditahan tombol A (-1 = tidak ada)
    int  keyHoldStartFrame_ = -1;
    bool keyHoldFired_     = false;

    int  frame_             = 0;
    std::vector<FxItem> fx_;
    // Status bos yang sedang TAMPIL (bukan status logika): mengatur pose 3D dan indikator HUD.
    // Menyala saat efeknya mulai tampil, mati begitu logika bilang sudah tidak berlaku.
    bool bossBroken_        = false;
    bool bossStunned_       = false;
    int  detailSkillIdx_    = -1;
    int  gestureRow_        = -1;               // baris skill yang sedang ditekan (-1 = tidak ada)
    int  gestureStartFrame_ = -1;
    bool gestureHoldFired_  = false;
    int  lastTapRow_        = -1;
    int  lastTapFrame_      = -1000;
    bool suppressNextSkillHit_ = false;         // abaikan hit tap berikutnya (sudah dipakai gesture)

    // Animasi bar DP/HP party (indeks = slot front 0..2), bar DP/HP musuh, dan progres bar
    // Overdrive: nilainya meluncur halus menuju target tiap frame, dipakai gantinya nilai
    // mentah saat menggambar bar (lihat AnimatedBar di ui.h).
    AnimatedBar allyDpAnim_[3], allyHpAnim_[3];
    AnimatedBar enemyDpAnim_, enemyHpAnim_;
    AnimatedBar odAnim_;
};
