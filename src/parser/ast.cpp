#include "parser/ast.h"
#include <stdexcept>

// ProgramNode visitor implementation
auto ProgramNode::accept(ASTVisitor& visitor) -> void {
    visitor.visit(*this);
}

// WordDefinitionNode visitor implementation
auto WordDefinitionNode::accept(ASTVisitor& visitor) -> void {
    visitor.visit(*this);
}

// WordCallNode visitor implementation
auto WordCallNode::accept(ASTVisitor& visitor) -> void {
    visitor.visit(*this);
}

// NumberLiteralNode visitor implementation
auto NumberLiteralNode::accept(ASTVisitor& visitor) -> void {
    visitor.visit(*this);
}

// StringLiteralNode visitor implementation
auto StringLiteralNode::accept(ASTVisitor& visitor) -> void {
    visitor.visit(*this);
}

// IfStatementNode visitor implementation
auto IfStatementNode::accept(ASTVisitor& visitor) -> void {
    visitor.visit(*this);
}

// BeginUntilLoopNode visitor implementation
auto BeginUntilLoopNode::accept(ASTVisitor& visitor) -> void {
    visitor.visit(*this);
}

// MathOperationNode visitor implementation
auto MathOperationNode::accept(ASTVisitor& visitor) -> void {
    visitor.visit(*this);
}

// VariableDeclarationNode visitor implementation
auto VariableDeclarationNode::accept(ASTVisitor& visitor) -> void {
    visitor.visit(*this);
}

// Utility function implementations
[[nodiscard]] auto nodeTypeToString(ASTNode::NodeType type) -> std::string {
    switch (type) {
        case ASTNode::NodeType::PROGRAM:               return "PROGRAM";
        case ASTNode::NodeType::WORD_DEFINITION:       return "WORD_DEFINITION";
        case ASTNode::NodeType::WORD_CALL:             return "WORD_CALL";
        case ASTNode::NodeType::NUMBER_LITERAL:        return "NUMBER_LITERAL";
        case ASTNode::NodeType::STRING_LITERAL:        return "STRING_LITERAL";
        case ASTNode::NodeType::IF_STATEMENT:          return "IF_STATEMENT";
        case ASTNode::NodeType::WHILE_LOOP:            return "WHILE_LOOP";
        case ASTNode::NodeType::BEGIN_UNTIL_LOOP:      return "BEGIN_UNTIL_LOOP";
        case ASTNode::NodeType::DO_LOOP:               return "DO_LOOP";
        case ASTNode::NodeType::VARIABLE_DECLARATION:  return "VARIABLE_DECLARATION";
        case ASTNode::NodeType::CONSTANT_DECLARATION:  return "CONSTANT_DECLARATION";
        case ASTNode::NodeType::MATH_OPERATION:        return "MATH_OPERATION";
        case ASTNode::NodeType::STACK_OPERATION:       return "STACK_OPERATION";
        case ASTNode::NodeType::MEMORY_ACCESS:         return "MEMORY_ACCESS";
        case ASTNode::NodeType::COMMENT:               return "COMMENT";
        default:                                       return "UNKNOWN";
    }
}

[[nodiscard]] auto createASTNode(ASTNode::NodeType type, const std::string& value, 
                                int line, int column) -> std::unique_ptr<ASTNode> {
    switch (type) {
        case ASTNode::NodeType::PROGRAM:
            return std::make_unique<ProgramNode>();
            
        case ASTNode::NodeType::WORD_DEFINITION:
            return std::make_unique<WordDefinitionNode>(value, line, column);
            
        case ASTNode::NodeType::WORD_CALL:
            return std::make_unique<WordCallNode>(value, line, column);
            
        case ASTNode::NodeType::NUMBER_LITERAL:
            return std::make_unique<NumberLiteralNode>(value, line, column);
            
        case ASTNode::NodeType::STRING_LITERAL:
            return std::make_unique<StringLiteralNode>(value, line, column);
            
        case ASTNode::NodeType::IF_STATEMENT:
            return std::make_unique<IfStatementNode>(line, column);
            
        case ASTNode::NodeType::BEGIN_UNTIL_LOOP:
            return std::make_unique<BeginUntilLoopNode>(line, column);
            
        case ASTNode::NodeType::MATH_OPERATION:
            return std::make_unique<MathOperationNode>(value, line, column);
            
        case ASTNode::NodeType::VARIABLE_DECLARATION:
            return std::make_unique<VariableDeclarationNode>(value, false, line, column);
            
        case ASTNode::NodeType::CONSTANT_DECLARATION:
            return std::make_unique<VariableDeclarationNode>(value, true, line, column);
            
        default:
            throw std::runtime_error("Cannot create AST node of type: " + nodeTypeToString(type));
    }
}
