#include "src/core/terminal/TerminalProcess.h"
#include <iostream>
#include <iomanip>
#include <string>

int main() {
    vrutti::core::terminal::TerminalProcess term;
    bool success = term.start([](const std::string& out) {
        std::cout << "OUT (" << out.length() << " chars): ";
        for (char c : out) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (0xff & (unsigned int)c) << " ";
        }
        std::cout << std::dec << std::endl;
        std::cout.flush();
    });
    std::cout << "start returned: " << success << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "exit_test") break;
        term.writeInput(line + "\r\n");
    }
    
    term.stop();
    return 0;
}
