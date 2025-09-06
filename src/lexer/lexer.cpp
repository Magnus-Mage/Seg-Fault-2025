// src/lexer/lexer.cpp
#include "lexer/lexer.h"
#include "common/utils.h"
#include <stdexcept>
#include <cctype>
#include <sstream>

ForthLexer::ForthLexer() 
    : currentLine{1}, currentColumn{1}, currentPos{0} {
    
    // Initialize control words
    controlWords = {
        ":", ";", "IF", "THEN", "ELSE", "BEGIN", "UNTIL", 
        "DO", "LOOP", "WHILE", "REPEAT", "VARIABLE", "CONSTANT"
    };
    
    // Initialize math words for detection
    mathWords = {
        // Basic arithmetic
        "+", "-", "*", "/", "MOD", "ABS", "NEGATE", "MIN", "MAX",
        "1+", "1-", "2*", "2/",

        // Comparison operations
        "<", "<=", ">", ">=", "=", "<>", "0<", "0=", "0>",
        
        // Advanced math
        "SQRT", "SIN", "COS", "TAN", "ASIN", "ACOS", "ATAN", "ATAN2",
        "LOG", "LOG10", "EXP", "EXP10", "POWER", "POW",
        
        // Bitwise operations
        "AND", "OR", "XOR", "NOT", "LSHIFT", "RSHIFT", "INVERT"
    };
}

[[nodiscard]] auto ForthLexer::tokenize(const std::string& sourceCode) -> std::vector<Token> {
    source = sourceCode;
    currentPos = 0;
    currentLine = 1;
    currentColumn = 1;
    
    std::vector<Token> tokens;
    tokens.reserve(sourceCode.length() / 4); // Rough estimation
    
    while (currentPos < source.length()) {
        skipWhitespace();
        
        if (currentPos >= source.length()) {
            break;
        }
        
        const char ch = currentChar();
        
        // Handle comments
        if (ch == '\\' || (ch == '(' && peekChar() == ' ')) {
            skipComment();
            continue;
        }
        
        // Handle strings
        if (ch == '"' || (ch == '.' && peekChar() == '"')) {
            tokens.emplace_back(readString());
            continue;
        }
        
        // Handle numbers
        if (std::isdigit(ch) || (ch == '-' && std::isdigit(peekChar()))) {
            tokens.emplace_back(readNumber());
            continue;
        }
        
        // Handle words and control structures
        if (std::isgraph(ch)) {
            tokens.emplace_back(readWord());
            continue;
        }
        
        // Unknown character
        throw std::runtime_error(
            "Unknown character '" + std::string(1, ch) + 
            "' at line " + std::to_string(currentLine) + 
            ", column " + std::to_string(currentColumn)
        );
    }
    
    tokens.emplace_back(TokenType::EOF_TOKEN, "", currentLine, currentColumn);
    return tokens;
}

[[nodiscard]] auto ForthLexer::currentChar() const -> char {
    return (currentPos < source.length()) ? source[currentPos] : '\0';
}

[[nodiscard]] auto ForthLexer::peekChar() const -> char {
    return (currentPos + 1 < source.length()) ? source[currentPos + 1] : '\0';
}

auto ForthLexer::advance() -> void {
    if (currentPos < source.length()) {
        if (source[currentPos] == '\n') {
            currentLine++;
            currentColumn = 1;
        } else {
            currentColumn++;
        }
        currentPos++;
    }
}

auto ForthLexer::skipWhitespace() -> void {
    while (currentPos < source.length() && std::isspace(currentChar())) {
        advance();
    }
}

auto ForthLexer::skipComment() -> void {
    const char ch = currentChar();
    
    if (ch == '\\') {
        // Line comment: skip to end of line
        while (currentPos < source.length() && currentChar() != '\n') {
            advance();
        }
    } else if (ch == '(' && peekChar() == ' ') {
        // Block comment: skip to ')'
        advance(); // skip '('
        advance(); // skip ' '
        
        while (currentPos < source.length() && currentChar() != ')') {
            advance();
        }
        
        if (currentPos < source.length()) {
            advance(); // skip closing ')'
        }
    }
}

[[nodiscard]] auto ForthLexer::readNumber() -> Token {
    const int startLine = currentLine;
    const int startColumn = currentColumn;
    std::string number;
    
    // Handle negative sign
    if (currentChar() == '-') {
        number += currentChar();
        advance();
    }
    
    // Read digits and optional decimal point
    bool hasDecimal = false;
    while (currentPos < source.length()) {
        const char ch = currentChar();
        
        if (std::isdigit(ch)) {
            number += ch;
            advance();
        } else if (ch == '.' && !hasDecimal) {
            hasDecimal = true;
            number += ch;
            advance();
        } else {
            break;
        }
    }
    
    // Validate the number
    if (!ForthUtils::isNumber(number)) {
        throw std::runtime_error(
            "Invalid number format '" + number + 
            "' at line " + std::to_string(startLine) + 
            ", column " + std::to_string(startColumn)
        );
    }
    
    return Token{TokenType::NUMBER, number, startLine, startColumn};
}

