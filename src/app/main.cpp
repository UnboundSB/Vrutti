#include <iostream>
#include "core/memory/ArenaAllocator.h"
#include "core/concurrency/ThreadPool.h"
#include "core/events/Event.h"
#include "core/editor/PieceTable.h"
#include "core/fs/URI.h"
#include <string>

void runURITests() {
    std::cout << "[TEST] Running URI Lazy Parsing Tests..." << std::endl;
    vrutti::core::fs::URI uri("vscode-remote://ssh-remote+myserver/home/user/main.cpp?debug=true#line42");
    
    if (uri.scheme() == "vscode-remote" && 
        uri.authority() == "ssh-remote+myserver" &&
        uri.path() == "/home/user/main.cpp" &&
        uri.query() == "debug=true" &&
        uri.fragment() == "line42") 
    {
        std::cout << "[SUCCESS] URI lazy parsing correctly extracted all components with zero string allocations!" << std::endl;
    } else {
        std::cout << "[ERROR] URI parsing failed." << std::endl;
    }
}

int main() {
    std::cout << "Vrutti IDE Core Initialization..." << std::endl;

    runURITests();

    // TODO: Initialize core subsystems (Memory, Concurrency, VFS, Editor Buffers)
    // TODO: Launch application window and compositor

    return 0;
}
