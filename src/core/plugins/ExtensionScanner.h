#pragma once

#include <string>
#include <vector>
#include "../utils/Json.h"

namespace vrutti {
namespace core {
namespace plugins {

class ExtensionScanner {
public:
    static std::string getExtensionsDir();
    static std::string getInstalledExtensions();
};

} // namespace plugins
} // namespace core
} // namespace vrutti
