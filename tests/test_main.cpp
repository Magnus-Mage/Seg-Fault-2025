// tests/test_main.cpp
#include "test_framework.h"

// Test registration functions
extern auto registerLexerTests(TestRunner& runner) -> void;

auto main() -> int {
    std::cout << "FORTH-ESP32 Compiler Test Suite\n";
    std::cout << "Phase 2: Lexical Analysis Tests\n";
    
    TestRunner runner;
    
    // Register all test suites
    registerLexerTests(runner);
    
    const int failures = runner.runAll();
    
    if (failures == 0) {
        std::cout << "\n🎉 All tests passed!\n";
    } else {
        std::cout << "\n💔 Some tests failed. Check the output above.\n";
    }
    
    return failures;
}
