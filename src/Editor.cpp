#include "Editor.h"
#include "glm/fwd.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <format>

Editor::Editor(int32_t width, int32_t height, int32_t lineHeight) : screen_(width, height), lineHeight_(lineHeight), showLines_(height / lineHeight) {
    lines_.resize(1);
}

Editor::Mode Editor::mode() const {
    return mode_;
}

void Editor::enter() {
    auto& currLine = lines_[cursorPos_.y];
    auto newLine = std::string(currLine.begin() + cursorPos_.x + lineNumberOffset_, currLine.end());
    currLine.erase(currLine.begin() + cursorPos_.x + lineNumberOffset_, currLine.end());

    cursorPos_.y++;
    cursorPos_.x = 0;
    lines_.insert(lines_.begin() + cursorPos_.y, newLine);

    moveLimit();
}

void Editor::backspace() {
    auto& currLine = lines_[cursorPos_.y];

    if (cursorPos_.x == 0 && cursorPos_.y == 0) {
        return ;
    }
    
    if (lineEmpty(currLine)) {
        lines_.erase(lines_.begin() + cursorPos_.y);
        cursorPos_.y--;
        cursorPos_.x = lines_[cursorPos_.y].size() - lineNumberOffset_;
    } else {
        delteChar();
    }

    moveLimit();
}

void Editor::insertChar(char c) {
    if (cursorPos_.y >= lines_.size()) {
        lines_.push_back({});
    }
    
    auto& currLine = lines_[cursorPos_.y];

    currLine.insert(currLine.begin() + cursorPos_.x + lineNumberOffset_, c);
    cursorPos_.x++;

    wordCount_++;

    moveLimit();
}

void Editor::insertStr(const std::string& str) {
    for (size_t i = 0; i < str.size(); i++) {
        insertChar(str[i]);
    }
}

void Editor::delteChar() {
    auto& currLine = lines_[cursorPos_.y];

    if (cursorPos_.x == 0) {
        lines_[cursorPos_.y - 1] += std::string(currLine.begin() + lineNumberOffset_, currLine.end());
        lines_.erase(lines_.begin() + cursorPos_.y);
        cursorPos_.y--;
        cursorPos_.x = lines_[cursorPos_.y].size() - lineNumberOffset_;
    } else {
        currLine.erase(currLine.begin() + (cursorPos_.x + lineNumberOffset_ - 1));
        cursorPos_.x--;
    }

    wordCount_--;

    moveLimit();
}

void Editor::moveCursor(Editor::Direction dir) {
    switch (dir) {
    case Up:
        if (cursorPos_.y == 0) {
            return ;
        }
        cursorPos_.y--;
        cursorPos_.x = std::min(cursorPos_.x, static_cast<int32_t>(lines_[cursorPos_.y].size() - lineNumberOffset_));
        break;
    case Down:
        if (cursorPos_.y >= lines_.size() - 1) {
            return ;
        }
        cursorPos_.y++;
        cursorPos_.x = std::min(cursorPos_.x, static_cast<int32_t>(lines_[cursorPos_.y].size() - lineNumberOffset_));
        break;
    case Right:
        if (cursorPos_.x + lineNumberOffset_ >= lines_[cursorPos_.y].size()) {
            return ;
        }
        cursorPos_.x++;
        break;
    case Left:
        if (cursorPos_.x <= 0) {
            return ;
        }
        cursorPos_.x--;
        break;
    }

    moveLimit();
}

void Editor::moveLimit() {
    if (cursorPos_.y < limit_.up_) {
        limit_.up_ = cursorPos_.y;
    } else if (cursorPos_.y >= limit_.bottom_) {
        limit_.up_ = cursorPos_.y - showLines_ + 1;
    }

    limit_.up_ = std::max(limit_.up_, 0);
    limit_.bottom_ = limit_.up_ + showLines_;
}

bool Editor::lineEmpty(const std::string& line) {
    return line.size() - lineNumberOffset_ <= 0;
}

void Editor::adjust(int32_t width, int32_t height) {
    screen_ = glm::uvec2(width, height);

    showLines_ = height / lineHeight_;

    limit_.bottom_ = limit_.up_ + showLines_;
}

glm::ivec2 Editor::cursorRenderPos(int32_t offsetX, int32_t fontAdvance) {
    auto x = static_cast<float>(cursorPos_.x);
    auto y = static_cast<float>(cursorPos_.y - limit_.up_);

    x += lineNumberOffset_;
    x *= fontAdvance;
    
    y *= static_cast<float>(lineHeight_);
    y += static_cast<float>(lineHeight_) / 2.0f;

    x += -static_cast<float>(screen_.x) / 2.0f;
    y = static_cast<float>(screen_.y) / 2.0f - y;

    return {x + offsetX, y};
}

bool Editor::empty() {
    for (size_t i = showLimit().up_; i < showLimit().bottom_; i++) {
        if (!lines_[i].empty()) {
            return false;
        }
    }

    return true;
}

Editor::Limit Editor::showLimit() {
    auto up = std::max(limit_.up_, 0);
    auto bottom = std::min(limit_.bottom_, static_cast<int32_t>(lines_.size()));

    return {up, bottom};
}