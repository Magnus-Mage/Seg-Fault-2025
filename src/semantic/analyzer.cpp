#include "semantic/analyzer.h"
#include "dictionary/dictionary.h"
#include "common/utils.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

SemanticAnalyzer::SemanticAnalyzer() 
    : dictionary(nullptr), inWordDefinition(false), analysisDepth(0) {
    initializeBuiltinEffects();
}

SemanticAnalyzer::SemanticAnalyzer(const ForthDictionary* dict) 
    : dictionary(dict), inWordDefinition(false), analysisDepth(0) {
    initializeBuiltinEffects();
}

auto SemanticAnalyzer::analyze(ProgramNode& program) -> bool {
    errors.clear();
    warnings.clear();
    currentStack.reset();
    analyzedWords.clear();
    wordEffects.clear();
    
    // First pass: collect all word definitions
    for (const auto& child : program.getChildren()) {
        if (auto wordDef = dynamic_cast<WordDefinitionNode*>(child.get())) {
            const auto& wordName = wordDef->getWordName();
            analyzedWords[wordName] = false; // Mark as needing analysis
        }
    }
    
    // Second pass: analyze the program
    visit(program);
    
    // Third pass: resolve forward references if needed
    resolveForwardReferences();
    
    return !hasErrors();
}

void SemanticAnalyzer::visit(ProgramNode& node) {
    for (const auto& child : node.getChildren()) {
        child->accept(*this);
    }
}

void SemanticAnalyzer::visit(WordDefinitionNode& node) {
    const auto& wordName = node.getWordName();
    
    if (analyzedWords.find(wordName) != analyzedWords.end() && analyzedWords[wordName]) {
        return; // Already analyzed
    }
    
    currentWordName = wordName;
    inWordDefinition = true;
    
    saveStackState();
    currentStack.reset();
    
    auto effect = analyzeWordDefinition(node);
    markWordAsAnalyzed(wordName, effect);
    
    restoreStackState();
    inWordDefinition = false;
    currentWordName.clear();
}

auto SemanticAnalyzer::analyzeWordDefinition(WordDefinitionNode& node) -> TypedStackEffect {
    StackState initialState;
    
    for (const auto& child : node.getChildren()) {
        child->accept(*this);
    }
    
    // Calculate net effect
    TypedStackEffect effect;
    effect.effect.consumed = std::max(0, -currentStack.minDepth);
    effect.effect.produced = currentStack.depth + effect.effect.consumed;
    effect.effect.isKnown = currentStack.isValid;
    
    if (!currentStack.isValid) {
        addError("Stack underflow in word definition: " + node.getWordName(), node);
    }
    
    return effect;
}

void SemanticAnalyzer::visit(WordCallNode& node) {
    const auto& wordName = node.getWordName();
    
    // Check if it's the word we're currently defining (recursion)
    if (wordName == currentWordName) {
        if (analysisDepth > 10) {
            addWarning("Deep recursion detected in word: " + wordName, node);
            // Use a conservative estimate for recursive calls
            auto effect = ASTNode::StackEffect{1, 1, false};
            if (!popStack(effect.consumed)) {
                addError("Stack underflow calling recursive word: " + wordName, node);
            }
            pushStack(effect.produced);
            return;
        }
        analysisDepth++;
    }
    
    auto effect = calculateWordEffect(wordName);
    
    if (!effect.effect.isKnown) {
        addWarning("Unknown stack effect for word: " + wordName, node);
    }
    
    // Apply stack effect
    if (!popStack(effect.effect.consumed)) {
        addError("Stack underflow calling word: " + wordName, node);
    }
    pushStack(effect.effect.produced);
    
    if (wordName == currentWordName) {
        analysisDepth--;
    }
}

void SemanticAnalyzer::visit(NumberLiteralNode& node) {
    pushStack(1, node.isFloatingPoint() ? ForthValueType::FLOAT : ForthValueType::INTEGER);
}

void SemanticAnalyzer::visit(StringLiteralNode& node) {
    if (node.isPrint()) {
        // Print strings don't affect stack
        return;
    } else {
        // Regular strings push address and length
        pushStack(1, ForthValueType::STRING_ADDR);
        pushStack(1, ForthValueType::STRING_LENGTH);
    }
}

