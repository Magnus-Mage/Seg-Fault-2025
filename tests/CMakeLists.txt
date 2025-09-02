# Find or download a simple testing framework (using built-in approach for now)
add_executable(test_lexer
    test_main.cpp
    lexer/test_lexer.cpp
    ../src/lexer/lexer.cpp
)

target_include_directories(test_lexer PRIVATE ../src)

# Add test
add_test(NAME lexer_tests COMMAND test_lexer)
