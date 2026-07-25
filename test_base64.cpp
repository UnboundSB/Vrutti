#include "src/core/utils/Base64.h"
#include <iostream>

int main() {
    std::string s1 = "hello world";
    std::string s2 = "\x1b[?9001h";
    std::cout << base64_encode(s1) << std::endl;
    std::cout << base64_encode(s2) << std::endl;
    return 0;
}
