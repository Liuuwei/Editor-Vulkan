#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class Grammar {
public:
    Grammar(const std::string& path);

    bool matchWord(const std::string& word) const;
    glm::vec3 color(const std::string& word) const;
    std::vector<std::pair<std::pair<int, int>, glm::vec3>> parseLine(std::string line) const;

private:
    std::unordered_map<std::string, glm::vec3> wordToColor_;
    std::unordered_map<std::string, glm::vec3> stringToColor_;
};