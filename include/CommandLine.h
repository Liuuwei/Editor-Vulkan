#pragma once

#include <glm/glm.hpp>

#include <string>
#include "Editor.h"

class CommandLine : public Editor {
public:
    CommandLine(const Editor& editor);

    std::string enter();
    void insertChar(char c);
    void insertStr(const std::string& str);
    void backspace();
    void moveCursor(int dir);
    glm::ivec2 cursorRenderPos(int fontAdvance);
    bool empty();
    void clear();

private:
    int whichLine_;
    int cursorX_;
    std::string onlyLine_; 
};