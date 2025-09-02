// tests/lexer/test_lexer.cpp
#include "lexer/lexer.h"
#include <cassert>
#include <iostream>
#include "../test_framework.h"

auto registerLexerTests(TestRunner& runner) -> void {
    
    runner.addTest("Basic Numbers", []() {
        ForthLexer lexer;
        auto tokens = lexer.tokenize("42 -17 3.14");
        
        assert(tokens.size() == 4); // 3 numbers + EOF
        assert(tokens[0].type == TokenType::NUMBER);
        assert(tokens[0].value == "42");
        assert(tokens[1].type == TokenType::NUMBER);
        assert(tokens[1].value == "-17");
        assert(tokens[2].type == TokenType::NUMBER);
        assert(tokens[2].value == "3.14");
        assert(tokens[3].type == TokenType::EOF_TOKEN);
        
        return true;
    });
    
    runner.addTest("Control Words", []() {
        ForthLexer lexer;
        auto tokens = lexer.tokenize(": HELLO if then ;");
        
        assert(tokens.size() == 6); // 5 tokens + EOF
        assert(tokens[0].type == TokenType::COLON_DEF);
        assert(tokens[0].value == ":");
        assert(tokens[1].type == TokenType::WORD);
        assert(tokens[1].value == "HELLO");
        assert(tokens[2].type == TokenType::IF);
        assert(tokens[2].value == "if");
        assert(tokens[3].type == TokenType::THEN);
        assert(tokens[3].value == "then");
        assert(tokens[4].type == TokenType::SEMICOLON);
        assert(tokens[4].value == ";");
        
        return true;
    });
    
    runner.addTest("Math Words", []() {
        ForthLexer lexer;
        auto tokens = lexer.tokenize("+ - * / SQRT SIN COS");
        
        assert(tokens.size() == 8); // 7 math words + EOF
        for (int i = 0; i < 7; ++i) {
            assert(tokens[i].type == TokenType::MATH_WORD);
        }
        
        return true;
    });
    
    runner.addTest("Strings", []() {
        ForthLexer lexer;
        auto tokens = lexer.tokenize(".\" Hello World\" \"test\"");
        
        assert(tokens.size() == 3); // 2 strings + EOF
        assert(tokens[0].type == TokenType::STRING);
        assert(tokens[0].value == ".Hello World");
        assert(tokens[1].type == TokenType::STRING);
        assert(tokens[1].value == "test");
        
        return true;
    });
    
    runner.addTest("Comments", []() {
        ForthLexer lexer;
        auto tokens = lexer.tokenize("42 \\ this is a comment\n17 ( block comment ) 99");
        
        assert(tokens.size() == 4); // 3 numbers + EOF (comments are skipped)
        assert(tokens[0].type == TokenType::NUMBER);
        assert(tokens[0].value == "42");
        assert(tokens[1].type == TokenType::NUMBER);
        assert(tokens[1].value == "17");
        assert(tokens[2].type == TokenType::NUMBER);
        assert(tokens[2].value == "99");
        
        return true;
    });
    
    runner.addTest("Line/Column Tracking", []() {
        ForthLexer lexer;
        auto tokens = lexer.tokenize("42\n  17\n    99");
        
        assert(tokens.size() == 4); // 3 numbers + EOF
        assert(tokens[0].line == 1 && tokens[0].column == 1);  // 42
        assert(tokens[1].line == 2 && tokens[1].column == 3);  // 17
        assert(tokens[2].line == 3 && tokens[2].column == 5);  // 99
        
        return true;
    });
    
    runner.addTest("Complex Program", []() {
        ForthLexer lexer;
        std::string program = R"(
            : SQUARE DUP * ;
            : DISTANCE SWAP DUP * SWAP DUP * + SQRT ;
            42 SQUARE .
            3.0 4.0 DISTANCE .
        )";
        
        auto tokens = lexer.tokenize(program);
        
        // Should parse without errors and produce reasonable token count
        assert(tokens.size() > 10);
        assert(tokens.back().type == TokenType::EOF_TOKEN);
        
        // Check that we have definitions
        bool hasColon = false, hasSemicolon = false, hasMath = false;
        for (const auto& token : tokens) {
            if (token.type == TokenType::COLON_DEF) hasColon = true;
            if (token.type == TokenType::SEMICOLON) hasSemicolon = true;
            if (token.type == TokenType::MATH_WORD) hasMath = true;
        }
        
        assert(hasColon && hasSemicolon && hasMath);
        return true;
    });
    
    runner.addTest("Error Handling - Unterminated String", []() {
        ForthLexer lexer;
        
        try {
            lexer.tokenize("42 \"unterminated string");
            return false; // Should have thrown
        } catch (const std::exception&) {
            return true; // Expected exception
        }
    });
    
    runner.addTest("Error Handling - Invalid Number", []() {
        ForthLexer lexer;
        
        try {
            lexer.tokenize("42 12.34.56 17"); // Invalid number with two decimal points
            return false; // Should have thrown
        } catch (const std::exception&) {
            return true; // Expected exception
        }
    });
    
    runner.addTest("Case Insensitivity", []() {
        ForthLexer lexer;
        auto tokens = lexer.tokenize("if IF If iF");
        
        assert(tokens.size() == 5); // 4 IFs + EOF
        for (int i = 0; i < 4; ++i) {
            assert(tokens[i].type == TokenType::IF);
        }
        
        return true;
    });
}
