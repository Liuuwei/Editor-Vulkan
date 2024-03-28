#include "Grammar.h"

#include <cstddef>
#include <fstream>
#include <algorithm>

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <iostream>

using json = nlohmann::json;

Grammar::Grammar(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("json file open failed");
    }

    json data = json::parse(file);

    stringToColor_ = {
        {"Red", glm::vec3(1.0f, 0.0f, 0.0f)}, 
        {"Green", glm::vec3(0.0f, 1.0f, 0.0f)}, 
        {"Blue", glm::vec3(0.0f, 0.0f, 1.0f)}, 
        {"Purple", glm::vec3(128.0f, 0.0f, 128.0f)}, 
    };
    
    for (auto it = data.begin(); it != data.end(); ++it) {
        glm::vec3 color;

        if (stringToColor_.find(it.key()) == stringToColor_.end()) {
            color = glm::vec3(0.0f, 0.0f, 0.0f);
        } else {
            color = stringToColor_[it.key()];
        }

        for (const auto& word : it.value()) {
            wordToColor_[word] = color;
        }
    }
}

bool Grammar::matchWord(const std::string& word) const {
    if (word.size() >= 2 && word.front() == '\"' && word.back() == '\"') {
        return true;
    }
    if (word.size() > 2 && word.front() == '<' && word.back() == '>') {
        return true;
    }

    return wordToColor_.find(word) != wordToColor_.end();
}

glm::vec3 Grammar::color(const std::string& word) const {
    if (word.size() >= 2 && word.front() == '\"' && word.back() == '\"') {
        return wordToColor_.at("\"\"");
    }
    if (word.size() > 2 && word.front() == '<' && word.back() == '>') {
        return wordToColor_.at("<>");
    }

    return wordToColor_.at(word);
}

std::vector<std::pair<std::pair<int, int>, glm::vec3>> Grammar::parseLine(std::string line) const {
    std::vector<std::pair<std::pair<int, int>, glm::vec3>> result;

    line.push_back(' ');

    std::vector<int> spaceIndex;

    for (int i = line.size() - 1; i >= 1; i--) {
        if (line[i] == ' ' && line[i - 1] != ' ') {
            spaceIndex.push_back(i);
        }
    }

    for (int i = 0; i < line.size(); ) {
        if (line[i] != ' ' && (i == 0 || line[i - 1] == ' ')) {
            bool match = false;
            
            for (auto index : spaceIndex) {
                if (i > index) {
                    break;
                }
                
                std::string word(line.begin() + i, line.begin() + index);
                if (matchWord(word)) {
                    result.push_back({{i, index - i}, color(word)});
                    match = true;
                    i = index + 1;
                    break;
                }
            }

            if (!match) {
                i++;
            }
        } else {
            i++;
        }
    }

    return result;
}