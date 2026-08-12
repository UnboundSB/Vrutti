#include "src/core/config/SettingsManager.h"
#include <iostream>
int main() { std::cout << vrutti::core::config::SettingsManager::getInstance().getSettingsJson() << std::endl; return 0; }