[[nodiscard]] auto ForthLexer::readWord() -> Token {
    const int startLine = currentLine;
    const int startColumn = currentColumn;
    std::string word;
    
    // Read word characters (everything except whitespace and special chars)
    while (currentPos < source.length()) {
        const char ch = currentChar();
        
        if (std::isspace(ch) || ch == '"' || ch == '\\' || ch == '(' || ch == ')') {
            break;
        }
        
        word += ch;
        advance();
    }
    
    if (word.empty()) {
        throw std::runtime_error(
            "Empty word at line " + std::to_string(startLine) + 
            ", column " + std::to_string(startColumn)
        );
    }
    
    const TokenType type = classifyWord(word);
    return Token{type, word, startLine, startColumn};
}

[[nodiscard]] auto ForthLexer::readString() -> Token {
    const int startLine = currentLine;
    const int startColumn = currentColumn;
    std::string str;
    
    // Handle ." strings
    if (currentChar() == '.' && peekChar() == '"') {
        advance(); // skip '.'
        advance(); // skip '"'
        
        while (currentPos < source.length() && currentChar() != '"') {
            str += currentChar();
            advance();
        }
        
        if (currentPos >= source.length()) {
            throw std::runtime_error(
                "Unterminated string at line " + std::to_string(startLine) + 
                ", column " + std::to_string(startColumn)
            );
        }
        
        advance(); // skip closing '"'
        return Token{TokenType::STRING, "." + str, startLine, startColumn};
    }
    
    // Handle regular " strings
    if (currentChar() == '"') {
        advance(); // skip opening '"'
        
        while (currentPos < source.length() && currentChar() != '"') {
            str += currentChar();
            advance();
        }
        
        if (currentPos >= source.length()) {
            throw std::runtime_error(
                "Unterminated string at line " + std::to_string(startLine) + 
                ", column " + std::to_string(startColumn)
            );
        }
        
        advance(); // skip closing '"'
        return Token{TokenType::STRING, str, startLine, startColumn};
    }
    
    throw std::runtime_error("Internal error in readString");
}

[[nodiscard]] auto ForthLexer::classifyWord(const std::string& word) -> TokenType {
    const std::string upperWord = ForthUtils::toUpper(word);
    
    // Check control words first
    if (controlWords.contains(upperWord)) {
        // Special handling for specific control words
        if (upperWord == ":") return TokenType::COLON_DEF;
        if (upperWord == ";") return TokenType::SEMICOLON;
        if (upperWord == "IF") return TokenType::IF;
        if (upperWord == "THEN") return TokenType::THEN;
        if (upperWord == "ELSE") return TokenType::ELSE;
        if (upperWord == "BEGIN") return TokenType::BEGIN;
        if (upperWord == "UNTIL") return TokenType::UNTIL;
        if (upperWord == "DO") return TokenType::DO;
        if (upperWord == "LOOP") return TokenType::LOOP;
        
        return TokenType::WORD; // Other control words
    }
    
    // Check math words
    if (mathWords.contains(upperWord)) {
        return TokenType::MATH_WORD;
    }
    
    // Default to regular word
    return TokenType::WORD;
}

[[nodiscard]] auto ForthLexer::tokenTypeToString(TokenType type) -> std::string {
    switch (type) {
        case TokenType::NUMBER:      return "NUMBER";
        case TokenType::WORD:        return "WORD";
        case TokenType::STRING:      return "STRING";
        case TokenType::COMMENT:     return "COMMENT";
        case TokenType::COLON_DEF:   return "COLON_DEF";
        case TokenType::SEMICOLON:   return "SEMICOLON";
        case TokenType::IF:          return "IF";
        case TokenType::THEN:        return "THEN";
        case TokenType::ELSE:        return "ELSE";
        case TokenType::BEGIN:       return "BEGIN";
        case TokenType::UNTIL:       return "UNTIL";
        case TokenType::DO:          return "DO";
        case TokenType::LOOP:        return "LOOP";
        case TokenType::MATH_WORD:   return "MATH_WORD";
        case TokenType::THREAD_WORD: return "THREAD_WORD";
        case TokenType::EOF_TOKEN:   return "EOF";
        case TokenType::UNKNOWN:     return "UNKNOWN";
        default:                     return "INVALID";
    }
}
