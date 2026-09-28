#include "dialogue.h"

#include <algorithm>

namespace {
constexpr float kCharsPerFrame = 0.6f;   // ~36 karakter per detik pada 60 FPS
}

void DialogueScene::load(const std::vector<DialogueLine>& lines) {
    lines_    = lines;
    index_    = 0;
    revealed_ = 0.f;
    finished_ = lines_.empty();
}

const DialogueLine* DialogueScene::current() const {
    return finished_ ? nullptr : &lines_[index_];
}

std::size_t DialogueScene::revealedBytes() const {
    if (finished_) return 0;
    const std::string& s = lines_[index_].text;
    std::size_t n = std::min(s.size(), static_cast<std::size_t>(revealed_));
    while (n < s.size() && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80)
        ++n;                                   // jangan potong di tengah karakter UTF-8
    return n;
}

bool DialogueScene::lineComplete() const {
    return finished_ || revealedBytes() >= lines_[index_].text.size();
}

std::string DialogueScene::visibleText() const {
    if (finished_) return std::string();
    return lines_[index_].text.substr(0, revealedBytes());
}

std::string DialogueScene::logText(std::size_t i) const {
    if (i >= lines_.size()) return std::string();
    if (!finished_ && i == index_) return visibleText();
    return lines_[i].text;
}

void DialogueScene::update() {
    if (!finished_ && !lineComplete()) revealed_ += kCharsPerFrame;
}

void DialogueScene::advance() {
    if (finished_) return;
    if (!lineComplete()) {
        revealed_ = static_cast<float>(lines_[index_].text.size());
        return;
    }
    revealed_ = 0.f;
    if (++index_ >= lines_.size()) finished_ = true;
}

void DialogueScene::skipAll() {
    finished_ = true;
}

std::vector<DialogueLine> makePrologueScript() {
    return {
        { "Nanami Nanase", "Visual telah dikonfirmasi. Hellspider kini terlihat di tengah kota." },
        { "Nanami Nanase", "Squad 31-A diperintahkan untuk menghadapi Hellspider." },
        { "Ruka Kayamori", "Disini Ruka Kayamori. Squad 31-A akan segera menuju kesana." },
        { "Ruka Kayamori", "Ayo kita bergerak!" },
        { "Member 31-A yang lain", "Baik!" },
    };
}
