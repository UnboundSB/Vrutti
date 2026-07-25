#include "src/core/terminal/TerminalProcess.h"
#include <iostream>
#include <string>

int main() {
    vrutti::core::terminal::TerminalProcess term;
    bool success = term.start([](const std::string& out) {
        std::cout << "OUT: " << out;
        std::cout.flush();
    });
    std::cout << "start returned: " << success << "\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "exit_test") break;
        term.writeInput(line + "\r\n");
    }
    
    term.stop();
    return 0;
}
