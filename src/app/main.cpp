#include <iostream>
#include "ui/compositor/Window.h"

int main() {
    std::cout << "--- Vrutti IDE Core ---" << std::endl;
    
    // Launch UI
    vrutti::ui::Window window(1280, 720, "Vrutti IDE");
    if (!window.init()) {
        std::cerr << "Failed to initialize UI window!" << std::endl;
        return 1;
    }

    // Run the main IDE render loop
    window.run();

    return 0;
}
