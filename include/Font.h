#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <iostream>

#include "Grammar.h"
#include "Plane.h"
#include "Image.h"
#include "Tools.h"
#include "Vertex.h"
#include "Grammar.h"
#include "vulkan/vulkan_core.h"
#include "Timer.h"

#include <ft2build.h>
#include <utility>
#include FT_FREETYPE_H

class Font : public Plane {  
public:
    Font(const std::string& path, uint32_t size);

    VkVertexInputBindingDescription bindingDescription(uint32_t binding) const override {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = binding;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescription.stride = sizeof(Point);

        return bindingDescription;
    }

    std::vector<VkVertexInputAttributeDescription> attributeDescription(uint32_t binding) const override {
        auto attributes = Plane::attributeDescription(binding);

        VkVertexInputAttributeDescription attributeDescription{};
        attributeDescription.binding = binding;
        attributeDescription.location = attributes.size();
        attributeDescription.format = VK_FORMAT_R32_UINT;
        attributeDescription.offset = offsetof(Point, index_);
        attributes.push_back(attributeDescription);

        return attributes;
    }

    struct Point {
        Point(float x, float y, float r, float g, float b, float u, float v, uint32_t index = 0) :
            position_(x, y), color_(r, g, b), texCoord_(u, v), index_(index) {}

        Point(glm::vec2 position, glm::vec3 color, glm::vec2 texCoord, uint32_t index = 0) : 
            position_(position), color_(color), texCoord_(texCoord), index_(index) {}

        Point(Plane::Point point, uint32_t index) : 
            position_(point.position_), color_(point.color_), texCoord_(point.texCoord_), index_(index) {}

        Point() {}

        Point operator+(const Point& rhs) {
            Point result(*this);
            result.position_ += rhs.position_;

            return result;
        }

        Point operator*(float value) {
            Point result(*this);
            result.position_ *= value;

            return result;
        }

        friend std::ostream& operator<<(std::ostream& os, const Font::Point& rhs) {
            // auto str = std::format("%d, %d", rhs.position_.x, rhs.position_.y);
            // os << str;

            os << rhs.position_.x << ", " << rhs.position_.y;
            
            return os;
        }

        glm::vec2 position_{};
        glm::vec3 color_{};
        glm::vec2 texCoord_{};
        uint32_t index_{};
    };

    struct Character {
        Character() {}
        char char_{};
        // left top point
        int offsetX_{};
        int offsetY_{};
        uint32_t width_{};
        uint32_t height_{};
        int advance_{};
        uint32_t index_{};
        glm::vec3 color_ = glm::vec3(0.0f, 0.0f, 0.0f);
        std::shared_ptr<Image> image_;
    };

    static std::pair<std::vector<Point>, std::vector<uint32_t>> vertices(float x, float y, const Character& character, glm::vec3 color) {
        auto width = character.width_, height = character.height_;
        
        auto index = character.index_;

        auto w2 = width / 2.0f, h2 = height / 2.0f;

        std::vector<Font::Point> vertices = {
            {x - w2, y + h2, color.x, color.y, color.z, 0.0f, 0.0f, index}, 
            {x + w2, y + h2, color.x, color.y, color.z,  1.0f, 0.0f, index}, 
            {x - w2, y - h2, color.x, color.y, color.z, 0.0f, 1.0f, index}, 
            {x + w2, y - h2, color.x, color.y, color.z, 1.0f, 1.0f, index}, 
        };

        std::vector<uint32_t> indices = {
            0, 1, 2, 2, 1, 3
        };

        return {vertices, indices};
    }

    #ifndef defVertices
    #define defVertices(vertices, indices, x, y, character, color) \
    { \
        auto w2 = character.width_ / 2.0f, h2 = character.height_ / 2.0f; \
        auto index = character.index_; \
      \
        vertices = { \
            {x - w2, y + h2, color.x, color.y, color.z, 0.0f, 0.0f, index}, \
            {x + w2, y + h2, color.x, color.y, color.z,  1.0f, 0.0f, index}, \
            {x - w2, y - h2, color.x, color.y, color.z, 0.0f, 1.0f, index}, \
            {x + w2, y - h2, color.x, color.y, color.z, 1.0f, 1.0f, index}, \
        }; \
      \
        indices = { \
            0, 1, 2, 2, 1, 3 \
        }; \
    }
    #endif

    static std::pair<std::vector<Point>, std::vector<uint32_t>> merge(std::pair<std::vector<Point>, std::vector<uint32_t>> p1, std::pair<std::vector<Point>, std::vector<uint32_t>> p2) {
        auto size = p1.first.size();

        std::for_each(p2.second.begin(), p2.second.end(), [size](auto& v) {
            v += size;
        });

        p1.first.insert(p1.first.end(), p2.first.begin(), p2.first.end());
        p1.second.insert(p1.second.end(), p2.second.begin(), p2.second.end());

        return p1;
    }

    #ifndef mergeVerticesDefine
    #define mergeVerticesDefine(p1, p2) \
    { \
            auto size = p1.first.size(); \
            std::for_each(p2.second.begin(), p2.second.end(), [size](auto& v) { \
                v += size; \
            }); \
                \
            p1.first.insert(p1.first.end(), p2.first.begin(), p2.first.end()); \
            p1.second.insert(p1.second.end(), p2.second.begin(), p2.second.end()); \
    }
    #endif

    

