#include <iostream>
#include "src/core/plugins/ExtensionScanner.h"

int main() {
    std::string json = vrutti::core::plugins::ExtensionScanner::getInstalledExtensions();
    std::cout << json << std::endl;
    return 0;
}
