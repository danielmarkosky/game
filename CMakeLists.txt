cmake_minimum_required(VERSION 3.15)
project(game LANGUAGES CXX)

include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake OPTIONAL)

file(GLOB SRC_FILES src/*.cpp)
file(GLOB TEST_FILES tests/*.cpp)
file(GLOB LEVELS_FILES levels/*.cpp)
file(GLOB CHARACTER_FILES characters/*.cpp)
file(GLOB DEBUG_FILES debug/*.cpp)

find_package(Boost REQUIRED COMPONENTS program_options)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
find_package(GTest REQUIRED)

add_executable(game main.cpp ${SRC_FILES} ${LEVELS_FILES} ${CHARACTER_FILES})
target_link_libraries(game PRIVATE sfml-graphics sfml-window sfml-system Boost::program_options)
target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(game_tests ${SRC_FILES} ${LEVELS_FILES} ${TEST_FILES})
target_link_libraries(game_tests PRIVATE gtest sfml-graphics sfml-window sfml-system Boost::program_options)
target_include_directories(game_tests PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(game_debug ${SRC_FILES} ${LEVELS_FILES} ${DEBUG_FILES} ${CHARACTER_FILES})
target_link_libraries(game_debug PRIVATE sfml-graphics sfml-window sfml-system Boost::program_options)
target_include_directories(game_debug PRIVATE ${CMAKE_SOURCE_DIR})
