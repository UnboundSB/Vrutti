#include <iostream>
#include "core/memory/ArenaAllocator.h"
#include "core/concurrency/ThreadPool.h"
#include "core/events/Event.h"
#include "core/editor/PieceTable.h"
#include "core/fs/URI.h"
#include <string>

int main() {
    std::cout << "Vrutti IDE Core Initialization..." << std::endl;

    // TODO: Initialize core subsystems (Memory, Concurrency, VFS, Editor Buffers)
    // TODO: Launch application window and compositor

    return 0;
}
