cmake_minimum_required(VERSION 3.15)
project(game LANGUAGES CXX)

include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake OPTIONAL)

file(GLOB SRC_FILES src/*.cpp)
file(GLOB TEST_SRC tests/*.cpp)

add_executable(game main.cpp ${SRC_FILES})
target_link_libraries(game PRIVATE sfml-system sfml-window sfml-graphics sfml-audio sfml-network)

add_executable(game_tests ${SRC_FILES} ${TEST_SRC})
target_link_libraries(game_tests PRIVATE gtest)