void SemanticAnalyzer::visit(IfStatementNode& node) {
    // IF statement consumes condition from stack
    if (!popStack(1)) {
        addError("Stack underflow in IF condition", node);
    }
    
    // Analyze branches
    StackState beforeBranches = currentStack;
    
    // Analyze THEN branch
    saveStackState();
    if (node.getThenBranch()) {
        for (const auto& child : node.getThenBranch()->getChildren()) {
            child->accept(*this);
        }
    }
    StackState afterThen = currentStack;
    restoreStackState();
    
    // Analyze ELSE branch if it exists
    StackState afterElse = beforeBranches;
    if (node.getElseBranch()) {
        saveStackState();
        for (const auto& child : node.getElseBranch()->getChildren()) {
            child->accept(*this);
        }
        afterElse = currentStack;
        restoreStackState();
    }
    
    // Merge stack states from both branches
    currentStack = mergeStackStates(afterThen, afterElse);
    
    if (!currentStack.isValid) {
        addError("Inconsistent stack effects in IF-THEN-ELSE branches", node);
    }
}

void SemanticAnalyzer::visit(BeginUntilLoopNode& node) {
    saveStackState();
    StackState loopEntry = currentStack;
    
    // Analyze loop body
    if (node.getBody()) {
        for (const auto& child : node.getBody()->getChildren()) {
            child->accept(*this);
        }
    }
    
    // UNTIL consumes condition from stack
    if (!popStack(1)) {
        addError("Stack underflow in UNTIL condition", node);
    }
    
    // Check that loop maintains stack balance
    int netEffect = currentStack.depth - loopEntry.depth;
    if (netEffect != 0) {
        addWarning("Loop may have unbalanced stack effect: " + std::to_string(netEffect), node);
    }
    
    restoreStackState();
}

void SemanticAnalyzer::visit(MathOperationNode& node) {
    auto effect = node.getStackEffect();
    
    if (!popStack(effect.consumed)) {
        addError("Stack underflow in math operation: " + node.getOperation(), node);
    }
    pushStack(effect.produced);
    
    // Type checking for math operations
    const auto& op = node.getOperation();
    if (op == "+" || op == "-" || op == "*" || op == "/") {
        // Binary arithmetic operations
        // In a full implementation, we'd check operand types here
    } else if (op == "SQRT" || op == "SIN" || op == "COS") {
        // Unary math operations - might require float
    }
}

void SemanticAnalyzer::visit(VariableDeclarationNode& node) {
    const auto& varName = node.getVarName();
    
    if (node.isConst()) {
        // Constants consume initial value from stack
        if (!popStack(1)) {
            addError("Stack underflow in constant declaration: " + varName, node);
        }
        constantTypes[varName] = ForthValueType::CELL; // Will be refined later
    } else {
        // Variables don't affect stack during declaration
        variableTypes[varName] = ForthValueType::ADDRESS;
    }
}

// Private implementation methods
auto SemanticAnalyzer::calculateWordEffect(const std::string& wordName) -> TypedStackEffect {
    // Check if already analyzed
    auto it = wordEffects.find(wordName);
    if (it != wordEffects.end()) {
        return it->second;
    }
    
    // Check if it's a built-in word
    auto builtinEffect = getBuiltinStackEffect(wordName);
    if (builtinEffect.effect.isKnown) {
        return builtinEffect;
    }
    
    // Check dictionary for user-defined words
    if (dictionary && dictionary->isWordDefined(wordName)) {
        auto dictEffect = dictionary->getStackEffect(wordName);
        TypedStackEffect effect(dictEffect);
        wordEffects[wordName] = effect;
        return effect;
    }
    
    // Unknown word - don't add warning here to avoid spam
    // The parser will catch undefined words
    return TypedStackEffect(ASTNode::StackEffect{0, 0, false});
}

