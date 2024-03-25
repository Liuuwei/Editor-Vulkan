#pragma once

#include <unordered_map>

class Keyboard {
public:
    Keyboard(unsigned long long deltaTime);

    void press(int key, unsigned long long currTime);
    void release(int key);
    bool processKey(int key, unsigned long long currTime);
    const std::unordered_map<int, bool>& keyPress();

private:
    std::unordered_map<int, unsigned long long> keyPressTime_;
    std::unordered_map<int, bool> keyPress_;
    unsigned long long delteTime_ = 0;
};