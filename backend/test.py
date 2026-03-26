import os

# Explicitly tell Python where to find the MSYS2 UCRT64 DLLs 
# (Make sure this path matches your actual MSYS2 installation)
msys2_bin_path = r"C:\msys64\ucrt64\bin"

# add_dll_directory is only available on Windows Python 3.8+
if hasattr(os, 'add_dll_directory') and os.path.exists(msys2_bin_path):
    os.add_dll_directory(msys2_bin_path)

# Now Python has permission to load the required threading DLLs
import hello

# Instantiate the C++ object, passing the name argument
my_greeter = hello.Greeter("Vrutti Developer")

# Call the public method, which executes the private C++ 'greet' method
result = my_greeter.say_hello()

print(f"C++ Engine says: {result}")