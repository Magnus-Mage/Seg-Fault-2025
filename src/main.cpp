#include <iostream>
#include <fstream>
#include <string>
#include "lexer/lexer.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <forth_file>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Error: Cannot open file " << argv[1] << std::endl;
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    ForthLexer lexer;
    try {
        auto tokens = lexer.tokenize(source);
        
        std::cout << "=== TOKENIZATION RESULTS ===" << std::endl;
        for (const auto& token : tokens) {
            std::cout << "Type: " << lexer.tokenTypeToString(token.type) 
                      << ", Value: '" << token.value << "'"
                      << ", Line: " << token.line 
                      << ", Column: " << token.column << std::endl;
        }
        
        std::cout << "\nTotal tokens: " << tokens.size() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Lexical error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
