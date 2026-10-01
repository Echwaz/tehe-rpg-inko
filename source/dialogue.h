#pragma once
// State dialog ala visual novel (murni data + logika, tanpa render; lihat dialogue_screen.cpp).
// Dialog ditampilkan di atas scene kota 3D, tanpa sprite karakter.
#include <cstddef>
#include <string>
#include <vector>

struct DialogueLine {
    std::string speaker;
    std::string text;

    DialogueLine(const char* s, const char* t) : speaker(s), text(t) {}
};

class DialogueScene {
public:
    void load(const std::vector<DialogueLine>& lines);

    void update();
    void advance();     // ketuk: tampilkan semua teks, atau lanjut ke baris berikut
    void skipAll();

    bool finished()     const { return finished_; }
    bool lineComplete() const;
    const DialogueLine* current() const;
    std::string visibleText() const;
    std::size_t index() const { return index_; }

    // Riwayat untuk tombol Log: baris 0 sampai baris yang sedang tampil.
    std::size_t historyCount() const { return finished_ ? lines_.size() : index_ + 1; }
    const DialogueLine& lineAt(std::size_t i) const { return lines_[i]; }
    std::string logText(std::size_t i) const;   // baris terakhir: hanya bagian yang sudah tampil
    std::size_t count() const { return lines_.size(); }

private:
    std::size_t revealedBytes() const;

    std::vector<DialogueLine> lines_;
    std::size_t index_    = 0;
    float       revealed_ = 0.f;
    bool        finished_ = true;
};

std::vector<DialogueLine> makePrologueScript();
