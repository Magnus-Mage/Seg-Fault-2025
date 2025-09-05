#ifndef FORTH_PARSER_H
#define FORTH_PARSER_H

#include <vector>
#include <memory>
#include <stack>
#include "common/types.h"
#include "parser/ast.h"
#include "dictionary/dictionary.h"

class ForthParser {
private:
    std::vector<Token> tokens;
    size_t currentPos;
    std::unique_ptr<ForthDictionary> dictionary;
    
    // Parser state
    std::stack<std::unique_ptr<ASTNode>> nodeStack;
    std::vector<std::string> errors;
    
    // Control flow tracking
    std::stack<TokenType> controlStack;
    
public:
    ForthParser();
    explicit ForthParser(std::unique_ptr<ForthDictionary> dict);
    ~ForthParser() = default;
    
    // Main parsing interface
    [[nodiscard]] auto parseProgram(const std::vector<Token>& tokenList) -> std::unique_ptr<ProgramNode>;
    
    // Error handling
    [[nodiscard]] auto hasErrors() const -> bool { return !errors.empty(); }
    [[nodiscard]] auto getErrors() const -> const std::vector<std::string>& { return errors; }
    auto clearErrors() -> void { errors.clear(); }
    
    // Dictionary access
    [[nodiscard]] auto getDictionary() const -> const ForthDictionary& { return *dictionary; }
    auto getDictionary() -> ForthDictionary& { return *dictionary; }
    
    // Collect all the words when called
    auto collectWordDefinations() -> void;
    
    // Parser statistics
    struct ParseStatistics {
        size_t totalTokens;
        size_t wordsDefinitions;
        size_t mathOperations;
        size_t controlStructures;
        size_t variables;
        size_t constants;
        bool hasThreadingWords;
        bool hasAdvancedMath;
    };
    
    [[nodiscard]] auto getStatistics() const -> ParseStatistics;
    
private:
    // Token management
    [[nodiscard]] auto currentToken() const -> const Token&;
    [[nodiscard]] auto peekToken(size_t offset = 1) const -> const Token&;
    [[nodiscard]] auto isAtEnd() const -> bool;
    auto advance() -> void;
    auto expect(TokenType type) -> bool;
    auto consume(TokenType type, const std::string& errorMsg) -> bool;
    
    // Core parsing methods
    auto parseStatement() -> std::unique_ptr<ASTNode>;
    auto parseWordDefinition() -> std::unique_ptr<WordDefinitionNode>;
    auto parseControlFlow() -> std::unique_ptr<ASTNode>;
    auto parseIfStatement() -> std::unique_ptr<IfStatementNode>;
    auto parseBeginUntilLoop() -> std::unique_ptr<BeginUntilLoopNode>;
    auto parseVariableDeclaration() -> std::unique_ptr<VariableDeclarationNode>;
    auto parseConstantDeclaration() -> std::unique_ptr<VariableDeclarationNode>;
    
    // Expression parsing
    auto parseExpression() -> std::unique_ptr<ASTNode>;
    auto parsePrimaryExpression() -> std::unique_ptr<ASTNode>;
    
    // Utility methods
    auto addError(const std::string& message) -> void;
    auto addError(const std::string& message, const Token& token) -> void;
    [[nodiscard]] auto validateControlFlow() const -> bool;
    [[nodiscard]] auto isControlFlowToken(TokenType type) const -> bool;
    [[nodiscard]] auto isEndOfDefinition() const -> bool;
    auto validateForwardReferences() -> void;
    
    // Semantic validation during parsing
    auto validateStackBalance(ASTNode* node) -> bool;
    auto analyzeWordUsage(const std::string& wordName) -> void;
};

#endif // FORTH_PARSER_H
