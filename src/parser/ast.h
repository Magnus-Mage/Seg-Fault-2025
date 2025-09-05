#ifndef FORTH_AST_H
#define FORTH_AST_H

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "common/types.h"

// Forward declarations
class ASTVisitor;
class CodeGenerator;

// Base AST Node class
class ASTNode {
public:
    enum class NodeType {
        PROGRAM,
        WORD_DEFINITION,
        WORD_CALL,
        NUMBER_LITERAL,
        STRING_LITERAL,
        IF_STATEMENT,
        WHILE_LOOP,
        BEGIN_UNTIL_LOOP,
        DO_LOOP,
        VARIABLE_DECLARATION,
        CONSTANT_DECLARATION,
        MATH_OPERATION,
        STACK_OPERATION,
        MEMORY_ACCESS,
        COMMENT
    };

protected:
    NodeType nodeType;
    int line;
    int column;
    std::vector<std::unique_ptr<ASTNode>> children;

public:
    ASTNode(NodeType type, int line = 0, int column = 0) 
        : nodeType(type), line(line), column(column) {}
    
    virtual ~ASTNode() = default;
    
    // Core interface
    [[nodiscard]] auto getType() const -> NodeType { return nodeType; }
    [[nodiscard]] auto getLine() const -> int { return line; }
    [[nodiscard]] auto getColumn() const -> int { return column; }
    
    // Child management
    auto addChild(std::unique_ptr<ASTNode> child) -> void {
        children.push_back(std::move(child));
    }
    
    [[nodiscard]] auto getChildren() const -> const std::vector<std::unique_ptr<ASTNode>>& {
        return children;
    }
    
    [[nodiscard]] auto getChild(size_t index) const -> ASTNode* {
        return (index < children.size()) ? children[index].get() : nullptr;
    }
    
    [[nodiscard]] auto getChildCount() const -> size_t {
        return children.size();
    }
    
    // Visitor pattern for code generation
    virtual auto accept(ASTVisitor& visitor) -> void = 0;
    
    // Utility methods
    virtual auto toString() const -> std::string = 0;
    
    // Stack effect analysis (for optimization and validation)
    struct StackEffect {
        int consumed;  // How many items this node consumes from stack
        int produced;  // How many items this node produces on stack
        bool isKnown;  // Whether the effect is statically determinable
    };
    
    virtual auto getStackEffect() const -> StackEffect {
        return {0, 0, true}; // Default: no effect
    }
};

// Program node - root of the AST
class ProgramNode : public ASTNode {
public:
    ProgramNode() : ASTNode(NodeType::PROGRAM) {}
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto toString() const -> std::string override {
        return "Program[" + std::to_string(children.size()) + " statements]";
    }
};

// Word definition node: : WORD_NAME ... ;
class WordDefinitionNode : public ASTNode {
private:
    std::string wordName;
    
public:
    WordDefinitionNode(const std::string& name, int line, int column)
        : ASTNode(NodeType::WORD_DEFINITION, line, column), wordName(name) {}
    
    [[nodiscard]] auto getWordName() const -> const std::string& { return wordName; }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto toString() const -> std::string override {
        return "WordDef[" + wordName + "]";
    }
    
    auto getStackEffect() const -> StackEffect override {
        // Word definitions don't directly affect stack during compilation
        return {0, 0, true};
    }
};

// Word call node - calling a defined word
class WordCallNode : public ASTNode {
private:
    std::string wordName;
    
public:
    WordCallNode(const std::string& name, int line, int column)
        : ASTNode(NodeType::WORD_CALL, line, column), wordName(name) {}
    
    [[nodiscard]] auto getWordName() const -> const std::string& { return wordName; }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto toString() const -> std::string override {
        return "WordCall[" + wordName + "]";
    }
    
    auto getStackEffect() const -> StackEffect override {
        // Stack effect depends on the word being called
        // Will be resolved during semantic analysis
        return {0, 0, false};
    }
};

