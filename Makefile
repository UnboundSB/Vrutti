# Vrutti IDE - Platform Independent Makefile
# Works natively with MSYS2/MinGW, Linux (GCC/Clang), and macOS

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -I./src

# Directories
SRC_DIR = src
OBJ_DIR = build/obj
BIN_DIR = build/bin

# Source files (add new ones here or use wildcard)
SRCS = $(SRC_DIR)/app/main.cpp \
       $(SRC_DIR)/core/memory/ArenaAllocator.cpp

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Target Executable
TARGET = $(BIN_DIR)/vrutti_app

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "[SUCCESS] Build complete: $@"

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf build/

.PHONY: all clean
