#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <iostream>
#include "Editor.h"

int main() {
    char m[10];
    snprintf(m, 10, "%4d", 0);
    std::string s = m;
    for (auto c : s) {
        if (c == ' ') {
            std::cout << "1";
        }
    }
}