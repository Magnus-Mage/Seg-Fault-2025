#include "parser/parser.h"
#include "dictionary/dictionary.h"
#include "common/utils.h"
#include <stdexcept>
#include <sstream>

ForthParser::ForthParser() : currentPos(0) {
    dictionary = DictionaryFactory::create(DictionaryFactory::Configuration::STANDARD);
}

ForthParser::ForthParser(std::unique_ptr<ForthDictionary> dict) 
    : currentPos(0), dictionary(std::move(dict)) {
}

[[nodiscard]] auto ForthParser::parseProgram(const std::vector<Token>& tokenList) -> std::unique_ptr<ProgramNode> {
    tokens = tokenList;
    currentPos = 0;
    errors.clear();
    
    auto program = std::make_unique<ProgramNode>();
    
    while (!isAtEnd() && currentToken().type != TokenType::EOF_TOKEN) {
        try {
            auto statement = parseStatement();
            if (statement) {
                program->addChild(std::move(statement));
            }
        } catch (const std::exception& e) {
            addError("Parse error: " + std::string(e.what()), currentToken());
            // Skip to next statement boundary
            while (!isAtEnd() && currentToken().type != TokenType::SEMICOLON && 
                   currentToken().type != TokenType::EOF_TOKEN) {
                advance();
            }
            if (currentToken().type == TokenType::SEMICOLON) {
                advance();
            }
        }
    }
    
    // Validate control flow is balanced
    if (!validateControlFlow()) {
        addError("Unmatched control flow structures");
    }
    
    return program;
}

auto ForthParser::parseStatement() -> std::unique_ptr<ASTNode> {
    const auto& token = currentToken();
    
    switch (token.type) {
        case TokenType::COLON_DEF:
            return parseWordDefinition();
            
        case TokenType::IF:
            return parseIfStatement();
            
        case TokenType::BEGIN:
            return parseBeginUntilLoop();
            
        case TokenType::NUMBER:
            return parsePrimaryExpression();
            
        case TokenType::STRING:
            return parsePrimaryExpression();
            
        case TokenType::MATH_WORD: {
            auto mathNode = std::make_unique<MathOperationNode>(token.value, token.line, token.column);
            advance();
            return mathNode;
        }
        
        case TokenType::WORD: {
            const std::string wordName = ForthUtils::toUpper(token.value);
            
            // Check for special words
            if (wordName == "VARIABLE") {
                return parseVariableDeclaration();
            }
            if (wordName == "CONSTANT") {
                return parseConstantDeclaration();
            }
            
            // Regular word call
            analyzeWordUsage(wordName);
            auto wordCall = std::make_unique<WordCallNode>(wordName, token.line, token.column);
            advance();
            return wordCall;
        }
        
        default:
            addError("Unexpected token", token);
            advance();
            return nullptr;
    }
}

auto ForthParser::parseWordDefinition() -> std::unique_ptr<WordDefinitionNode> {
    consume(TokenType::COLON_DEF, "Expected ':' at start of word definition");
    
    if (currentToken().type != TokenType::WORD) {
        addError("Expected word name after ':'", currentToken());
        return nullptr;
    }
    
    const std::string wordName = ForthUtils::toUpper(currentToken().value);
    const int line = currentToken().line;
    const int column = currentToken().column;
    advance();
    
    auto definition = std::make_unique<WordDefinitionNode>(wordName, line, column);
    
    // Parse word body until semicolon
    while (!isAtEnd() && currentToken().type != TokenType::SEMICOLON) {
        auto statement = parseStatement();
        if (statement) {
            definition->addChild(std::move(statement));
        }
    }
    
    consume(TokenType::SEMICOLON, "Expected ';' at end of word definition");
    
    // Add to dictionary
    // Note: We'll create a simplified AST for the dictionary
    auto bodyNode = std::make_unique<ProgramNode>();
    // Copy children from definition (simplified approach)
    dictionary->defineWord(wordName, std::move(bodyNode));
    
    return definition;
}

auto ForthParser::parseIfStatement() -> std::unique_ptr<IfStatementNode> {
    const int line = currentToken().line;
    const int column = currentToken().column;
    
    consume(TokenType::IF, "Expected 'IF'");
    controlStack.push(TokenType::IF);
    
    auto ifNode = std::make_unique<IfStatementNode>(line, column);
    
    // Parse THEN branch
    auto thenBranch = std::make_unique<ProgramNode>();
    while (!isAtEnd() && currentToken().type != TokenType::THEN && 
           currentToken().type != TokenType::ELSE) {
        auto statement = parseStatement();
        if (statement) {
            thenBranch->addChild(std::move(statement));
        }
    }
    ifNode->setThenBranch(std::move(thenBranch));
    
    // Check for ELSE branch
    if (currentToken().type == TokenType::ELSE) {
        advance();
        auto elseBranch = std::make_unique<ProgramNode>();
        while (!isAtEnd() && currentToken().type != TokenType::THEN) {
            auto statement = parseStatement();
            if (statement) {
                elseBranch->addChild(std::move(statement));
            }
        }
        ifNode->setElseBranch(std::move(elseBranch));
    }
    
    consume(TokenType::THEN, "Expected 'THEN' to close IF statement");
    controlStack.pop();
    
    return ifNode;
}

