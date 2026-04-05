import os

# 1. THE BYPASS: Tell Python where to find the MinGW C++ libraries
# (Ensure this path matches your system!)
msys2_bin_path = r"C:\msys64\ucrt64\bin"
if hasattr(os, 'add_dll_directory') and os.path.exists(msys2_bin_path):
    os.add_dll_directory(msys2_bin_path)

# 2. THE IMPORT: Load your C++ Engine
import vrutti_core

print("====================================")
print("  VRUTTI ENGINE INITIALIZATION")
print("====================================\n")

# Instantiate the C++ class
doc = vrutti_core.PieceTable("Hello World")

print("--- BEFORE INSERTION ---")
print(f"Text:   '{doc.get_text()}'")
print(f"Length: {doc.length()} characters\n")


print("--- TRIGGERING BOSS FIGHT (INSERTION) ---")
# Insert "Beautiful " exactly at index 6 (right after "Hello ")
doc.insert_text(6, "Beautiful ")


print("--- AFTER INSERTION ---")
print(f"Text:   '{doc.get_text()}'")
print(f"Length: {doc.length()} characters\n")