// Number literal node
class NumberLiteralNode : public ASTNode {
private:
    std::string value;
    bool isFloat;
    
public:
    NumberLiteralNode(const std::string& val, int line, int column)
        : ASTNode(NodeType::NUMBER_LITERAL, line, column), value(val) {
        isFloat = val.find('.') != std::string::npos;
    }
    
    [[nodiscard]] auto getValue() const -> const std::string& { return value; }
    [[nodiscard]] auto isFloatingPoint() const -> bool { return isFloat; }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto toString() const -> std::string override {
        return "Number[" + value + "]";
    }
    
    auto getStackEffect() const -> StackEffect override {
        return {0, 1, true}; // Numbers push one item onto stack
    }
};

// String literal node
class StringLiteralNode : public ASTNode {
private:
    std::string value;
    bool isPrintString; // true for ." strings, false for regular strings
    
public:
    StringLiteralNode(const std::string& val, int line, int column)
        : ASTNode(NodeType::STRING_LITERAL, line, column), value(val) {
        isPrintString = val.length() > 0 && val[0] == '.';
        if (isPrintString) {
            value = val.substr(1); // Remove the '.' prefix
        }
    }
    
    [[nodiscard]] auto getValue() const -> const std::string& { return value; }
    [[nodiscard]] auto isPrint() const -> bool { return isPrintString; }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto toString() const -> std::string override {
        std::string prefix = isPrintString ? "PRINT:" : "";
        return "String[" + prefix + value + "]";
    }
    
    auto getStackEffect() const -> StackEffect override {
        if (isPrintString) {
            return {0, 0, true}; // Print strings don't affect stack
        } else {
            return {0, 2, true}; // Regular strings push addr+len
        }
    }
};

// Control flow nodes
class IfStatementNode : public ASTNode {
private:
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> thenBranch;
    std::unique_ptr<ASTNode> elseBranch; // nullable
    
public:
    IfStatementNode(int line, int column)
        : ASTNode(NodeType::IF_STATEMENT, line, column) {}
    
    auto setCondition(std::unique_ptr<ASTNode> cond) -> void {
        condition = std::move(cond);
    }
    
    auto setThenBranch(std::unique_ptr<ASTNode> then) -> void {
        thenBranch = std::move(then);
    }
    
    auto setElseBranch(std::unique_ptr<ASTNode> elseNode) -> void {
        elseBranch = std::move(elseNode);
    }
    
    [[nodiscard]] auto getCondition() const -> ASTNode* { return condition.get(); }
    [[nodiscard]] auto getThenBranch() const -> ASTNode* { return thenBranch.get(); }
    [[nodiscard]] auto getElseBranch() const -> ASTNode* { return elseBranch.get(); }
    [[nodiscard]] auto hasElse() const -> bool { return elseBranch != nullptr; }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto toString() const -> std::string override {
        std::string result = "If[";
        result += hasElse() ? "with-else" : "no-else";
        result += "]";
        return result;
    }
    
    auto getStackEffect() const -> StackEffect override {
        return {1, 0, false}; // IF consumes condition from stack
    }
};

// BEGIN...UNTIL loop
class BeginUntilLoopNode : public ASTNode {
private:
    std::unique_ptr<ASTNode> body;
    std::unique_ptr<ASTNode> condition;
    
public:
    BeginUntilLoopNode(int line, int column)
        : ASTNode(NodeType::BEGIN_UNTIL_LOOP, line, column) {}
    
    auto setBody(std::unique_ptr<ASTNode> bodyNode) -> void {
        body = std::move(bodyNode);
    }
    
    auto setCondition(std::unique_ptr<ASTNode> cond) -> void {
        condition = std::move(cond);
    }
    
    [[nodiscard]] auto getBody() const -> ASTNode* { return body.get(); }
    [[nodiscard]] auto getCondition() const -> ASTNode* { return condition.get(); }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto toString() const -> std::string override {
        return "BeginUntil[]";
    }
    
