#ifndef FORTH_TYPES_H
#define FORTH_TYPES_H

#include <string>
#include <vector>
#include <memory>

// Forward declarations
class ASTNode;
class ForthDictionary;

// Basic data types for FORTH
using ForthInt = int32_t;
using ForthFloat = float;
using ForthBool = bool;

// Token definition
enum class TokenType {
    // Basic types
    NUMBER,
    WORD,
    STRING,
    COMMENT,
    
    // Control structures  
    COLON_DEF,      // :
    SEMICOLON,      // ;
    IF,
    THEN, 
    ELSE,
    BEGIN,
    UNTIL,
    DO,
    LOOP,
    
    // Math operations (detected during lexing)
    MATH_WORD,
    
    // Threading operations (for future)
    THREAD_WORD,
    
    // Special
    EOF_TOKEN,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
    
    Token(TokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c) {}
};

#endif // FORTH_TYPES_H
