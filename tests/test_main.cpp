// tests/test_main.cpp
#include "test_framework.h"

// Test registration functions
extern auto registerLexerTests(TestRunner& runner) -> void;
extern auto registerParserTests(TestRunner& runner) -> void;

auto main() -> int 
{
    std::cout << "FORTH-ESP32 Compiler Test Suite\n";
    std::cout << "Phase 3: Lexical Analysis Tests\n";
    
    TestRunner runner;
    
    // Register all test suites
    std::cout << "Registering lexer tests...\n";
    registerLexerTests(runner);

    std::cout << "Registering parser tests...\n";
    registerParserTests(runner);

    const int failures = runner.runAll();
    
    if (failures == 0) 
    {
        std::cout << "\n🎉 All tests passed!\n";
        std::cout << "✅ Lexical analysis working correctly\n";
        std::cout << "✅ Parser generating proper AST\n";
        std::cout << "✅ Dictionary system functional\n";
        std::cout << "✅ Error handling working\n";
        std::cout << "\nReady for Phase 4: Semantic Analysis\n";
    } 
    else 
    {
        std::cout << "\n💔 Some tests failed. Check the output above.\n";
    }
    
    return failures;
}
