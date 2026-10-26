cmake_minimum_required(VERSION 3.15)
project(game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake OPTIONAL)

find_package(SFML 2.6 COMPONENTS graphics window system audio REQUIRED)
find_package(nlohmann_json REQUIRED)

file(GLOB SRC_FILES CONFIGURE_DEPENDS src/*.cpp)

# Everything except main() lives in one library so the game and the tests share a single compile.
add_library(game_core STATIC ${SRC_FILES})
target_link_libraries(game_core PUBLIC sfml-graphics sfml-window sfml-system sfml-audio nlohmann_json::nlohmann_json)
target_include_directories(game_core PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_definitions(game_core PUBLIC ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
target_compile_options(game_core PUBLIC
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
)

# Conan's OpenAL asks for libc++ twice; harmless, but the linker says so every build.
if (APPLE)
    add_link_options(LINKER:-no_warn_duplicate_libraries)
endif ()

add_executable(game main.cpp)
target_link_libraries(game PRIVATE game_core)

# Checks whether rooms can be got through: see tools/levelbot.cpp and tools/check_world.py.
add_executable(levelbot tools/levelbot.cpp)
target_link_libraries(levelbot PRIVATE game_core)

# Lays rooms out with the mouse: see tools/editor.cpp.
add_executable(editor tools/editor.cpp)
target_link_libraries(editor PRIVATE game_core)

find_package(GTest QUIET)
if (GTest_FOUND)
    file(GLOB TEST_FILES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(game_tests ${TEST_FILES})
    target_link_libraries(game_tests PRIVATE game_core GTest::gtest)
endif ()
