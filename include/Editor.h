#pragma once

#include "GLFW/glfw3.h"
#include "glm/fwd.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>

class Editor {
public:
    enum Mode {
        Command, 
        Input, 
    };

    enum Direction {
        Up = GLFW_KEY_UP, 
        Down = GLFW_KEY_DOWN, 
        Right = GLFW_KEY_RIGHT, 
        Left = GLFW_KEY_LEFT, 
    };

    Editor(int32_t width, int32_t height, int32_t lineHeight);

    Mode mode() const;
    void enter();
    void backspace();
    void insertChar(char c);
    void insertStr(const std::string& str);
    void delteChar();
    void moveCursor(Direction dir);
    void moveLimit();
    bool lineEmpty(const std::string& line);
    void adjust(int32_t width, int32_t height);
    glm::ivec2 cursorRenderPos(int32_t offsetX, int32_t fontAdvance);
public:
    struct Limit {
        int32_t up_ = 0;
        int32_t bottom_ = 1;
    };

    Mode mode_ = Command;
    int32_t lineHeight_;
    int32_t currLine_ = 0;
    std::vector<std::string> lines_;
    glm::ivec2 cursorPos_ = {0, 0};
    glm::ivec2 cursorPosTrue_ = {};
    glm::ivec2 screen_;
    int32_t showLines_;
    Limit limit_{};
    int32_t lineNumberOffset_ = 0;
    unsigned long long wordCount_ = 0;
};