    auto getStackEffect() const -> StackEffect override {
        return {0, 0, false}; // Loop effect depends on body and iterations
    }
};

// Math operation node
class MathOperationNode : public ASTNode {
private:
    std::string operation;
    
public:
    MathOperationNode(const std::string& op, int line, int column)
        : ASTNode(NodeType::MATH_OPERATION, line, column), operation(op) {}
    
    [[nodiscard]] auto getOperation() const -> const std::string& { return operation; }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto toString() const -> std::string override {
        return "Math[" + operation + "]";
    }
    
    auto getStackEffect() const -> StackEffect override {
        // Basic stack effects for common math operations
        if (operation == "+" || operation == "-" || operation == "*" || 
            operation == "/" || operation == "MOD") {
            return {2, 1, true}; // Binary operations: consume 2, produce 1
        } else if (operation == "NEGATE" || operation == "ABS" || 
                  operation == "SQRT" || operation == "SIN" || operation == "COS") {
            return {1, 1, true}; // Unary operations: consume 1, produce 1
        } else if (operation == "DUP") {
            return {1, 2, true}; // DUP: consume 1, produce 2
        } else if (operation == "DROP") {
            return {1, 0, true}; // DROP: consume 1, produce 0
        } else if (operation == "SWAP") {
            return {2, 2, true}; // SWAP: consume 2, produce 2
        }
        return {0, 0, false}; // Unknown operation
    }
};

// Variable/Constant declarations
class VariableDeclarationNode : public ASTNode {
private:
    std::string varName;
    std::unique_ptr<ASTNode> initialValue; // For CONSTANT, nullptr for VARIABLE
    bool isConstant;
    
public:
    VariableDeclarationNode(const std::string& name, bool constant, int line, int column)
        : ASTNode(isConstant ? NodeType::CONSTANT_DECLARATION : NodeType::VARIABLE_DECLARATION, 
                 line, column), 
          varName(name), isConstant(constant) {}
    
    [[nodiscard]] auto getVarName() const -> const std::string& { return varName; }
    [[nodiscard]] auto isConst() const -> bool { return isConstant; }
    [[nodiscard]] auto getInitialValue() const -> ASTNode* { return initialValue.get(); }
    
    auto setInitialValue(std::unique_ptr<ASTNode> value) -> void {
        initialValue = std::move(value);
    }
    
    auto accept(ASTVisitor& visitor) -> void override;
    auto toString() const -> std::string override {
        return (isConstant ? "Constant[" : "Variable[") + varName + "]";
    }
    
    auto getStackEffect() const -> StackEffect override {
        if (isConstant) {
            return {1, 0, true}; // CONSTANT consumes initial value from stack
        } else {
            return {0, 0, true}; // VARIABLE doesn't affect stack during declaration
        }
    }
};

// Visitor pattern interface
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    virtual auto visit(ProgramNode& node) -> void = 0;
    virtual auto visit(WordDefinitionNode& node) -> void = 0;
    virtual auto visit(WordCallNode& node) -> void = 0;
    virtual auto visit(NumberLiteralNode& node) -> void = 0;
    virtual auto visit(StringLiteralNode& node) -> void = 0;
    virtual auto visit(IfStatementNode& node) -> void = 0;
    virtual auto visit(BeginUntilLoopNode& node) -> void = 0;
    virtual auto visit(MathOperationNode& node) -> void = 0;
    virtual auto visit(VariableDeclarationNode& node) -> void = 0;
};

// Utility functions
[[nodiscard]] auto nodeTypeToString(ASTNode::NodeType type) -> std::string;
[[nodiscard]] auto createASTNode(ASTNode::NodeType type, const std::string& value = "", 
                                int line = 0, int column = 0) -> std::unique_ptr<ASTNode>;

#endif // FORTH_AST_H
