add_executable(test_lexer
    test_main.cpp
    lexer/test_lexer.cpp
    parser/test_parser.cpp
    ../src/lexer/lexer.cpp
    ../src/parser/ast.cpp
    ../src/parser/parser.cpp
    ../src/dictionary/dictionary.cpp

)

target_include_directories(test_forth_compiler PRIVATE ../src)

# Add test
add_test(NAME forth_compiler_tests COMMAND test_forth_compiler)
