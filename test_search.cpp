#include <iostream>
#include <windows.h>
#include "src/core/plugins/IPlugin.h"

typedef vrutti::core::plugins::IPlugin* (*CreatePluginFunc)();

int main() {
    HMODULE hLib = LoadLibraryA("build/libvrutti_search.dll");
    if (!hLib) {
        std::cerr << "Failed to load DLL\n";
        return 1;
    }
    
    CreatePluginFunc createPlugin = (CreatePluginFunc)GetProcAddress(hLib, "CreatePlugin");
    if (!createPlugin) {
        std::cerr << "Failed to find createPlugin\n";
        return 1;
    }
    
    vrutti::core::plugins::IPlugin* plugin = createPlugin();
    plugin->initialize();
    
    std::string payload = "{\"query\": \"pi\", \"directory\": \".\", \"matchCase\": false, \"wholeWord\": false, \"useRegex\": false, \"isReplace\": false, \"replaceString\": \"\"}";
    
    std::string result = plugin->executeCommand("search", payload);
    
    std::cout << "RESULT:\n" << result << "\n";
    
    plugin->shutdown();
    delete plugin;
    FreeLibrary(hLib);
    
    return 0;
}