auto ForthParser::parseBeginUntilLoop() -> std::unique_ptr<BeginUntilLoopNode> {
    const int line = currentToken().line;
    const int column = currentToken().column;
    
    consume(TokenType::BEGIN, "Expected 'BEGIN'");
    controlStack.push(TokenType::BEGIN);
    
    auto loopNode = std::make_unique<BeginUntilLoopNode>(line, column);
    
    // Parse loop body
    auto body = std::make_unique<ProgramNode>();
    while (!isAtEnd() && currentToken().type != TokenType::UNTIL) {
        auto statement = parseStatement();
        if (statement) {
            body->addChild(std::move(statement));
        }
    }
    loopNode->setBody(std::move(body));
    
    consume(TokenType::UNTIL, "Expected 'UNTIL' to close BEGIN loop");
    controlStack.pop();
    
    return loopNode;
}

auto ForthParser::parseVariableDeclaration() -> std::unique_ptr<VariableDeclarationNode> {
    consume(TokenType::WORD, "Expected 'VARIABLE'"); // Consume VARIABLE token
    
    if (currentToken().type != TokenType::WORD) {
        addError("Expected variable name after 'VARIABLE'", currentToken());
        return nullptr;
    }
    
    const std::string varName = ForthUtils::toUpper(currentToken().value);
    const int line = currentToken().line;
    const int column = currentToken().column;
    advance();
    
    auto varNode = std::make_unique<VariableDeclarationNode>(varName, false, line, column);
    
    // Add to dictionary
    dictionary->defineVariable(varName);
    
    return varNode;
}

auto ForthParser::parseConstantDeclaration() -> std::unique_ptr<VariableDeclarationNode> {
    consume(TokenType::WORD, "Expected 'CONSTANT'"); // Consume CONSTANT token
    
    if (currentToken().type != TokenType::WORD) {
        addError("Expected constant name after 'CONSTANT'", currentToken());
        return nullptr;
    }
    
    const std::string constName = ForthUtils::toUpper(currentToken().value);
    const int line = currentToken().line;
    const int column = currentToken().column;
    advance();
    
    auto constNode = std::make_unique<VariableDeclarationNode>(constName, true, line, column);
    
    // Constants need an initial value from the stack
    // This will be handled during code generation
    dictionary->defineConstant(constName, nullptr);
    
    return constNode;
}

auto ForthParser::parsePrimaryExpression() -> std::unique_ptr<ASTNode> {
    const auto& token = currentToken();
    
    switch (token.type) {
        case TokenType::NUMBER: {
            auto numberNode = std::make_unique<NumberLiteralNode>(token.value, token.line, token.column);
            advance();
            return numberNode;
        }
        
        case TokenType::STRING: {
            auto stringNode = std::make_unique<StringLiteralNode>(token.value, token.line, token.column);
            advance();
            return stringNode;
        }
        
        default:
            addError("Expected number or string", token);
            return nullptr;
    }
}

// Utility methods implementation
[[nodiscard]] auto ForthParser::currentToken() const -> const Token& {
    if (currentPos >= tokens.size()) {
        static const Token eofToken{TokenType::EOF_TOKEN, "", 0, 0};
        return eofToken;
    }
    return tokens[currentPos];
}

[[nodiscard]] auto ForthParser::peekToken(size_t offset) const -> const Token& {
    if (currentPos + offset >= tokens.size()) {
        static const Token eofToken{TokenType::EOF_TOKEN, "", 0, 0};
        return eofToken;
    }
    return tokens[currentPos + offset];
}

[[nodiscard]] auto ForthParser::isAtEnd() const -> bool {
    return currentPos >= tokens.size() || currentToken().type == TokenType::EOF_TOKEN;
}

auto ForthParser::advance() -> void {
    if (!isAtEnd()) {
        currentPos++;
    }
}

auto ForthParser::expect(TokenType type) -> bool {
    return currentToken().type == type;
}

auto ForthParser::consume(TokenType type, const std::string& errorMsg) -> bool {
    if (expect(type)) {
        advance();
        return true;
    } else {
        addError(errorMsg, currentToken());
        return false;
    }
}

auto ForthParser::addError(const std::string& message) -> void {
    errors.push_back(message);
}

auto ForthParser::addError(const std::string& message, const Token& token) -> void {
    std::ostringstream oss;
    oss << message << " at line " << token.line << ", column " << token.column;
    if (!token.value.empty()) {
        oss << " (token: '" << token.value << "')";
    }
    errors.push_back(oss.str());
}

[[nodiscard]] auto ForthParser::validateControlFlow() const -> bool {
    return controlStack.empty();
}

auto ForthParser::analyzeWordUsage(const std::string& wordName) -> void {
    if (!dictionary->isWordDefined(wordName)) {
        addError("Undefined word: " + wordName);
    }
}

[[nodiscard]] auto ForthParser::getStatistics() const -> ParseStatistics {
    // This would be implemented by traversing the AST
    // For now, return a basic implementation
    return {tokens.size(), 0, 0, 0, 0, 0, false, false};
}
