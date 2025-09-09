add_executable(test_forth_compiler
    test_main.cpp
    lexer/test_lexer.cpp
    parser/test_parser.cpp
    semantic/test_analyzer.cpp
    codegen/test_c_backend.cpp
    
    # Source files
    ../src/lexer/lexer.cpp
    ../src/parser/ast.cpp
    ../src/parser/parser.cpp
    ../src/dictionary/dictionary.cpp
    ../src/semantic/analyzer.cpp
    ../src/codegen/c_backend.cpp
)

target_include_directories(test_forth_compiler PRIVATE ../src)

# Add test
add_test(NAME forth_compiler_tests COMMAND test_forth_compiler)

# Compiler flags for Phase 4 - Updated for C backend
target_compile_options(test_forth_compiler PRIVATE
    -Wall -Wextra -Wpedantic
    -std=c++20
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

# Add C code generation definitions
target_compile_definitions(test_forth_compiler PRIVATE 
    WITH_C_CODEGEN
    TESTING_MODE
)

# Math library for tests
target_link_libraries(test_forth_compiler PRIVATE m)

# Create test data directory
add_custom_command(TARGET test_forth_compiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test_data
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/tests/test_data ${CMAKE_BINARY_DIR}/test_data
    COMMENT "Copying test data files"
)

# Integration test with ESP-IDF project creation
add_custom_target(test_esp32_integration
    COMMAND $<TARGET_FILE:test_forth_compiler> --integration-test
    DEPENDS test_forth_compiler
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running ESP32 integration tests"
)
