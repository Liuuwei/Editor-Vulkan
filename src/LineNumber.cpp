#include "LineNumber.h"
#include "Editor.h"
#include "glm/fwd.hpp"
#include <iostream>

LineNumber::LineNumber(const Editor& editor) : Editor(editor.screen_.x, editor.screen_.y, editor.lineHeight_) {
    lines_.clear();
    lines_.push_back({});
    addLineNumber(lines_.back(), lines_.size());
    wordCount_ = lines_.size() * 5;
    
    adjust(editor);
}

void LineNumber::adjust(const Editor& editor) {
    adjustCursor(editor);
    limit_ = editor.limit_;
}

void LineNumber::adjustCursor(const Editor& editor) {
    cursorPos_ = editor.cursorPos_;
    while (lines_.size() < editor.lines_.size()) {
        lines_.push_back({});
        addLineNumber(lines_.back(), lines_.size());
    }

    while (lines_.size() > editor.lines_.size()) {
        lines_.pop_back();
    }

    wordCount_ = lines_.size() * 5;
}

void LineNumber::addLineNumber(std::string& line, int32_t lineNumber) {
    char s[10];
    snprintf(s, 10, "%4d ", lineNumber);
    line = s;
}