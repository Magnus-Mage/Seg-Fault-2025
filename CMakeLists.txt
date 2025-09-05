cmake_minimum_required(VERSION 3.16)

project(forth_esp32_compiler 
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cpp
    src/lexer/lexer.cpp
    ./src/parser/ast.cpp
    ./src/parser/parser.cpp
    ./src/dictionary/dictionary.cpp
)

# Include directories
include_directories(src)

# Main executable
add_executable(forth_compiler ${SOURCES})

# Enable testing
enable_testing()
add_subdirectory(tests)

# Compiler flags
target_compile_options(forth_compiler PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# Add math library for advanced math functions
target_link_libraries(forth_compiler m)
