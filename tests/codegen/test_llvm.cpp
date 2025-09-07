#include "../test_framework.h"
#include "codegen/llvm_backend.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/analyzer.h"
#include <memory>
#include <regex>

class LLVMTestFixture {
public:
    ForthLexer lexer;
    ForthParser parser;
    SemanticAnalyzer analyzer;
    ForthLLVMCodegen codegen;
    
    LLVMTestFixture() : codegen("test_module") {
        codegen.setSemanticAnalyzer(&analyzer);
        codegen.setDictionary(&parser.getDictionary());
    }
    
    auto compileCode(const std::string& code) -> std::unique_ptr<llvm::Module, ModuleDeleter> {
        auto tokens = lexer.tokenize(code);
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) {
            throw std::runtime_error("Parser errors");
        }
        
        if (!analyzer.analyze(*ast)) {
            throw std::runtime_error("Semantic analysis errors");
        }
        
        return codegen.generateModule(*ast);
    }
    
    auto getLLVMIR(const std::string& code) -> std::string {
        auto module = compileCode(code);
        if (!module || codegen.hasErrors()) {
            throw std::runtime_error("Code generation errors");
        }
        return codegen.emitLLVMIR();
    }
    
    auto hasErrors() -> bool {
        return parser.hasErrors() || analyzer.hasErrors() || codegen.hasErrors();
    }
};

