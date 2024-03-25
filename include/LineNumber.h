#pragma once

#include "Editor.h"
#include "glm/fwd.hpp"
#include <cstdint>

class LineNumber : public Editor {
public:
    LineNumber(const Editor& editor);
    
    void adjust(const Editor& editor);
    void adjustCursor(const Editor& editor);
    void addLineNumber(std::string& line, int32_t lineNumber);

public:
    int32_t lineNumberOffset_ = 5;
};