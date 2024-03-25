#include "Keyboard.h"

Keyboard::Keyboard(unsigned long long deltatIime) : delteTime_(deltatIime) {
    
}

void Keyboard::press(int key, unsigned long long currTime) {
    keyPress_[key] = true;
    keyPressTime_[key] = currTime;
}

void Keyboard::release(int key) {
    keyPress_[key] = false;
}

bool Keyboard::processKey(int key, unsigned long long currTime) {
    if (currTime - keyPressTime_[key] >= delteTime_) {
        keyPressTime_[key] = currTime;
        return true;
    } else {
        return false;
    }
}

const std::unordered_map<int, bool>& Keyboard::keyPress() {
    return keyPress_;
}