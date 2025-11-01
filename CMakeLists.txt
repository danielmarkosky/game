cmake_minimum_required(VERSION 3.15)
project(game LANGUAGES CXX)

include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake OPTIONAL)

find_package(Boost REQUIRED COMPONENTS program_options)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
find_package(GTest REQUIRED)

file(GLOB SRC_FILES src/*.cpp)
file(GLOB TEST_FILES tests/*.cpp)

add_executable(game main.cpp ${SRC_FILES})
target_link_libraries(game PRIVATE sfml-graphics sfml-window sfml-system Boost::program_options)
target_include_directories(game PRIVATE ${CMAKE_SOURCE_DIR})

add_executable(game_tests ${SRC_FILES} ${TEST_FILES})
target_link_libraries(game_tests
        PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
        Boost::program_options
        GTest::gtest
)
target_include_directories(game_tests PRIVATE ${CMAKE_SOURCE_DIR})
