cmake_minimum_required(VERSION 3.16)

project(forth_esp32_compiler
    VERSION 0.3.0
    DESCRIPTION "FORTH Compiler targeting ESP32 with C code generation"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_ESP_IDF "Build as ESP-IDF component" OFF)
option(ENABLE_DEBUG "Enable debug output" OFF)
option(ENABLE_OPTIMIZATIONS "Enable code generation optimizations" ON)

# Source files - Updated to use C codegen instead of LLVM
set(SOURCES
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/ast.cpp
    src/parser/parser.cpp
    src/dictionary/dictionary.cpp
    src/semantic/analyzer.cpp
    src/codegen/c_backend.cpp
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

if(ENABLE_OPTIMIZATIONS)
    target_compile_definitions(forth_compiler PRIVATE ENABLE_CODE_OPTIMIZATIONS)
endif()

# Math library
target_link_libraries(forth_compiler PRIVATE m)

# === C Code Generation Backend ===
# No external dependencies needed - pure C++ implementation
target_compile_definitions(forth_compiler PRIVATE WITH_C_CODEGEN)

message(STATUS "Using C code generation backend (no LLVM dependency)")

# === ESP-IDF Component Mode ===
if(BUILD_ESP_IDF)
    # When building as ESP-IDF component, we need different structure
    message(STATUS "Building as ESP-IDF component")
    target_compile_definitions(forth_compiler PRIVATE ESP_IDF_COMPONENT=1)
    
    # ESP-IDF specific settings would go here
    # This is handled by the ESP-IDF build system when used as component
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

# Install ESP-IDF template project
install(DIRECTORY esp32_template/
    DESTINATION share/forth_compiler/esp32_template
    COMPONENT esp32_support
    PATTERN "*")

# === Summary ===
message(STATUS "=== FORTH ESP32 Compiler Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Tests: ${BUILD_TESTS}")
message(STATUS "ESP-IDF Mode: ${BUILD_ESP_IDF}")
message(STATUS "Code Generation: C Backend (no LLVM)")
message(STATUS "Debug Output: ${ENABLE_DEBUG}")
message(STATUS "Optimizations: ${ENABLE_OPTIMIZATIONS}")
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
    COMMAND ${CMAKE_COMMAND} -E echo ": BLINK 13 OUTPUT 1000 0 DO 13 HIGH 500 DELAY 13 LOW 500 DELAY LOOP ;" > ${CMAKE_BINARY_DIR}/examples/blink.fth
    COMMAND ${CMAKE_COMMAND} -E echo ": TEMPERATURE-MONITOR BEGIN ADC-READ .\\\" Temperature: \\\" . .\\\" C\\\" CR 1000 DELAY AGAIN ;" > ${CMAKE_BINARY_DIR}/examples/temp_monitor.fth
    COMMENT "Generating example FORTH files")

add_dependencies(forth_compiler generate_examples)

# === ESP-IDF Integration Targets ===
add_custom_target(create_esp32_project
    COMMAND ${CMAKE_COMMAND} -E echo "Creating ESP32 project template..."
    COMMAND $<TARGET_FILE:forth_compiler> --create-esp32-project ${CMAKE_BINARY_DIR}/esp32_project
    DEPENDS forth_compiler
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Creating ESP32 project template")

add_custom_target(test_esp32_build
    COMMAND ${CMAKE_COMMAND} -E echo "Testing ESP32 build..."
    COMMAND cd ${CMAKE_BINARY_DIR}/esp32_project && idf.py build
    DEPENDS create_esp32_project
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Testing ESP32 build process")

# === Code Generation Testing ===
add_custom_target(test_codegen
    COMMAND $<TARGET_FILE:forth_compiler> ${CMAKE_BINARY_DIR}/examples/hello.fth -c --output ${CMAKE_BINARY_DIR}/hello_generated.c
    COMMAND $<TARGET_FILE:forth_compiler> ${CMAKE_BINARY_DIR}/examples/factorial.fth -c --output ${CMAKE_BINARY_DIR}/factorial_generated.c
    DEPENDS forth_compiler generate_examples
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Testing C code generation")

# === Documentation Generation ===
find_program(DOXYGEN_EXECUTABLE doxygen)
if(DOXYGEN_EXECUTABLE)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating documentation with Doxygen"
        VERBATIM)
    
    message(STATUS "Documentation generation available (make docs)")
else()
    message(STATUS "Doxygen not found - documentation generation disabled")
endif()

# === Performance Benchmarks ===
add_custom_target(benchmark
    COMMAND $<TARGET_FILE:forth_compiler> --benchmark ${CMAKE_SOURCE_DIR}/benchmarks/
    DEPENDS forth_compiler
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running performance benchmarks")

# === Static Analysis (if tools available) ===
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE} 
            --enable=all 
            --std=c++20 
            --platform=unix64 
            --check-config
            --suppressions-list=${CMAKE_SOURCE_DIR}/.cppcheck-suppressions
            ${CMAKE_SOURCE_DIR}/src
        COMMENT "Running cppcheck static analysis")
    
    message(STATUS "Static analysis available (make cppcheck)")
endif()

# === Packaging ===
set(CPACK_PACKAGE_NAME "forth-esp32-compiler")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "FORTH ESP32 Project")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Platform-specific packaging
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
endif()

include(CPack)

# === IDE Integration ===
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    message(STATUS "Compile commands will be exported for IDE integration")
endif()

# Set IDE folder structure
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(forth_compiler PROPERTIES FOLDER "Main")

if(BUILD_TESTS)
    set_target_properties(test_forth_compiler PROPERTIES FOLDER "Tests")
endif()

# === Final Status Report ===
message(STATUS "")
message(STATUS "🎯 FORTH ESP32 Compiler Ready")
message(STATUS "   Version: ${PROJECT_VERSION}")
message(STATUS "   Backend: C Code Generation")
message(STATUS "   Target: ESP32 via ESP-IDF")
message(STATUS "")
message(STATUS "📋 Available Targets:")
message(STATUS "   make forth_compiler     - Build main compiler")
if(BUILD_TESTS)
message(STATUS "   make test_forth_compiler - Build and run tests")
message(STATUS "   make run_tests          - Run test suite")
endif()
message(STATUS "   make create_esp32_project - Create ESP32 template")
message(STATUS "   make test_codegen       - Test C code generation")
message(STATUS "   make generate_examples  - Create example FORTH files")
message(STATUS "")
message(STATUS "🚀 Next Steps:")
message(STATUS "   1. make forth_compiler")
message(STATUS "   2. make test_forth_compiler")
message(STATUS "   3. make create_esp32_project")
message(STATUS "   4. cd build/esp32_project && idf.py build")
message(STATUS "")
