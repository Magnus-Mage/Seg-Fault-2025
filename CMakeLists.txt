cmake_minimum_required(VERSION 3.16)

project(forth_esp32_compiler 
    VERSION 0.2.0
    DESCRIPTION "FORTH Compiler targeting ESP32 with LLVM backend"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
option(BUILD_TESTS "Build test suite" ON)
option(WITH_LLVM "Enable LLVM backend (requires LLVM installation)" OFF)
option(ENABLE_DEBUG "Enable debug output" OFF)

# Source files
set(SOURCES
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/ast.cpp
    src/parser/parser.cpp
    src/dictionary/dictionary.cpp
    src/semantic/analyzer.cpp
    src/codegen/llvm_backend.cpp
    src/common/utils.h
)

# Include directories
include_directories(src)

# Main executable
add_executable(forth_compiler ${SOURCES})

target_compile_features(forth_compiler PRIVATE cxx_std_20)

# Compiler flags
target_compile_options(forth_compiler PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

if(ENABLE_DEBUG)
    target_compile_definitions(forth_compiler PRIVATE ENABLE_DEBUG_OUTPUT)
endif()

# Libraries
target_link_libraries(forth_compiler m)

# LLVM integration (optional)
if(WITH_LLVM)
    find_package(LLVM REQUIRED CONFIG)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
    
    target_include_directories(forth_compiler PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(forth_compiler PRIVATE ${LLVM_DEFINITIONS})
    target_compile_definitions(forth_compiler PRIVATE WITH_REAL_LLVM)
    
    # Find required LLVM components
    llvm_map_components_to_libnames(llvm_libs support core irreader)
    target_link_libraries(forth_compiler ${llvm_libs})
    
    message(STATUS "LLVM integration enabled")
else()
    message(STATUS "Using mock LLVM implementation for testing")
endif()

# Testing
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Test suite enabled")
endif()

# Installation
install(TARGETS forth_compiler
    RUNTIME DESTINATION bin
    COMPONENT runtime)

# Documentation
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md
        DESTINATION share/doc/forth_compiler
        COMPONENT documentation)
endif()

# Print configuration summary
message(STATUS "=== FORTH ESP32 Compiler Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Tests: ${BUILD_TESTS}")
message(STATUS "LLVM Backend: ${WITH_LLVM}")
message(STATUS "Debug Output: ${ENABLE_DEBUG}")
message(STATUS "===========================================")
