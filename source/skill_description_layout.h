#pragma once
#include <string>
#include <vector>

class SkillDescriptionLayout {
public:
    void reset() { ready_ = false; }
    template<class Measure>
    const std::vector<std::string>& lines(const char* description, const char* tag, Measure measure) {
        if (ready_) return lines_;
        lines_.clear();
        std::string text = description;
        if (tag[0]) { if (!text.empty()) text += ' '; text += tag; }
        std::string current;
        std::size_t at = 0;
        while (at <= text.size()) {
            const std::size_t next = text.find(' ', at);
            const std::string word = text.substr(at, next == std::string::npos ? next : next - at);
            const std::string candidate = current.empty() ? word : current + " " + word;
            if (!current.empty() && measure(candidate.c_str()) > 276.f) {
                lines_.push_back(current);
                current = word;
            } else current = candidate;
            if (next == std::string::npos) break;
            at = next + 1;
        }
        if (!current.empty()) lines_.push_back(current);
        ready_ = true;
        return lines_;
    }
private:
    bool ready_ = false;
    std::vector<std::string> lines_;
};
