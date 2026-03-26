from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "hello",
        ["hello.cpp"],
        include_dirs=[pybind11.get_include()],
        language='c++',
        # The compiler flags (optimization and modern C++)
        extra_compile_args=['-std=c++17', '-O3'],
        # The long-term fix: Force static linking of C++ standard libraries
        extra_link_args=['-static-libgcc', '-static-libstdc++'],
    ),
]

setup(
    name="hello",
    ext_modules=ext_modules,
)