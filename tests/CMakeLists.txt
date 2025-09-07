add_executable(test_forth_compiler
    test_main.cpp
    lexer/test_lexer.cpp
    parser/test_parser.cpp
    semantic/test_analyzer.cpp
    codegen/test_llvm.cpp
    
    # Source files
    ../src/lexer/lexer.cpp
    ../src/parser/ast.cpp
    ../src/parser/parser.cpp
    ../src/dictionary/dictionary.cpp
    ../src/semantic/analyzer.cpp
    ../src/codegen/llvm_backend.cpp
)

target_include_directories(test_forth_compiler PRIVATE ../src)

# Add test
add_test(NAME forth_compiler_tests COMMAND test_forth_compiler)

# Compiler flags for Phase 4
target_compile_options(test_forth_compiler PRIVATE
    -Wall -Wextra -Wpedantic
    -std=c++20
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)
