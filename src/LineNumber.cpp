#include "LineNumber.h"
#include "Editor.h"
#include "glm/fwd.hpp"
#include <iostream>

LineNumber::LineNumber(const Editor& editor) : Editor(editor.screen_.x, editor.screen_.y, editor.lineHeight_) {
    lines_.clear();
    lines_.push_back({});
    addLineNumber(lines_.back(), lines_.size());
    // std::cout << lines_.back() << std::endl;
    wordCount_ = lines_.size() * 5;
}

void LineNumber::adjustCursor(glm::ivec2 cursorPos, const std::vector<std::string>& lines) {
    cursorPos_ = cursorPos;
    std::cout << lines_.size() << std::endl;
    while (lines_.size() < lines.size()) {
        lines_.push_back({});
        addLineNumber(lines_.back(), lines_.size());
    }

    while (lines_.size() > lines.size()) {
        lines_.pop_back();
    }

    wordCount_ = lines_.size() * 5;

    moveLimit();
}

void LineNumber::addLineNumber(std::string& line, int32_t lineNumber) {
    char s[10];
    snprintf(s, 10, "%4d ", lineNumber);
    line = s;
}