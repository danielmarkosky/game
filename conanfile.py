from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout

class GameConan(ConanFile):
    name = "game"
    version = "0.1"
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "CMakeToolchain"
    exports_sources = "src/*", "CMakeLists.txt"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("sfml/2.5.1")
        self.requires("gtest/1.14.0")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["game"]
