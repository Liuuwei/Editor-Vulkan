#pragma once

#include "GLFW/glfw3.h"
#include "glm/fwd.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <iostream>
#include <format>

class Editor {
public:
    enum Mode {
        General, 
        Command, 
        Insert, 
    };

    enum Direction {
        Up = GLFW_KEY_UP, 
        Down = GLFW_KEY_DOWN, 
        Right = GLFW_KEY_RIGHT, 
        Left = GLFW_KEY_LEFT, 
    };

    struct Limit {
        int32_t up_ = 0;
        int32_t bottom_ = 1;
    };

    Editor(int32_t width, int32_t height, int32_t lineHeight, int32_t fontAdvance = 0, int32_t showWordsOffset = 0);

    void init(const std::string& path);
    Mode mode() const;
    void enter();
    void backspace();
    void insertChar(char c);
    void insertStr(const std::string& str);
    void delteChar();
    void copyLine();
    void paste();
    void rmSpaceOrWord();
    void rmSpace();
    void rmWord();
    void moveRight();
    void moveLeft();
    void moveRightSpace();
    void moveRightWord();
    void moveLeftSpace();
    void moveLeftWord();
    void rmCopyLine();
    void adjustCursor();
    void moveCursor(Direction dir);
    void setCursor(glm::ivec2 cursorPos);
    void moveLimit();
    bool lineEmpty(const std::string& line);
    void adjust(int32_t width, int32_t height);
    glm::ivec2 cursorRenderPos(int32_t offsetX, int32_t fontAdvance);
    bool empty();
    Editor::Limit showLimit();
    glm::ivec2 posToScreenPos(glm::ivec2 pos);
    glm::ivec2 searchStr(const std::string& str);
    bool save();
    bool save(const std::string& fileName);
    void setMode(Mode mode);
    void newLine(); // huan hang
    glm::ivec2 nextCharPosition(int32_t offsetX, int32_t fontAdvance);
    void setShowWordOffset(int32_t offset);

public:
    Mode mode_ = General;
    int32_t lineHeight_;
    int32_t fontAdvance_;
    int32_t currLine_ = 0;
    std::vector<std::string> lines_;
    glm::ivec2 cursorPos_ = {0, 0};
    glm::ivec2 cursorPosTrue_ = {};
    glm::ivec2 screen_;
    int32_t showLines_;
    int32_t showWords_;
    int32_t showWordsOffset_ = 0;
    Limit limit_{};
    int32_t lineNumberOffset_ = 0;
    int showLinesOffset_ = 1;
    unsigned long long wordCount_ = 0;
    std::string fileName_;
    std::string copyLine_;
};