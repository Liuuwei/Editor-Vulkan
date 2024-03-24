#pragma once

#include "Editor.h"
#include "glm/fwd.hpp"
#include <cstdint>

class LineNumber : public Editor {
public:
    LineNumber(const Editor& editor);
    
    void adjustCursor(glm::ivec2 cursorPos, const std::vector<std::string>& lines);
    void addLineNumber(std::string& line, int32_t lineNumber);

public:
    int32_t lineNumberOffset_ = 5;
};