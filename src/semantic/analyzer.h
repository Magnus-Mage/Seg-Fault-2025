#ifndef FORTH_SEMANTIC_ANALYZER_H
#define FORTH_SEMANTIC_ANALYZER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <stack>
#include "parser/ast.h"
#include "dictionary/dictionary.h"
#include "common/types.h"

// Stack state during semantic analysis
struct StackState {
    int depth;          // Current stack depth
    int minDepth;       // Minimum depth reached
    int maxDepth;       // Maximum depth reached
    bool isValid;       // Whether stack state is valid (no underflow)
    
    StackState() : depth(0), minDepth(0), maxDepth(0), isValid(true) {}
    
    auto push(int count = 1) -> void {
        depth += count;
        maxDepth = std::max(maxDepth, depth);
    }
    
    auto pop(int count = 1) -> bool {
        depth -= count;
        minDepth = std::min(minDepth, depth);
        if (depth < 0) {
            isValid = false;
            return false;
        }
        return true;
    }
    
    auto reset() -> void {
        depth = minDepth = maxDepth = 0;
        isValid = true;
    }
};

// Type information for FORTH values
enum class ForthValueType {
    UNKNOWN,
    INTEGER,
    FLOAT,
    BOOLEAN,
    ADDRESS,
    STRING_ADDR,
    STRING_LENGTH,
    CELL         // Generic cell value
};

struct TypedStackEffect {
    ASTNode::StackEffect effect;
    std::vector<ForthValueType> consumedTypes;
    std::vector<ForthValueType> producedTypes;
    
    TypedStackEffect() = default;
    TypedStackEffect(const ASTNode::StackEffect& e) : effect(e) {}
};

// Semantic analyzer for FORTH programs
class SemanticAnalyzer : public ASTVisitor {
private:
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    // Stack analysis state
    StackState currentStack;
    std::stack<StackState> stackStateStack; // For nested contexts
    
    // Word analysis results
    std::unordered_map<std::string, TypedStackEffect> wordEffects;
    std::unordered_map<std::string, bool> analyzedWords;
    
    // Type tracking
    std::unordered_map<std::string, ForthValueType> variableTypes;
    std::unordered_map<std::string, ForthValueType> constantTypes;
    
    // Analysis context
    const ForthDictionary* dictionary;
    std::string currentWordName;
    bool inWordDefinition;
    int analysisDepth; // For recursion protection
    
public:
    SemanticAnalyzer();
    explicit SemanticAnalyzer(const ForthDictionary* dict);
    
    // Main analysis entry point
    auto analyze(ProgramNode& program) -> bool;
    
    // Stack effect queries
    [[nodiscard]] auto getStackEffect(const std::string& wordName) const -> ASTNode::StackEffect;
    [[nodiscard]] auto getTypedStackEffect(const std::string& wordName) const -> TypedStackEffect;
    
    // Error handling
    [[nodiscard]] auto hasErrors() const -> bool { return !errors.empty(); }
    [[nodiscard]] auto hasWarnings() const -> bool { return !warnings.empty(); }
    [[nodiscard]] auto getErrors() const -> const std::vector<std::string>& { return errors; }
    [[nodiscard]] auto getWarnings() const -> const std::vector<std::string>& { return warnings; }
    auto clearErrors() -> void { errors.clear(); warnings.clear(); }
    
    // Analysis results
    [[nodiscard]] auto getWordEffects() const -> const std::unordered_map<std::string, TypedStackEffect>& {
        return wordEffects;
    }
    
    [[nodiscard]] auto getMaxStackDepth() const -> int { return currentStack.maxDepth; }
    [[nodiscard]] auto getMinStackDepth() const -> int { return currentStack.minDepth; }
    
    // Visitor pattern implementation
    void visit(ProgramNode& node) override;
    void visit(WordDefinitionNode& node) override;
    void visit(WordCallNode& node) override;
    void visit(NumberLiteralNode& node) override;
    void visit(StringLiteralNode& node) override;
    void visit(IfStatementNode& node) override;
    void visit(BeginUntilLoopNode& node) override;
    void visit(MathOperationNode& node) override;
    void visit(VariableDeclarationNode& node) override;
    
private:
    // Stack effect calculation
    auto calculateWordEffect(const std::string& wordName) -> TypedStackEffect;
    auto analyzeWordDefinition(WordDefinitionNode& node) -> TypedStackEffect;
    auto analyzeControlFlow(ASTNode& node) -> TypedStackEffect;
    
    // Stack management
    auto pushStack(int count = 1, ForthValueType type = ForthValueType::CELL) -> void;
    auto popStack(int count = 1) -> bool;
    auto saveStackState() -> void;
    auto restoreStackState() -> void;
    auto mergeStackStates(const StackState& state1, const StackState& state2) -> StackState;
    
    // Type analysis
    auto inferType(ASTNode& node) -> ForthValueType;
    auto checkTypeCompatibility(ForthValueType expected, ForthValueType actual) -> bool;
    
    // Built-in word effects
    auto getBuiltinStackEffect(const std::string& wordName) -> TypedStackEffect;
    auto initializeBuiltinEffects() -> void;
    
    // Error reporting
    auto addError(const std::string& message, ASTNode& node) -> void;
    auto addError(const std::string& message) -> void;
    auto addWarning(const std::string& message, ASTNode& node) -> void;
    auto addWarning(const std::string& message) -> void;
    
    // Utility methods
    auto formatStackEffect(const ASTNode::StackEffect& effect) -> std::string;
    auto validateStackBalance(ASTNode& node) -> bool;
    [[nodiscard]] auto isRecursiveCall(const std::string& wordName) const -> bool;
    
    // Forward declaration resolution
    auto resolveForwardReferences() -> void;
    auto markWordAsAnalyzed(const std::string& wordName, const TypedStackEffect& effect) -> void;
};

// Stack effect calculation utilities
namespace StackEffectUtils {
    [[nodiscard]] auto combine(const ASTNode::StackEffect& a, const ASTNode::StackEffect& b) -> ASTNode::StackEffect;
    [[nodiscard]] auto sequence(const std::vector<ASTNode::StackEffect>& effects) -> ASTNode::StackEffect;
    [[nodiscard]] auto conditional(const ASTNode::StackEffect& condition, 
                                  const ASTNode::StackEffect& thenBranch,
                                  const ASTNode::StackEffect& elseBranch) -> ASTNode::StackEffect;
    [[nodiscard]] auto loop(const ASTNode::StackEffect& body, 
                           const ASTNode::StackEffect& condition) -> ASTNode::StackEffect;
}

// Semantic analysis configuration
struct AnalysisConfig {
    bool strictTypeChecking;
    bool warnOnUnknownWords;
    bool allowRecursion;
    int maxRecursionDepth;
    bool optimizeStackEffects;
    
    AnalysisConfig() 
        : strictTypeChecking(false)
        , warnOnUnknownWords(true)
        , allowRecursion(true)
        , maxRecursionDepth(100)
        , optimizeStackEffects(true) {}
};

#endif // FORTH_SEMANTIC_ANALYZER_H
