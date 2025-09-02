#ifndef FORTH_LEXER_H
#define FORTH_LEXER_H

#include <vector>
#include <string>
#include <unordered_set>
#include "common/types.h"

class ForthLexer {
private:
    std::unordered_set<std::string> controlWords;
    std::unordered_set<std::string> mathWords;
    
    int currentLine;
    int currentColumn;
    size_t currentPos;
    std::string source;
    
    char currentChar();
    char peekChar();
    void advance();
    void skipWhitespace();
    void skipComment();
    
    Token readNumber();
    Token readWord();
    Token readString();
    
    TokenType classifyWord(const std::string& word);
    
public:
    ForthLexer();
    std::vector<Token> tokenize(const std::string& source);
    std::string tokenTypeToString(TokenType type);
};

#endif // FORTH_LEXER_H
