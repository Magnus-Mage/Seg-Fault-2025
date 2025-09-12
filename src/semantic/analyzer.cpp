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
    
    // Pass 1: Collect all word definitions and give them placeholder effects
    for (const auto& child : program.getChildren()) {
        if (auto wordDef = dynamic_cast<WordDefinitionNode*>(child.get())) {
            const auto& wordName = wordDef->getWordName();
            // Give a placeholder effect initially
            wordEffects[wordName] = TypedStackEffect(ASTNode::StackEffect{1, 1, false});
            analyzedWords[wordName] = false;
        }
    }
    
    // Pass 2: Analyze word definitions (may need multiple iterations)
    bool changed = true;
    int iterations = 0;
    while (changed && iterations < 5) {
        changed = false;
        iterations++;
        
        for (const auto& child : program.getChildren()) {
            if (auto wordDef = dynamic_cast<WordDefinitionNode*>(child.get())) {
                const auto& wordName = wordDef->getWordName();
                if (!analyzedWords[wordName]) {
                    auto oldEffect = wordEffects[wordName];
                    
                    // Analyze this word definition
                    currentWordName = wordName;
                    inWordDefinition = true;
                    saveStackState();
                    currentStack.reset();
                    
                    auto newEffect = analyzeWordDefinition(*wordDef);
                    
                    restoreStackState();
                    inWordDefinition = false;
                    currentWordName.clear();
                    
                    // Check if effect changed significantly
                    if (newEffect.effect.consumed != oldEffect.effect.consumed ||
                        newEffect.effect.produced != oldEffect.effect.produced) {
                        changed = true;
                    }
                    
                    wordEffects[wordName] = newEffect;
                    analyzedWords[wordName] = true;
                }
            }
        }
    }
    
    // Pass 3: Analyze the actual program execution
    currentStack.reset();
    inWordDefinition = false;
    
    for (const auto& child : program.getChildren()) {
        if (!dynamic_cast<WordDefinitionNode*>(child.get())) {
            // Only analyze non-definition statements for program flow
            child->accept(*this);
        }
    }
    
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
    // WRONG: currentStack.reset(); // Starts with empty stack
    
    // CORRECT: Start assuming the word has its expected inputs
    currentStack.reset();
    
    // Give it plenty of stack to work with, track how much it actually needs
    const int ASSUMED_STACK_START = 10;
    currentStack.depth = ASSUMED_STACK_START;
    
    // Track the minimum depth reached
    int minDepthReached = ASSUMED_STACK_START;
    
    for (const auto& child : node.getChildren()) {
        child->accept(*this);
        minDepthReached = std::min(minDepthReached, currentStack.depth);
    }
    
    // Calculate the actual stack effect
    TypedStackEffect effect;
    
    // How far below our starting point did we go? That's what we consumed
    effect.effect.consumed = std::max(0, ASSUMED_STACK_START - minDepthReached);
    
    // What's our net change from the starting point?
    int netChange = currentStack.depth - ASSUMED_STACK_START;
    effect.effect.produced = effect.effect.consumed + netChange;
    
    effect.effect.isKnown = currentStack.isValid;
    
    return effect;
}

