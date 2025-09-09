cmake_minimum_required(VERSION 3.16)

project(forth_esp32_compiler
    VERSION 0.2.0
    DESCRIPTION "FORTH Compiler targeting ESP32 with LLVM backend"
    LANGUAGES C CXX)

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
)

# Include directories
include_directories(src)

# Main executable
add_executable(forth_compiler ${SOURCES})

target_compile_features(forth_compiler PRIVATE cxx_std_20)

target_compile_options(forth_compiler PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG -fsanitize=address>
    $<$<CONFIG:Release>:-O3 -DNDEBUG -DNDEBUG_STACK_CHECKS>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(forth_compiler PRIVATE -fsanitize=address)
endif()

if(ENABLE_DEBUG)
    target_compile_definitions(forth_compiler PRIVATE ENABLE_DEBUG_OUTPUT)
endif()

# Math library
target_link_libraries(forth_compiler PRIVATE m)

# === LLVM Integration ===
if(WITH_LLVM)
    find_package(LLVM REQUIRED CONFIG)

    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
    message(STATUS "LLVM Include Dirs: ${LLVM_INCLUDE_DIRS}")
    message(STATUS "LLVM Library Dirs: ${LLVM_LIBRARY_DIRS}")

    include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

    target_include_directories(forth_compiler PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_directories(forth_compiler PRIVATE ${LLVM_LIBRARY_DIRS})
    target_compile_definitions(forth_compiler PRIVATE ${LLVM_DEFINITIONS})
    target_compile_definitions(forth_compiler PRIVATE WITH_REAL_LLVM)

    if(COMMAND llvm_map_components_to_libnames)
        llvm_map_components_to_libnames(llvm_libs
            support core irreader bitwriter analysis
            target transformutils codegen mc passes nativecodegen
        )
        target_link_libraries(forth_compiler PRIVATE ${llvm_libs})
    else()
        message(WARNING "llvm_map_components_to_libnames() is not available. Manually specify LLVM libraries if needed.")
    endif()
endif()

# === Testing ===
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Test suite enabled")
endif()

# === Installation ===
install(TARGETS forth_compiler
    RUNTIME DESTINATION bin
    COMPONENT runtime)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    install(FILES README.md
        DESTINATION share/doc/forth_compiler
        COMPONENT documentation)
endif()

install(DIRECTORY examples/
    DESTINATION share/forth_compiler/examples
    COMPONENT examples
    PATTERN "*.fth"
    PATTERN "README*")

# === Summary ===
message(STATUS "=== FORTH ESP32 Compiler Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Tests: ${BUILD_TESTS}")
message(STATUS "LLVM Backend: ${WITH_LLVM}")
message(STATUS "Debug Output: ${ENABLE_DEBUG}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "===========================================")

# === Development Targets ===
add_custom_target(run_tests
    COMMAND ctest --output-on-failure
    DEPENDS forth_compiler
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running test suite")

add_custom_target(clean_build
    COMMAND ${CMAKE_COMMAND} --build . --target clean
    COMMAND ${CMAKE_COMMAND} --build .
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Clean rebuild")

add_custom_target(generate_examples
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/examples
    COMMAND ${CMAKE_COMMAND} -E echo ": HELLO .\\\" Hello, World!\\\" CR ;" > ${CMAKE_BINARY_DIR}/examples/hello.fth
    COMMAND ${CMAKE_COMMAND} -E echo ": FACTORIAL DUP 1 <= IF DROP 1 ELSE DUP 1- FACTORIAL * THEN ;" > ${CMAKE_BINARY_DIR}/examples/factorial.fth
    COMMENT "Generating example FORTH files")

add_dependencies(forth_compiler generate_examples)

