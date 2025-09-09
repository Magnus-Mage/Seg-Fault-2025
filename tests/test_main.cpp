#include "test_framework.h"

// Test registration functions
extern auto registerLexerTests(TestRunner& runner) -> void;
extern auto registerParserTests(TestRunner& runner) -> void;
extern auto registerSemanticTests(TestRunner& runner) -> void;
extern auto registerCCodegenTests(TestRunner& runner) -> void;  // Updated from LLVM to C

auto main() -> int 
{
    std::cout << "FORTH-ESP32 Compiler Test Suite\n";
    std::cout << "Phase 4: Semantic Analysis & C Code Generation Tests\n";
    
    TestRunner runner;
    
    // Register all test suites
    std::cout << "Registering lexer tests...\n";
    registerLexerTests(runner);

    std::cout << "Registering parser tests...\n";
    registerParserTests(runner);

    std::cout << "Registering semantic analysis tests...\n";
    registerSemanticTests(runner);
    
    std::cout << "Registering C code generation tests...\n";
    registerCCodegenTests(runner);  // Updated from LLVM

    const int failures = runner.runAll();
    
    if (failures == 0) 
    {
        std::cout << "\n🎉 All tests passed!\n";
        std::cout << "✅ Lexical analysis working correctly\n";
        std::cout << "✅ Parser generating proper AST\n";
        std::cout << "✅ Dictionary system functional\n";
        std::cout << "✅ Semantic analysis operational\n";
        std::cout << "✅ C code generation working\n";
        std::cout << "✅ Stack effect analysis functional\n";
        std::cout << "✅ Error handling working\n";
        std::cout << "\nReady for Phase 5: ESP32 Integration & Optimization\n";
    } 
    else 
    {
        std::cout << "\n💔 Some tests failed. Check the output above.\n";
        std::cout << "Fix failing tests before proceeding to Phase 5.\n";
    }
    
    return failures;
}
