#include "LineNumber.h"
#include "Editor.h"
#include "glm/fwd.hpp"

LineNumber::LineNumber(const Editor& editor) : Editor(editor.screen_.x, editor.screen_.y, editor.lineHeight_) {
    lines_.push_back({});
    addLineNumber(lines_.back(), lines_.size());
    wordCount_ = lines_.size() * 5;
}

void LineNumber::adjustCursor(glm::ivec2 cursorPos, const std::vector<std::string>& lines) {
    cursorPos_ = cursorPos;
    while (lines_.size() < lines.size()) {
        lines_.push_back({});
        addLineNumber(lines_.back(), lines_.size() + 1);
    }

    while (lines_.size() > lines.size()) {
        lines_.pop_back();
    }

    wordCount_ = lines_.size() * 5;
}

void LineNumber::addLineNumber(std::string& line, int32_t lineNumber) {
    char s[10];
    snprintf(s, 10, "%4d ", lineNumber);
    line = s;
}