# Vrutti IDE - Platform Independent Makefile
# Works natively with MSYS2/MinGW, Linux (GCC/Clang), and macOS

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -I./src -I./build/_deps/webview2-src/build/native/include

# Directories
SRC_DIR = src
OBJ_DIR = build/obj
BIN_DIR = build

OS := $(shell uname -s 2>/dev/null || echo Windows_NT)

ifeq ($(OS), Windows_NT)
	TARGET = $(BIN_DIR)/vrutti_app.exe
	LDFLAGS += -lole32 -lcomctl32 -loleaut32 -luuid -lgdi32 -lshlwapi -lws2_32 -lkernel32 -ladvapi32 -lversion -static
else ifeq ($(OS), Linux)
	TARGET = $(BIN_DIR)/vrutti_app
	CXXFLAGS += $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.0)
	LDFLAGS += $(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.0)
else ifeq ($(OS), Darwin)
	TARGET = $(BIN_DIR)/vrutti_app
	LDFLAGS += -framework WebKit -framework Cocoa
else
	# Fallback for MSYS/MinGW environments where uname -s might return something else
	TARGET = $(BIN_DIR)/vrutti_app.exe
	LDFLAGS += -lole32 -lcomctl32 -loleaut32 -luuid -lgdi32 -lshlwapi -lws2_32 -lkernel32 -ladvapi32 -lversion -static
endif

# Source files
SRCS = $(SRC_DIR)/app/main.cpp \
       $(SRC_DIR)/core/memory/ArenaAllocator.cpp \
       $(SRC_DIR)/core/concurrency/ThreadPool.cpp \
       $(SRC_DIR)/core/config/SettingsManager.cpp \
       $(SRC_DIR)/core/editor/PieceTable.cpp \
       $(SRC_DIR)/core/fs/URI.cpp \
       $(SRC_DIR)/core/fs/Path.cpp \
       $(SRC_DIR)/core/fs/Glob.cpp \
       $(SRC_DIR)/core/fs/Workspace.cpp \
       $(SRC_DIR)/core/plugins/PluginLoader.cpp \
       $(SRC_DIR)/core/utils/Json.cpp \
       $(SRC_DIR)/core/utils/StringPool.cpp \
       $(SRC_DIR)/core/utils/LineScanner.cpp \
       $(SRC_DIR)/core/ipc/IPCClient.cpp \
       $(SRC_DIR)/core/terminal/ReplProcess.cpp \
       $(SRC_DIR)/core/terminal/TerminalProcess.cpp \
       $(SRC_DIR)/plugins/search/SearchPlugin.cpp \
       $(SRC_DIR)/ui/compositor/Window.cpp

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "[SUCCESS] Build complete: $@"

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf build/

.PHONY: all clean
