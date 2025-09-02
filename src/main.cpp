// src/main.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>

#include "lexer/lexer.h"
#include "common/utils.h"

namespace fs = std::filesystem;
using namespace std::chrono;

[[nodiscard]] auto readSourceFile(const std::string& filename) -> std::string {
    if (!fs::exists(filename)) {
        throw std::runtime_error("File does not exist: " + filename);
    }
    
    std::ifstream file{filename};
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    return std::string{
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{}
    };
}

auto printTokenizationResults(const std::vector<Token>& tokens) -> void {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "TOKENIZATION RESULTS\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    std::cout << std::left;
    std::cout << std::setw(12) << "Type" 
              << std::setw(20) << "Value" 
              << std::setw(8) << "Line" 
              << std::setw(8) << "Column" << "\n";
    std::cout << std::string(48, '-') << "\n";
    
    for (const auto& token : tokens) {
        if (token.type == TokenType::EOF_TOKEN) continue;
        
        std::string displayValue = token.value;
        if (displayValue.length() > 18) {
            displayValue = displayValue.substr(0, 15) + "...";
        }
        
        std::cout << std::setw(12) << ForthLexer{}.tokenTypeToString(token.type)
                  << std::setw(20) << ("'" + displayValue + "'")
                  << std::setw(8) << token.line
                  << std::setw(8) << token.column << "\n";
    }
    
    std::cout << "\nTotal tokens: " << (tokens.size() - 1) << " (excluding EOF)\n"; // -1 for EOF
}

auto printStatistics(const std::vector<Token>& tokens, 
                    const high_resolution_clock::duration& duration) -> void {
    std::cout << "\n" << std::string(40, '=') << "\n";
    std::cout << "STATISTICS\n";
    std::cout << std::string(40, '=') << "\n";
    
    // Count token types
    int numbers = 0, words = 0, mathWords = 0, controlWords = 0, strings = 0;
    
    for (const auto& token : tokens) {
        switch (token.type) {
            case TokenType::NUMBER:                                numbers++; break;
            case TokenType::WORD:                                  words++; break;
            case TokenType::MATH_WORD:                            mathWords++; break;
            case TokenType::STRING:                               strings++; break;
            case TokenType::COLON_DEF: case TokenType::SEMICOLON:
            case TokenType::IF: case TokenType::THEN: case TokenType::ELSE:
            case TokenType::BEGIN: case TokenType::UNTIL:
            case TokenType::DO: case TokenType::LOOP:             controlWords++; break;
            default: break;
        }
    }
    
    std::cout << "Numbers:        " << numbers << "\n";
    std::cout << "Words:          " << words << "\n";
    std::cout << "Math words:     " << mathWords << "\n";
    std::cout << "Control words:  " << controlWords << "\n";
    std::cout << "Strings:        " << strings << "\n";
    
    const auto ms = duration_cast<microseconds>(duration);
    std::cout << "Lexing time:    " << ms.count() << " μs\n";
    
    if (tokens.size() > 1) { // Exclude EOF token
        const auto tokensPerSecond = (tokens.size() - 1) * 1'000'000 / ms.count();
        std::cout << "Tokens/second:  " << tokensPerSecond << "\n";
    }
}

auto main(int argc, char* argv[]) -> int {
    std::cout << "FORTH-ESP32 Compiler v0.1.0\n";
    std::cout << "Phase 2: Lexical Analysis\n\n";
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <forth_file> [options]\n";
        std::cerr << "Options:\n";
        std::cerr << "  -v, --verbose    Show detailed token information\n";
        std::cerr << "  -s, --stats      Show lexing statistics\n";
        return 1;
    }
    
    const std::string filename{argv[1]};
    bool verbose = false;
    bool showStats = false;
    
    // Parse command line options
    for (int i = 2; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-s" || arg == "--stats") {
            showStats = true;
        }
    }
    
    try {
        // Read source file
        std::cout << "Reading file: " << filename << "\n";
        const auto source = readSourceFile(filename);
        
        if (source.empty()) {
            std::cout << "Warning: File is empty\n";
            return 0;
        }
        
        std::cout << "Source size: " << source.size() << " bytes\n";
        
        // Tokenize with timing
        ForthLexer lexer;
        const auto startTime = high_resolution_clock::now();
        
        const auto tokens = lexer.tokenize(source);
        
        const auto endTime = high_resolution_clock::now();
        const auto duration = endTime - startTime;
        
        // Display results
        if (verbose || tokens.size() <= 50) { // Always show if small
            printTokenizationResults(tokens);
        } else {
            std::cout << "\nTokenization completed successfully!\n";
            std::cout << "Use -v/--verbose to see detailed token list\n";
        }
        
        if (showStats || verbose) {
            printStatistics(tokens, duration);
        }
        
        // Basic validation
        bool hasDefinitions = false;
        bool hasMath = false;
        
        for (const auto& token : tokens) {
            if (token.type == TokenType::COLON_DEF) hasDefinitions = true;
            if (token.type == TokenType::MATH_WORD) hasMath = true;
        }
        
        std::cout << "\n" << std::string(40, '-') << "\n";
        std::cout << "Features detected:\n";
        std::cout << "- Word definitions: " << (hasDefinitions ? "Yes" : "No") << "\n";
        std::cout << "- Math operations:  " << (hasMath ? "Yes" : "No") << "\n";
        std::cout << "- Threading:        Not implemented yet\n";
        
        std::cout << "\n✅ Lexical analysis completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
