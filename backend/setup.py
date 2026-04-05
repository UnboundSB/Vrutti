import glob
from setuptools import Extension, setup
import pybind11

cpp_sources = glob.glob("core/src/*.cpp")+glob.glob("core/bindings/*.cpp")

vrutti_extension = Extension(name = "vrutti_core",
                              sources=cpp_sources,
                                include_dirs=["core/include", pybind11.get_include()],
                             language="c++", 
                             #Mingw compiler Flags
                             extra_compile_args=["-std=c++17", "-O3"],
                             extra_link_args=["-static-libgcc", "-static-libstdc++"]
                             )
setup(name="vrutti_core", ext_modules=[vrutti_extension])