void SemanticAnalyzer::visit(WordCallNode& node) {
    const auto& wordName = node.getWordName();
    
    // Get or calculate the word's stack effect
    auto effect = calculateWordEffect(wordName);
    
    if (!effect.effect.isKnown) {
        if (wordName == currentWordName) {
            // Recursive call - assume it maintains stack balance
            effect.effect = {1, 1, true};
        } else {
            addWarning("Unknown stack effect for word: " + wordName, node);
            effect.effect = {0, 0, false};
            return; // Don't try to apply unknown effects
        }
    }
    
    if (inWordDefinition) {
        // In word definition, allow negative depth tracking
        int oldDepth = currentStack.depth;
        currentStack.depth -= effect.effect.consumed;
        currentStack.depth += effect.effect.produced;
        
        // Track minimum depth reached (this can be negative)
        currentStack.minDepth = std::min(currentStack.minDepth, currentStack.depth);
        currentStack.maxDepth = std::max(currentStack.maxDepth, currentStack.depth);
        
        // Don't mark as invalid just because we went negative in word definition
        if (oldDepth >= 0 && currentStack.depth < 0) {
            // We crossed into negative - this is the actual consumption requirement
        }
    } else {
        // Normal program-level execution - must not underflow
        if (currentStack.depth < effect.effect.consumed) {
            addError("Stack underflow calling word: " + wordName, node);
            currentStack.isValid = false;
            return;
        }
        currentStack.depth -= effect.effect.consumed;
        currentStack.depth += effect.effect.produced;
        currentStack.maxDepth = std::max(currentStack.maxDepth, currentStack.depth);
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
   StackState afterElse = beforeBranches;  // Start with state before IF

    if (node.getElseBranch()) {
        // Has explicit ELSE branch
        saveStackState();
        for (const auto& child : node.getElseBranch()->getChildren()) {
            child->accept(*this);
        }
        afterElse = currentStack;
        restoreStackState();
    } 
    // If no ELSE branch, afterElse remains as beforeBranches (no-op)
    
    // Now merge - both branches should end at same level
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
        return TypedStackEffect(ASTNode::StackEffect{1, 1, true});  // fixed missing NEGATE
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
    
    // Both states must be individually valid
    if (!state1.isValid || !state2.isValid) {
        merged.isValid = false;
        return merged;
    }
    
    // For IF-THEN-ELSE, both branches should end at same stack level
    if (state1.depth == state2.depth) {
        merged.depth = state1.depth;
        merged.isValid = true;
    } else {
        // Different depths - this is actually an error in FORTH
        merged.isValid = false;
    }
    
    merged.minDepth = std::min(state1.minDepth, state2.minDepth);
    merged.maxDepth = std::max(state1.maxDepth, state2.maxDepth);
    
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

auto isBalanced(const ASTNode::StackEffect& effect) -> bool {
    return effect.consumed == effect.produced;
}

auto wouldUnderflow(const ASTNode::StackEffect& effect, int currentDepth) -> bool {
    return effect.consumed > currentDepth;
}

auto calculateMinRequiredDepth(const std::vector<ASTNode::StackEffect>& effects) -> int {
    int minRequired = 0;
    int currentDepth = 0;
    
    for (const auto& effect : effects) {
        minRequired = std::max(minRequired, effect.consumed - currentDepth);
        currentDepth = currentDepth - effect.consumed + effect.produced;
    }
    
    return minRequired;
}

auto optimizeEffectSequence(std::vector<ASTNode::StackEffect>& effects) -> void {
    // Basic optimization: combine adjacent effects where possible
    for (size_t i = 0; i < effects.size() - 1; ++i) {
        if (effects[i].isKnown && effects[i+1].isKnown) {
            auto combined = combine(effects[i], effects[i+1]);
            if (combined.isKnown) {
                effects[i] = combined;
                effects.erase(effects.begin() + i + 1);
                --i; // Check this position again
            }
        }
    }
}

} // namespace StackEffectUtils


SemanticAnalysisManager::SemanticAnalysisManager() 
    : analyzer(std::make_unique<SemanticAnalyzer>()) {
}

SemanticAnalysisManager::SemanticAnalysisManager(const SemanticAnalyzer::AnalysisOptions& opts) 
    : analyzer(std::make_unique<SemanticAnalyzer>()), options(opts) {
    if (analyzer) {
	    analyzer->setOptions(options);
    }
}

auto SemanticAnalysisManager::analyzeProgram(ProgramNode& program, const ForthDictionary& dictionary) -> SemanticReport {
    analyzer->setDictionary(&dictionary);
    analyzer->analyze(program);
    
    SemanticReport report;
    report.errors = analyzer->getErrors();
    report.warnings = analyzer->getWarnings();
    report.maxStackDepth = analyzer->getMaxStackDepth();
    report.minStackDepth = analyzer->getMinStackDepth();
    report.wordEffects = {};
    
    for (const auto& [word, effect] : analyzer->getWordEffects()) {
        report.wordEffects[word] = effect.effect;
    }
    
    return report;
}

auto SemanticAnalysisManager::setOptions(const SemanticAnalyzer::AnalysisOptions& opts) -> void {
    options = opts;
    analyzer->setOptions(options);
}

auto SemanticAnalyzer::isRecursiveCall(const std::string& wordName) const -> bool {
    return wordName == currentWordName;
}

auto SemanticAnalyzer::validateStackBalance(ASTNode& node) -> bool {
    auto effect = node.getStackEffect();
    if (!effect.isKnown) {
        return true; // Can't validate unknown effects
    }
    return effect.consumed <= currentStack.depth;
}

auto SemanticAnalyzer::inferType(ASTNode& node) -> ForthValueType {
    switch (node.getType()) {
        case ASTNode::NodeType::NUMBER_LITERAL:
            return ForthValueType::INTEGER;
        case ASTNode::NodeType::STRING_LITERAL:
            return ForthValueType::STRING_ADDR;
        default:
            return ForthValueType::UNKNOWN;
    }
}

auto SemanticAnalyzer::checkTypeCompatibility(ForthValueType expected, ForthValueType actual) -> bool {
    return expected == actual || expected == ForthValueType::CELL || actual == ForthValueType::CELL;
}

auto SemanticAnalyzer::analyzeControlFlow(ASTNode& node) -> TypedStackEffect {
    // Basic implementation
    return TypedStackEffect(node.getStackEffect());
}