auto SemanticAnalyzer::getBuiltinStackEffect(const std::string& wordName) -> TypedStackEffect {
    // Mathematical operations
    if (wordName == "+" || wordName == "-" || wordName == "*" || wordName == "/" || wordName == "MOD") {
        return TypedStackEffect(ASTNode::StackEffect{2, 1, true});
    }
    if (wordName == "NEGATE" || wordName == "ABS" || wordName == "1+" || wordName == "1-") {
        return TypedStackEffect(ASTNode::StackEffect{1, 1, true});
    }
    if (wordName == "SQRT" || wordName == "SIN" || wordName == "COS" || wordName == "TAN") {
        return TypedStackEffect(ASTNode::StackEffect{1, 1, true});
    }
    
    // Stack operations
    if (wordName == "DUP") {
        return TypedStackEffect(ASTNode::StackEffect{1, 2, true});
    }
    if (wordName == "DROP") {
        return TypedStackEffect(ASTNode::StackEffect{1, 0, true});
    }
    if (wordName == "SWAP") {
        return TypedStackEffect(ASTNode::StackEffect{2, 2, true});
    }
    if (wordName == "OVER") {
        return TypedStackEffect(ASTNode::StackEffect{2, 3, true});
    }
    if (wordName == "ROT") {
        return TypedStackEffect(ASTNode::StackEffect{3, 3, true});
    }
    
    // Comparison operations
    if (wordName == "<" || wordName == ">" || wordName == "=" || wordName == "<>" || 
        wordName == "<=" || wordName == ">=") {
        return TypedStackEffect(ASTNode::StackEffect{2, 1, true});
    }
    if (wordName == "0<" || wordName == "0=" || wordName == "0>") {
        return TypedStackEffect(ASTNode::StackEffect{1, 1, true});
    }
    
    // I/O operations
    if (wordName == ".") {
        return TypedStackEffect(ASTNode::StackEffect{1, 0, true});
    }
    if (wordName == "EMIT") {
        return TypedStackEffect(ASTNode::StackEffect{1, 0, true});
    }
    if (wordName == "CR" || wordName == "SPACE") {
        return TypedStackEffect(ASTNode::StackEffect{0, 0, true});
    }
    
    // Memory operations
    if (wordName == "@") {
        return TypedStackEffect(ASTNode::StackEffect{1, 1, true});
    }
    if (wordName == "!") {
        return TypedStackEffect(ASTNode::StackEffect{2, 0, true});
    }
    
    // Unknown built-in
    return TypedStackEffect(ASTNode::StackEffect{0, 0, false});
}

auto SemanticAnalyzer::initializeBuiltinEffects() -> void {
    // This method would populate wordEffects with all known built-in words
    // For now, we handle this in getBuiltinStackEffect
}

auto SemanticAnalyzer::pushStack(int count, ForthValueType type) -> void {
    currentStack.push(count);
}

auto SemanticAnalyzer::popStack(int count) -> bool {
    return currentStack.pop(count);
}

auto SemanticAnalyzer::saveStackState() -> void {
    stackStateStack.push(currentStack);
}

auto SemanticAnalyzer::restoreStackState() -> void {
    if (!stackStateStack.empty()) {
        currentStack = stackStateStack.top();
        stackStateStack.pop();
    }
}

auto SemanticAnalyzer::mergeStackStates(const StackState& state1, const StackState& state2) -> StackState {
    StackState merged;
    
    // Both states must be valid for merge to be valid
    merged.isValid = state1.isValid && state2.isValid;
    
    // States must have same final depth for consistent merge
    if (state1.depth == state2.depth) {
        merged.depth = state1.depth;
        merged.minDepth = std::min(state1.minDepth, state2.minDepth);
        merged.maxDepth = std::max(state1.maxDepth, state2.maxDepth);
    } else {
        merged.isValid = false;
        merged.depth = std::max(state1.depth, state2.depth);
        merged.minDepth = std::min(state1.minDepth, state2.minDepth);
        merged.maxDepth = std::max(state1.maxDepth, state2.maxDepth);
    }
    
    return merged;
}

auto SemanticAnalyzer::addError(const std::string& message, ASTNode& node) -> void {
    std::ostringstream oss;
    oss << message << " at line " << node.getLine() << ", column " << node.getColumn();
    errors.push_back(oss.str());
}

auto SemanticAnalyzer::addError(const std::string& message) -> void {
    errors.push_back(message);
}

auto SemanticAnalyzer::addWarning(const std::string& message, ASTNode& node) -> void {
    std::ostringstream oss;
    oss << message << " at line " << node.getLine() << ", column " << node.getColumn();
    warnings.push_back(oss.str());
}

auto SemanticAnalyzer::addWarning(const std::string& message) -> void {
    warnings.push_back(message);
}

auto SemanticAnalyzer::getStackEffect(const std::string& wordName) const -> ASTNode::StackEffect {
    auto it = wordEffects.find(wordName);
    if (it != wordEffects.end()) {
        return it->second.effect;
    }
    return ASTNode::StackEffect{0, 0, false};
}

auto SemanticAnalyzer::getTypedStackEffect(const std::string& wordName) const -> TypedStackEffect {
    auto it = wordEffects.find(wordName);
    if (it != wordEffects.end()) {
        return it->second;
    }
    return TypedStackEffect(ASTNode::StackEffect{0, 0, false});
}

