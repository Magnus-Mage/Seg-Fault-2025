#include "../test_framework.h"
#include "codegen/c_backend.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/analyzer.h"
#include <sstream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

auto registerCCodegenTests(TestRunner& runner) -> void {
    
    runner.addTest("C Backend Basic Initialization", []() -> bool {
        ForthCCodegen codegen("test_program");
        codegen.setTarget("esp32");
        
        // Should initialize without errors
        return !codegen.hasErrors();
    });
    
    runner.addTest("Simple Number Generation", []() -> bool {
        // Test: "42"
        ForthLexer lexer;
        auto tokens = lexer.tokenize("42");
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) return false;
        
        ForthCCodegen codegen("test_num");
        bool success = codegen.generateCode(*ast);
        
        if (!success || codegen.hasErrors()) return false;
        
        std::string code = codegen.getCompleteCode();
        
        // Should contain stack push operation
        return code.find("forth_push_int(42)") != std::string::npos;
    });
    
    runner.addTest("Math Operation Generation", []() -> bool {
        // Test: "5 3 +"
        ForthLexer lexer;
        auto tokens = lexer.tokenize("5 3 +");
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) return false;
        
        ForthCCodegen codegen("test_math");
        bool success = codegen.generateCode(*ast);
        
        if (!success || codegen.hasErrors()) return false;
        
        std::string code = codegen.getCompleteCode();
        
        // Should contain push operations and add call
        return code.find("forth_push_int(5)") != std::string::npos &&
               code.find("forth_push_int(3)") != std::string::npos &&
               code.find("forth_add()") != std::string::npos;
    });
    
    runner.addTest("Word Definition Generation", []() -> bool {
        // Test: ": DOUBLE DUP + ;"
        ForthLexer lexer;
        auto tokens = lexer.tokenize(": DOUBLE DUP + ;");
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) return false;
        
        ForthCCodegen codegen("test_word");
        bool success = codegen.generateCode(*ast);
        
        if (!success || codegen.hasErrors()) return false;
        
        std::string code = codegen.getCompleteCode();
        
        // Should generate function definition
        return code.find("void forth_word_double(void)") != std::string::npos &&
               code.find("forth_dup()") != std::string::npos &&
               code.find("forth_add()") != std::string::npos;
    });
    
    runner.addTest("String Literal Generation", []() -> bool {
        // Test: ".\" Hello World\""
        ForthLexer lexer;
        auto tokens = lexer.tokenize(".\" Hello World\"");
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) return false;
        
        ForthCCodegen codegen("test_string");
        bool success = codegen.generateCode(*ast);
        
        if (!success || codegen.hasErrors()) return false;
        
        std::string code = codegen.getCompleteCode();
        
        // Should contain printf call
        return code.find("printf(\"Hello World\")") != std::string::npos;
    });
    
    runner.addTest("If Statement Generation", []() -> bool {
        // Test: "1 IF 42 THEN"
        ForthLexer lexer;
        auto tokens = lexer.tokenize("1 IF 42 THEN");
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) return false;
        
        ForthCCodegen codegen("test_if");
        bool success = codegen.generateCode(*ast);
        
        if (!success || codegen.hasErrors()) return false;
        
        std::string code = codegen.getCompleteCode();
        
        // Should contain C if statement
        return code.find("if (forth_pop_int() != 0)") != std::string::npos &&
               code.find("forth_push_int(42)") != std::string::npos;
    });
    
    runner.addTest("Begin-Until Loop Generation", []() -> bool {
        // Test: "BEGIN 1 - DUP 0 = UNTIL"
        ForthLexer lexer;
        auto tokens = lexer.tokenize("BEGIN 1 - DUP 0 = UNTIL");
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) return false;
        
        ForthCCodegen codegen("test_loop");
        bool success = codegen.generateCode(*ast);
        
        if (!success || codegen.hasErrors()) return false;
        
        std::string code = codegen.getCompleteCode();
        
        // Should contain do-while loop
        return code.find("do {") != std::string::npos &&
               code.find("} while (forth_pop_int() == 0)") != std::string::npos;
    });
    
    runner.addTest("Variable Declaration", []() -> bool {
        // Test: "VARIABLE COUNTER"
        ForthLexer lexer;
        auto tokens = lexer.tokenize("VARIABLE COUNTER");
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) return false;
        
        ForthCCodegen codegen("test_var");
        bool success = codegen.generateCode(*ast);
        
        if (!success || codegen.hasErrors()) return false;
        
        std::string code = codegen.getCompleteCode();
        
        // Should contain variable declaration
        return code.find("static forth_int_t forth_var_counter") != std::string::npos;
    });
    
    runner.addTest("Runtime Header Generation", []() -> bool {
        ForthCCodegen codegen("test_runtime");
        bool success = codegen.generateCode(ProgramNode{});
        
        if (!success) return false;
        
        std::string header = codegen.getHeaderCode();
        
        // Should contain essential runtime functions
        return header.find("forth_push_int") != std::string::npos &&
               header.find("forth_pop_int") != std::string::npos &&
               header.find("forth_add") != std::string::npos &&
               header.find("forth_program_main") != std::string::npos;
    });
    
    runner.addTest("File Output Generation", []() -> bool {
        ForthCCodegen codegen("test_output");
        bool success = codegen.generateCode(ProgramNode{});
        
        if (!success) return false;
        
        // Create temp directory for test
        fs::path tempDir = fs::temp_directory_path() / "forth_test";
        fs::create_directories(tempDir);
        
        std::string basePath = tempDir / "test_program";
        bool fileSuccess = codegen.writeToFiles(basePath);
        
        if (!fileSuccess) return false;
        
        // Check files exist and contain expected content
        bool headerExists = fs::exists(basePath + ".h");
        bool sourceExists = fs::exists(basePath + ".c");
        
        // Cleanup
        fs::remove_all(tempDir);
        
        return headerExists && sourceExists;
    });
    
    runner.addTest("ESP-IDF Project Generation", []() -> bool {
        ForthCCodegen codegen("esp32_test");
        codegen.setTarget("esp32");
        
        bool success = codegen.generateCode(ProgramNode{});
        if (!success) return false;
        
        // Create temp directory for ESP-IDF project
        fs::path tempDir = fs::temp_directory_path() / "forth_esp32_test";
        
        try {
            bool projectSuccess = codegen.writeESPIDFProject(tempDir);
            
            if (!projectSuccess) return false;
            
            // Check essential ESP-IDF files exist
            bool rootCMakeExists = fs::exists(tempDir / "CMakeLists.txt");
            bool mainExists = fs::exists(tempDir / "main" / "main.cpp");
            bool componentExists = fs::exists(tempDir / "components" / "forth_compiler" / "CMakeLists.txt");
            
            // Cleanup
            fs::remove_all(tempDir);
            
            return rootCMakeExists && mainExists && componentExists;
            
        } catch (const std::exception&) {
            // Cleanup on failure
            if (fs::exists(tempDir)) {
                fs::remove_all(tempDir);
            }
            return false;
        }
    });
    
    runner.addTest("Complex Program Integration", []() -> bool {
        // Test a more complex FORTH program
        std::string complexProgram = R"(
            : FACTORIAL 
                DUP 1 <= IF 
                    DROP 1 
                ELSE 
                    DUP 1 - FACTORIAL * 
                THEN 
            ;
            
            VARIABLE RESULT
            5 FACTORIAL RESULT !
        )";
        
        ForthLexer lexer;
        auto tokens = lexer.tokenize(complexProgram);
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) return false;
        
        SemanticAnalyzer analyzer(&parser.getDictionary());
        bool semanticSuccess = analyzer.analyze(*ast);
        
        // Semantic analysis might have warnings but should not fail
        if (analyzer.hasErrors()) return false;
        
        ForthCCodegen codegen("factorial_test");
        codegen.setSemanticAnalyzer(&analyzer);
        codegen.setDictionary(&parser.getDictionary());
        
        bool success = codegen.generateCode(*ast);
        
        if (!success || codegen.hasErrors()) return false;
        
        std::string code = codegen.getCompleteCode();
        
        // Should contain function definition and variable
        return code.find("forth_word_factorial") != std::string::npos &&
               code.find("forth_var_result") != std::string::npos &&
               code.find("if (forth_pop_int() != 0)") != std::string::npos;
    });
    
    runner.addTest("Error Handling", []() -> bool {
        // Test with invalid/problematic code
        ForthLexer lexer;
        auto tokens = lexer.tokenize("UNDEFINED_WORD");
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        ForthCCodegen codegen("error_test");
        bool success = codegen.generateCode(*ast);
        
        // Should either handle gracefully or report errors appropriately
        // This test ensures the codegen doesn't crash on problematic input
        return true; // If we get here without crashing, that's good
    });
    
    runner.addTest("Code Generation Statistics", []() -> bool {
        std::string program = "42 DUP + .\" Result: \" .";
        
        ForthLexer lexer;
        auto tokens = lexer.tokenize(program);
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) return false;
        
        ForthCCodegen codegen("stats_test");
        bool success = codegen.generateCode(*ast);
        
        if (!success) return false;
        
        auto stats = codegen.getStatistics();
        
        // Should have reasonable statistics
        return stats.linesGenerated > 0 && stats.linesGenerated < 10000;
    });
}
