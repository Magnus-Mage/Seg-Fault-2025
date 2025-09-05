#ifndef FORTH_DICTIONARY_H
#define FORTH_DICTIONARY_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "parser/ast.h"
#include "common/types.h"

// Word entry in the dictionary
struct WordEntry {
    enum class WordType {
        BUILTIN,        // Built-in FORTH words
        USER_DEFINED,   // User-defined words with : name ... ;
        MATH_BUILTIN,   // Mathematical operations
        CONTROL_FLOW,   // Control flow words (IF, THEN, etc.)
        VARIABLE,       // Variable declarations
        CONSTANT,       // Constant declarations
        IMMEDIATE       // Immediate words (execute during compilation)
    };
    
    std::string name;
    WordType type;
    bool isImmediate;
    
    // For user-defined words
    std::unique_ptr<ASTNode> definition; // AST of the word definition
    
    // For built-in words
    std::string cppImplementation; // C++ code template
    
    // Stack effect information
    ASTNode::StackEffect stackEffect;
    
    // Compilation information
    bool isCompiled;
    std::string compiledCode;
    
    WordEntry(const std::string& n, WordType t, bool immediate = false)
        : name(n), type(t), isImmediate(immediate), isCompiled(false) {}
};

class ForthDictionary {
private:
    std::unordered_map<std::string, std::unique_ptr<WordEntry>> words;
    std::unordered_map<std::string, std::unique_ptr<WordEntry>> variables;
    std::unordered_map<std::string, std::unique_ptr<WordEntry>> constants;
    
    // Stack for nested word definitions (if needed)
    std::vector<std::string> definitionStack;
    
public:
    ForthDictionary();
    ~ForthDictionary() = default;
    
    // Core dictionary operations
    auto defineWord(const std::string& name, std::unique_ptr<ASTNode> definition) -> void;
    auto defineBuiltinWord(const std::string& name, const std::string& cppCode,
                          const ASTNode::StackEffect& effect = {0, 0, true}) -> void;
    auto defineVariable(const std::string& name, std::unique_ptr<ASTNode> initialValue = nullptr) -> void;
    auto defineConstant(const std::string& name, std::unique_ptr<ASTNode> value) -> void;
    
    // Lookup operations
    [[nodiscard]] auto lookupWord(const std::string& name) const -> WordEntry*;
    [[nodiscard]] auto isWordDefined(const std::string& name) const -> bool;
    [[nodiscard]] auto isVariable(const std::string& name) const -> bool;
    [[nodiscard]] auto isConstant(const std::string& name) const -> bool;
    
    // Iteration support
    [[nodiscard]] auto getAllWords() const -> std::vector<const WordEntry*>;
    [[nodiscard]] auto getUserDefinedWords() const -> std::vector<const WordEntry*>;
    [[nodiscard]] auto getBuiltinWords() const -> std::vector<const WordEntry*>;
    
    // Dictionary state management
    auto clear() -> void;
    auto clone() const -> std::unique_ptr<ForthDictionary>;
    
    // Stack effect analysis
    [[nodiscard]] auto getStackEffect(const std::string& wordName) const -> ASTNode::StackEffect;
    
    // Forward reference handling
    auto markForwardReference(const std::string& name) -> void;
    auto resolveForwardReference(const std::string& name, std::unique_ptr<ASTNode> definition) -> void;
    [[nodiscard]] auto hasUnresolvedReferences() const -> bool;
    
    // Debugging and introspection
    auto printDictionary() const -> void;
    [[nodiscard]] auto getDictionarySize() const -> size_t;
    
private:
    auto initializeBuiltinWords() -> void;
    auto initializeMathWords() -> void;
    auto initializeControlWords() -> void;
    auto initializeStackWords() -> void;
    auto initializeMemoryWords() -> void;
    
    [[nodiscard]] auto normalizeWordName(const std::string& name) const -> std::string;
};

// Dictionary factory for different configurations
class DictionaryFactory {
public:
    enum class Configuration {
        MINIMAL,        // Basic FORTH words only
        STANDARD,       // Standard FORTH words
        MATH_ENHANCED,  // Standard + advanced math
        ESP32_OPTIMIZED // ESP32-specific optimizations
    };
    
    [[nodiscard]] static auto create(Configuration config) -> std::unique_ptr<ForthDictionary>;
};

#endif // FORTH_DICTIONARY_H
