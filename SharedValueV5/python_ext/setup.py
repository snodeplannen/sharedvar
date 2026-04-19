import os
import sys
from setuptools import setup, Extension
from pybind11.setup_helpers import Pybind11Extension, build_ext

__version__ = "0.4.0"

cpp_core_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "cpp_core"))

ext_modules = [
    Pybind11Extension(
        "sharedvalue5",
        ["src/main.cpp"],
        define_macros=[("VERSION_INFO", __version__)],
        include_dirs=[cpp_core_dir],
        cxx_std=17,
        language="c++",
    ),
]

setup(
    name="sharedvalue5",
    version=__version__,
    author="SharedValue Project",
    author_email="example@example.com",
    description="Python bindings for the SharedValueV5 IPC Engine",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.7",
)
