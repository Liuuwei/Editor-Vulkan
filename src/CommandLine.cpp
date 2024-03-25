#include "CommandLine.h"
#include "Editor.h"
#include "GLFW/glfw3.h"
#include "glm/fwd.hpp"

CommandLine::CommandLine(const Editor& editor) : Editor(editor) {
    whichLine_ = editor.showLines_;
    lineNumberOffset_ = 1;
    onlyLine_.insert(onlyLine_.begin(), ':');
    cursorX_ = 0;
}

std::string CommandLine::enter() {
    std::string str = {onlyLine_.begin() + lineNumberOffset_, onlyLine_.end()};

    onlyLine_.erase(onlyLine_.begin() + lineNumberOffset_, onlyLine_.end());

    cursorX_ = 0;

    return str;
}

void CommandLine::insertChar(char c) {
    onlyLine_.insert(onlyLine_.begin() + cursorX_ + lineNumberOffset_, c);
    cursorX_++;
}

void CommandLine::insertStr(const std::string& str) {
    for (auto c : str) {
        insertChar(c);
    }
}

void CommandLine::backspace() {
    if (onlyLine_.size() > 1) {
        onlyLine_.pop_back();
        cursorX_--;
    }
}

void CommandLine::moveCursor(int dir) {
    if (dir == GLFW_KEY_LEFT) {
        if (cursorX_ > 0) {
            cursorX_--;
        }
    } else if (dir == GLFW_KEY_RIGHT) {
        if (cursorX_ < onlyLine_.size() - lineNumberOffset_) {
            cursorX_++;
        }
    }
}

glm::ivec2 CommandLine::cursorRenderPos(int fontAdvance) {
    glm::ivec2 xy;
    xy.x = static_cast<float>(cursorX_);
    xy.y = static_cast<float>(whichLine_);

    xy.x += lineNumberOffset_;
    xy.x *= fontAdvance;

    xy.y *= static_cast<float>(lineHeight_);
    xy.y += static_cast<float>(lineHeight_) / 2.0f;

    xy = posToScreenPos(xy);

    return xy;
}

bool CommandLine::empty() {
    return onlyLine_.size() <= 1;
}

void CommandLine::clear() {
    onlyLine_.erase(onlyLine_.begin() + lineNumberOffset_, onlyLine_.end());
    cursorX_ = 0;
}