    static std::pair<std::vector<Font::Point>, std::vector<uint32_t>> genTextLine(float x, float y, const std::string& line, const std::unordered_map<char, Character>& dictionary, const Grammar* const grammar) {
        auto s = Timer::nowMilliseconds();
        std::pair<std::vector<Font::Point>, std::vector<uint32_t>> result;
        unsigned long long v = 0, m = 0;
        bool isSpace_ = false;
        for (size_t i = 0; i < line.size(); i++) {
            glm::vec2 center;
            center.x = x + dictionary.at(line[i]).offsetX_ + dictionary.at(line[i]).width_ / 2.0f;
            center.y = y + dictionary.at(line[i]).offsetY_ - dictionary.at(line[i]).height_ / 2.0f;

            auto t = Font::vertices(center.x, center.y, dictionary.at(line[i]), dictionary.at(line[i]).color_);

            mergeVerticesDefine(result, t);

            x += dictionary.at(line[i]).advance_;
        }
        // color
        if (grammar != nullptr) {
            auto wordColor = grammar->parseLine(line);
            for (auto& word : wordColor) {
                auto begin = word.first.first;
                auto size = word.first.second;
                auto color = word.second;
                for (int i = begin * 4; i < (begin + size) * 4; i++) {
                    result.first[i].color_ = color;
                }
            }
        }

        return result;
    }

    static std::pair<std::vector<Font::Point>, std::vector<uint32_t>> genTextLines(float x, float y, uint32_t lineHeight, const std::vector<std::string>& lines, const std::unordered_map<char, Character>& dictionary, const Grammar* const grammar) {
        std::pair<std::vector<Font::Point>, std::vector<uint32_t>> result;

        unsigned long long g = 0, m = 0;
        for (auto& line : lines) {
            // auto s = Timer::nowMilliseconds();
            auto pointAndIndex = Font::genTextLine(x, y, line, dictionary, grammar);
            // auto e = Timer::nowMilliseconds();
            // g += e - s;
            
            mergeVerticesDefine(result, pointAndIndex);
            // s = Timer::nowMilliseconds();
            // m += s - e;

            y -= lineHeight;
        }

        // std::cout << std::format("generate ms: {}, merge ms: {}\n", g, m);

        return result;
    }

    static std::pair<std::vector<Font::Point>, std::vector<uint32_t>> genTextLine(float x, float y, const std::string& line, const std::unordered_map<char, Character>& dictionary, const Grammar* const grammar, int32_t leftLimit, int32_t rightLimit) {
        auto s = Timer::nowMilliseconds();
        std::pair<std::vector<Font::Point>, std::vector<uint32_t>> result;
        unsigned long long v = 0, m = 0;
        bool isSpace_ = false;
        for (size_t i = leftLimit; i < line.size() && i < rightLimit; i++) {
            glm::vec2 center;
            center.x = x + dictionary.at(line[i]).offsetX_ + dictionary.at(line[i]).width_ / 2.0f;
            center.y = y + dictionary.at(line[i]).offsetY_ - dictionary.at(line[i]).height_ / 2.0f;

            auto t = Font::vertices(center.x, center.y, dictionary.at(line[i]), dictionary.at(line[i]).color_);

            mergeVerticesDefine(result, t);

            x += dictionary.at(line[i]).advance_;
        }
        // color
        if (grammar != nullptr) {
            auto wordColor = grammar->parseLine(line);
            for (auto& word : wordColor) {
                auto begin = word.first.first;
                auto size = word.first.second;
                auto color = word.second;
                for (int i = begin * 4; i < (begin + size) * 4; i++) {
                    result.first[i].color_ = color;
                }
            }
        }

        return result;
    }

    static std::pair<std::vector<Font::Point>, std::vector<uint32_t>> genTextLines(float x, float y, uint32_t lineHeight, const std::vector<std::string>& lines, const std::unordered_map<char, Character>& dictionary, const Grammar* const grammar, int32_t leftLimit, int32_t rightLimit) {
        std::pair<std::vector<Font::Point>, std::vector<uint32_t>> result;

        unsigned long long g = 0, m = 0;
        for (auto& line : lines) {
            auto pointAndIndex = Font::genTextLine(x, y, line, dictionary, grammar, leftLimit, rightLimit);

            mergeVerticesDefine(result, pointAndIndex);

            y -= lineHeight;
        }

        return result;
    }

    static std::vector<Font::Point> genOneChar(float x, float y, uint32_t lineHeight, char c, const std::unordered_map<char, Character>& dictionary) {
        glm::ivec2 center;
        auto& word = dictionary.at(c);

        center.x = x + word.offsetX_ + word.width_ / 2.0f;
        center.y = y + word.offsetY_ - lineHeight / 2.0f;

        return Font::vertices(center.x, center.y, word, word.color_).first;
    }

    void loadChar(char c);
    void renderMode(FT_Render_Mode mode);
    FT_Bitmap bitmap() { return face_->glyph->bitmap; }
    FT_GlyphSlot glyph() { return face_->glyph; }
    uint32_t advance_ = 0;
    uint32_t lineHeight_ =0 ;
private:
    void check(FT_Error error);

    FT_Library libarry_;
    FT_Face face_;
};