#include "Editor.h"
#include "CommandPool.h"
#include "glm/fwd.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <format>
#include <stdexcept>
#include <string>

Editor::Editor(int32_t width, int32_t height, int32_t lineHeight) : screen_(width, height), lineHeight_(lineHeight), showLines_(height / lineHeight) {
    showLines_ -= showLinesOffset_;

    lines_.resize(1);
}

void Editor::init(const std::string& path) {
    std::fstream file(path);
    std::string line;
    if (!file.is_open()) {
        std::cout << std::format("faield to open file: {}\n", path);
        return ;
    }

    lines_.clear();
    cursorPos_.x = cursorPos_.y = 0;

    fileName_ = path;
    while (std::getline(file, line)) {
        insertStr(line);
        enter();
    }

    file.close();
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

void Editor::setCursor(glm::ivec2 cursorPos) {
    cursorPos_ = cursorPos;

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
    showLines_ -= showLinesOffset_;

    limit_.bottom_ = limit_.up_ + showLines_;
}

glm::ivec2 Editor::cursorRenderPos(int32_t offsetX, int32_t fontAdvance) {
    glm::ivec2 xy;
    xy.x = static_cast<float>(cursorPos_.x);
    xy.y = static_cast<float>(cursorPos_.y - limit_.up_);

    xy.x += lineNumberOffset_;
    xy.x *= fontAdvance;
    
    xy.y *= static_cast<float>(lineHeight_);
    xy.y += static_cast<float>(lineHeight_) / 2.0f;

    xy = posToScreenPos(xy);

    return {xy.x + offsetX, xy.y};
}

glm::ivec2 Editor::posToScreenPos(glm::ivec2 pos) {
    pos.x += -static_cast<float>(screen_.x) / 2.0f;
    pos.y = static_cast<float>(screen_.y) / 2.0f - pos.y;

    return pos;
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

glm::ivec2 Editor::searchStr(const std::string& str) {
    auto& currLine = lines_[cursorPos_.y];
    auto find = currLine.find(str);
    if (find != std::string::npos) {
        return {find, cursorPos_.y};
    }

    for (size_t i = cursorPos_.y; i < lines_.size(); i++) {
        find = lines_[i].find(str);
        if (find != std::string::npos) {
            return {find, i};
        }
    }

    for (size_t i = 0; i < cursorPos_.y; i++) {
        find = lines_[i].find(str);
        if (find != std::string::npos) {
            return {find, i};
        }
    }

    return {-1, -1};
}

bool Editor::save() {
    if (fileName_.empty()) {
        return false;
    }

    save(fileName_);

    return true;
}

bool Editor::save(const std::string& fileName) {
    std::ofstream file(fileName);
    if (!file.is_open()) {
        return false;
    }

    for (size_t i = 0; i < lines_.size(); i++) {
        file << lines_[i] << std::endl;
    }
    file.close();

    return true;
}

void Editor::setMode(Editor::Mode mode) {
    mode_ = mode;
}

void Editor::rmCopyLine() {
    if (lines_.size() <= 1) {
        lines_[0].clear();
        adjustCursor();
        return ;
    }

    copyLine_ = lines_[cursorPos_.y];

    lines_.erase(lines_.begin() + cursorPos_.y);
    adjustCursor();
}

void Editor::adjustCursor() {
    if (cursorPos_.y >= lines_.size()) {
        cursorPos_.y = lines_.size() - 1;
    }

    if (cursorPos_.x >= lines_[cursorPos_.y].size()) {
        cursorPos_.x = lines_[cursorPos_.y].size();
    }

    moveLimit();
}

void Editor::copyLine() {
    copyLine_ = lines_[cursorPos_.y];
}

void Editor::paste() {
    if (copyLine_.empty()) {
        return ;
    }

    insertStr(copyLine_);
}

void Editor::rmSpace() {
    while (cursorPos_.x > 0) {
        if (lines_[cursorPos_.y][cursorPos_.x - 1] != ' ') {
            break;
        }

        backspace();
    }
}

void Editor::rmWord() {
    while (cursorPos_.x > 0) {
        if (lines_[cursorPos_.y][cursorPos_.x - 1] == ' ') {
            break;
        }

        backspace();
    }
}

void Editor::rmSpaceOrWord() {
    if (cursorPos_.x > 0) {
        if (lines_[cursorPos_.y][cursorPos_.x - 1] == ' ') {
            rmSpace();
        } else {
            rmWord();
        }
    }
}

void Editor::moveRight() {
    if (cursorPos_.x < lines_[cursorPos_.y].size()) {
        if (lines_[cursorPos_.y][cursorPos_.x] == ' ') {
            moveRightSpace();
        } else {
            moveRightWord();
        }
    }
}

void Editor::moveRightSpace() {
    while (cursorPos_.x < lines_[cursorPos_.y].size()) {
        if (lines_[cursorPos_.y][cursorPos_.x] != ' ') {
            break;
        }

        moveCursor(Right);
    }
}

void Editor::moveRightWord() {
    while (cursorPos_.x < lines_[cursorPos_.y].size()) {
        if (lines_[cursorPos_.y][cursorPos_.x] == ' ') {
            break;
        }

        moveCursor(Right);
    }
}

void Editor::moveLeft() {
    if (cursorPos_.x > 0) {
        if (lines_[cursorPos_.y][cursorPos_.x - 1] == ' ') {
            moveLeftSpace();
        } else {
            moveLeftWord();
        }
    }
}

void Editor::moveLeftSpace() {
    while (cursorPos_.x > 0) {
        if (lines_[cursorPos_.y][cursorPos_.x - 1] != ' ') {
            break;
        }

        moveCursor(Left);
    }
}

void Editor::moveLeftWord() {
    while (cursorPos_.x > 0) {
        if (lines_[cursorPos_.y][cursorPos_.x - 1] == ' ') {
            break;
        }

        moveCursor(Left);
    }
}

void Editor::newLine() {
    lines_.insert(lines_.begin() + cursorPos_.y + 1, std::string());

    cursorPos_.y++;
    adjustCursor();
}