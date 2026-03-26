from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

# Define the C++ extension we want to build
ext_modules = [
    Pybind11Extension(
        "hello",            # The name of the module Python will import
        ["hello.cpp"],      # The source file
    ),
]

setup(
    name="hello",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)