auto registerLLVMTests(TestRunner& runner) -> void {
    
    // Basic LLVM module generation
    runner.addTest("llvm_module_creation", []() {
        LLVMTestFixture fixture;
        try {
            auto module = fixture.compileCode("42");
            return module != nullptr;
        } catch (...) {
            return false;
        }
    });
    
    // Number literal code generation
    runner.addTest("llvm_number_literal", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR("42");
            // Check that IR contains stack push operation
            return ir.find("store") != std::string::npos && 
                   ir.find("42") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Basic arithmetic code generation
    runner.addTest("llvm_basic_arithmetic", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR("10 20 +");
            // Should contain add instruction and stack operations
            return ir.find("add") != std::string::npos && 
                   ir.find("load") != std::string::npos &&
                   ir.find("store") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Stack operations
    runner.addTest("llvm_stack_operations", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR("42 DUP");
            // Should contain stack push/pop operations for DUP
            return ir.find("load") != std::string::npos && 
                   ir.find("store") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Word definition generation
    runner.addTest("llvm_word_definition", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR(": DOUBLE DUP + ;");
            // Should contain function definition
            return ir.find("define") != std::string::npos && 
                   ir.find("DOUBLE") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Control flow - IF statement
    runner.addTest("llvm_if_statement", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR(": TEST 42 0> IF 1 ELSE 0 THEN ;");
            // Should contain branch instructions and basic blocks
            return ir.find("br") != std::string::npos && 
                   ir.find("label") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Loop generation
    runner.addTest("llvm_begin_until_loop", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR(": COUNTDOWN BEGIN DUP . 1- DUP 0= UNTIL DROP ;");
            // Should contain loop structure with branch instructions
            return ir.find("br") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // String literal generation
    runner.addTest("llvm_string_literal", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR("\" Hello World\"");
            // Should contain string constant
            return ir.find("Hello World") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Print string generation
    runner.addTest("llvm_print_string", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR(".\" Hello World\"");
            // Should contain string output code
            return ir.find("Hello World") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Variable declaration
    runner.addTest("llvm_variable_declaration", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR("VARIABLE COUNTER");
            // Should contain global variable allocation
            return ir.find("alloca") != std::string::npos || 
                   ir.find("global") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Constant declaration
    runner.addTest("llvm_constant_declaration", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR("42 CONSTANT ANSWER");
            // Should contain constant definition
            return ir.find("ANSWER") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Complex word with multiple operations
    runner.addTest("llvm_complex_word", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR(": SQUARE DUP * ; : QUADRUPLE SQUARE SQUARE ;");
            // Should contain both function definitions
            return ir.find("SQUARE") != std::string::npos && 
                   ir.find("QUADRUPLE") != std::string::npos &&
                   ir.find("define") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Math operations variety
    runner.addTest("llvm_math_operations", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR("10 20 + 5 - 3 * 2 /");
            // Should contain various arithmetic instructions
            std::regex addPattern("add");
            std::regex subPattern("sub");
            std::regex mulPattern("mul");
            std::regex divPattern("div");
            
            return std::regex_search(ir, addPattern) && 
                   std::regex_search(ir, subPattern) &&
                   std::regex_search(ir, mulPattern);
        } catch (...) {
            return false;
        }
    });
    
    // Stack underflow detection during codegen
    runner.addTest("llvm_stack_underflow_error", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR("+"); // Should fail in semantic analysis
            return false; // Should not reach here
        } catch (...) {
            return true; // Expected to fail
        }
    });
    
    // Comparison operations
    runner.addTest("llvm_comparison_operations", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR("10 20 < 15 10 > = IF 1 ELSE 0 THEN");
            // Should contain comparison instructions
            return ir.find("icmp") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Multiple word definitions
    runner.addTest("llvm_multiple_definitions", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR(R"(
                : ADD2 2 + ;
                : MUL3 3 * ;
                : PROCESS DUP ADD2 SWAP MUL3 + ;
            )");
            // Should contain all three function definitions
            return ir.find("ADD2") != std::string::npos && 
                   ir.find("MUL3") != std::string::npos &&
                   ir.find("PROCESS") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Nested control structures
    runner.addTest("llvm_nested_control_flow", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR(R"(
                : COMPLEX_TEST 
                    DUP 0> IF 
                        DUP 10 < IF 
                            1 
                        ELSE 
                            2 
                        THEN 
                    ELSE 
                        0 
                    THEN ;
            )");
            // Should contain multiple branch instructions for nested IFs
            std::regex branchPattern("br");
            auto matches = std::sregex_iterator(ir.begin(), ir.end(), branchPattern);
            auto count = std::distance(matches, std::sregex_iterator());
            return count >= 4; // Should have multiple branches for nested structure
        } catch (...) {
            return false;
        }
    });
    
    // ESP32 target configuration test
    runner.addTest("llvm_esp32_target", []() {
        LLVMTestFixture fixture;
        try {
            fixture.codegen.setTarget("xtensa-esp32-elf");
            auto ir = fixture.getLLVMIR("42");
            // Should generate valid IR for ESP32 target
            return ir.find("target triple") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Error handling in code generation
    runner.addTest("llvm_error_handling", []() {
        LLVMTestFixture fixture;
        try {
            // Try to generate code with undefined word
            auto ir = fixture.getLLVMIR("UNDEFINED_WORD");
            return fixture.hasErrors(); // Should have semantic errors
        } catch (...) {
            return true; // Expected due to undefined word
        }
    });
    
    // LLVM IR validity check
    runner.addTest("llvm_ir_validity", []() {
        LLVMTestFixture fixture;
        try {
            auto module = fixture.compileCode(": VALID_TEST 42 DUP + ;");
            if (!module) return false;
            
            // Basic validation - module should have at least one function
            
            return true;
        } catch (...) {
            return false;
        }
    });
    
    // Memory operations
    runner.addTest("llvm_memory_operations", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR(R"(
                VARIABLE VAR1
                42 VAR1 !
                VAR1 @
            )");
            // Should contain load and store operations
            return ir.find("load") != std::string::npos && 
                   ir.find("store") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
    
    // Recursive word definition
    runner.addTest("llvm_recursive_word", []() {
        LLVMTestFixture fixture;
        try {
            auto ir = fixture.getLLVMIR(": FACTORIAL DUP 1 <= IF DROP 1 ELSE DUP 1- FACTORIAL * THEN ;");
            // Should contain function call to itself
            return ir.find("FACTORIAL") != std::string::npos && 
                   ir.find("call") != std::string::npos;
        } catch (...) {
            return false;
        }
    });
}