auto SemanticAnalyzer::resolveForwardReferences() -> void {
    // Iteratively resolve any forward references until no more changes
    bool changed = true;
    int iterations = 0;
    const int maxIterations = 10;
    
    while (changed && iterations < maxIterations) {
        changed = false;
        iterations++;
        
        for (auto& [wordName, analyzed] : analyzedWords) {
            if (!analyzed) {
                // Try to analyze this word again
                // This would require access to the AST nodes, which we don't store
                // In a full implementation, we'd keep references to WordDefinitionNode objects
            }
        }
    }
    
    if (iterations >= maxIterations) {
        addWarning("Maximum iterations reached in forward reference resolution");
    }
}

auto SemanticAnalyzer::markWordAsAnalyzed(const std::string& wordName, const TypedStackEffect& effect) -> void {
    analyzedWords[wordName] = true;
    wordEffects[wordName] = effect;
}

auto SemanticAnalyzer::formatStackEffect(const ASTNode::StackEffect& effect) -> std::string {
    std::ostringstream oss;
    oss << "(" << effect.consumed << " -> " << effect.produced << ")";
    if (!effect.isKnown) {
        oss << " [unknown]";
    }
    return oss.str();
}

// Stack effect utilities implementation
namespace StackEffectUtils {
    
auto combine(const ASTNode::StackEffect& a, const ASTNode::StackEffect& b) -> ASTNode::StackEffect {
    ASTNode::StackEffect result;
    result.isKnown = a.isKnown && b.isKnown;
    
    if (result.isKnown) {
        // b consumes from what a produces
        if (a.produced >= b.consumed) {
            result.consumed = a.consumed + std::max(0, b.consumed - a.produced);
            result.produced = a.produced - b.consumed + b.produced;
        } else {
            // Stack underflow scenario
            result.consumed = a.consumed + b.consumed - a.produced;
            result.produced = b.produced;
            result.isKnown = false; // Mark as uncertain due to underflow
        }
    } else {
        // Conservative estimate when effects are unknown
        result.consumed = a.consumed + b.consumed;
        result.produced = a.produced + b.produced;
    }
    
    return result;
}

auto sequence(const std::vector<ASTNode::StackEffect>& effects) -> ASTNode::StackEffect {
    if (effects.empty()) {
        return ASTNode::StackEffect{0, 0, true};
    }
    
    ASTNode::StackEffect result = effects[0];
    for (size_t i = 1; i < effects.size(); ++i) {
        result = combine(result, effects[i]);
    }
    
    return result;
}

auto conditional(const ASTNode::StackEffect& condition,
                const ASTNode::StackEffect& thenBranch,
                const ASTNode::StackEffect& elseBranch) -> ASTNode::StackEffect {
    ASTNode::StackEffect result;
    
    // Condition always executes
    result.consumed = condition.consumed;
    result.isKnown = condition.isKnown && thenBranch.isKnown && elseBranch.isKnown;
    
    if (result.isKnown) {
        // Both branches must have same net effect for deterministic analysis
        int thenNet = thenBranch.produced - thenBranch.consumed;
        int elseNet = elseBranch.produced - elseBranch.consumed;
        
        if (thenNet == elseNet) {
            result.consumed += std::max(thenBranch.consumed, elseBranch.consumed);
            result.produced = thenNet; // Net effect after condition consumption
        } else {
            result.isKnown = false;
            // Conservative estimate
            result.consumed += std::max(thenBranch.consumed, elseBranch.consumed);
            result.produced = std::max(thenBranch.produced, elseBranch.produced);
        }
    }
    
    return result;
}

auto loop(const ASTNode::StackEffect& body,
         const ASTNode::StackEffect& condition) -> ASTNode::StackEffect {
    ASTNode::StackEffect result;
    
    // Loops are complex to analyze statically
    result.consumed = std::max(body.consumed, condition.consumed);
    result.produced = 0; // Conservative: assume loop consumes its stack items
    result.isKnown = body.isKnown && condition.isKnown;
    
    if (result.isKnown) {
        // Body should maintain stack balance for loop to be valid
        int bodyNet = body.produced - body.consumed;
        if (bodyNet != 0) {
            result.isKnown = false; // Unknown number of iterations affects stack
            result.produced = bodyNet; // Optimistic: one iteration net effect
        }
    }
    
    return result;
}

} // namespace StackEffectUtils
