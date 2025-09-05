#include "dictionary/dictionary.h"
#include "common/utils.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>

ForthDictionary::ForthDictionary() {
    initializeBuiltinWords();
    initializeMathWords();
    initializeControlWords();
    initializeStackWords();
    initializeMemoryWords();
}

auto ForthDictionary::defineWord(const std::string& name, std::unique_ptr<ASTNode> definition) -> void {
    const auto normalizedName = normalizeWordName(name);
    
    auto entry = std::make_unique<WordEntry>(normalizedName, WordEntry::WordType::USER_DEFINED);
    entry->definition = std::move(definition);
    entry->stackEffect = {0, 0, false}; // Will be analyzed later
    
    words[normalizedName] = std::move(entry);
}

auto ForthDictionary::defineBuiltinWord(const std::string& name, const std::string& cppCode,
                                       const ASTNode::StackEffect& effect) -> void {
    const auto normalizedName = normalizeWordName(name);
    
    auto entry = std::make_unique<WordEntry>(normalizedName, WordEntry::WordType::BUILTIN);
    entry->cppImplementation = cppCode;
    entry->stackEffect = effect;
    entry->isCompiled = true;
    
    words[normalizedName] = std::move(entry);
}

auto ForthDictionary::defineVariable(const std::string& name, std::unique_ptr<ASTNode> initialValue) -> void {
    const auto normalizedName = normalizeWordName(name);
    
    auto entry = std::make_unique<WordEntry>(normalizedName, WordEntry::WordType::VARIABLE);
    entry->definition = std::move(initialValue);
    entry->stackEffect = {0, 1, true}; // Variables push their address
    
    variables[normalizedName] = std::move(entry);
}

auto ForthDictionary::defineConstant(const std::string& name, std::unique_ptr<ASTNode> value) -> void {
    const auto normalizedName = normalizeWordName(name);
    
    auto entry = std::make_unique<WordEntry>(normalizedName, WordEntry::WordType::CONSTANT);
    entry->definition = std::move(value);
    entry->stackEffect = {0, 1, true}; // Constants push their value
    
    constants[normalizedName] = std::move(entry);
}

[[nodiscard]] auto ForthDictionary::lookupWord(const std::string& name) const -> WordEntry* {
    const auto normalizedName = normalizeWordName(name);
    
    // Check words first
    auto it = words.find(normalizedName);
    if (it != words.end()) {
        return it->second.get();
    }
    
    // Check variables
    auto varIt = variables.find(normalizedName);
    if (varIt != variables.end()) {
        return varIt->second.get();
    }
    
    // Check constants
    auto constIt = constants.find(normalizedName);
    if (constIt != constants.end()) {
        return constIt->second.get();
    }
    
    return nullptr;
}

[[nodiscard]] auto ForthDictionary::isWordDefined(const std::string& name) const -> bool {
    return lookupWord(name) != nullptr;
}

[[nodiscard]] auto ForthDictionary::isVariable(const std::string& name) const -> bool {
    const auto normalizedName = normalizeWordName(name);
    return variables.find(normalizedName) != variables.end();
}

[[nodiscard]] auto ForthDictionary::isConstant(const std::string& name) const -> bool {
    const auto normalizedName = normalizeWordName(name);
    return constants.find(normalizedName) != constants.end();
}

[[nodiscard]] auto ForthDictionary::getStackEffect(const std::string& wordName) const -> ASTNode::StackEffect {
    auto entry = lookupWord(wordName);
    if (entry) {
        return entry->stackEffect;
    }
    return {0, 0, false}; // Unknown word
}

auto ForthDictionary::initializeBuiltinWords() -> void {
    // Basic stack operations
    defineBuiltinWord("DUP", "forth_stack.push(forth_stack.top())", {1, 2, true});
    defineBuiltinWord("DROP", "forth_stack.pop()", {1, 0, true});
    defineBuiltinWord("SWAP", R"({
        auto a = forth_stack.pop();
        auto b = forth_stack.pop();
        forth_stack.push(a);
        forth_stack.push(b);
    })", {2, 2, true});
    defineBuiltinWord("OVER", R"({
        auto a = forth_stack.pop();
        auto b = forth_stack.top();
        forth_stack.push(a);
        forth_stack.push(b);
    })", {2, 3, true});
    defineBuiltinWord("ROT", R"({
        auto a = forth_stack.pop();
        auto b = forth_stack.pop();
        auto c = forth_stack.pop();
        forth_stack.push(b);
        forth_stack.push(a);
        forth_stack.push(c);
    })", {3, 3, true});
}

auto ForthDictionary::initializeMathWords() -> void {
    // Basic arithmetic
    defineBuiltinWord("+", R"({
        auto b = forth_stack.pop();
        auto a = forth_stack.pop();
        forth_stack.push(a + b);
    })", {2, 1, true});
    
    defineBuiltinWord("-", R"({
        auto b = forth_stack.pop();
        auto a = forth_stack.pop();
        forth_stack.push(a - b);
    })", {2, 1, true});
    
    defineBuiltinWord("*", R"({
        auto b = forth_stack.pop();
        auto a = forth_stack.pop();
        forth_stack.push(a * b);
    })", {2, 1, true});
    
    defineBuiltinWord("/", R"({
        auto b = forth_stack.pop();
        auto a = forth_stack.pop();
        forth_stack.push(a / b);
    })", {2, 1, true});
    
    defineBuiltinWord("MOD", R"({
        auto b = forth_stack.pop();
        auto a = forth_stack.pop();
        forth_stack.push(a % b);
    })", {2, 1, true});
    
    // Advanced math functions
    defineBuiltinWord("SQRT", "forth_stack.push(sqrt(forth_stack.pop()))", {1, 1, true});
    defineBuiltinWord("SIN", "forth_stack.push(sin(forth_stack.pop()))", {1, 1, true});
    defineBuiltinWord("COS", "forth_stack.push(cos(forth_stack.pop()))", {1, 1, true});
    defineBuiltinWord("TAN", "forth_stack.push(tan(forth_stack.pop()))", {1, 1, true});
    
    // Bitwise operations
    defineBuiltinWord("AND", R"({
        auto b = forth_stack.pop();
        auto a = forth_stack.pop();
        forth_stack.push(a & b);
    })", {2, 1, true});
    
    defineBuiltinWord("OR", R"({
        auto b = forth_stack.pop();
        auto a = forth_stack.pop();
        forth_stack.push(a | b);
    })", {2, 1, true});
    
    defineBuiltinWord("XOR", R"({
        auto b = forth_stack.pop();
        auto a = forth_stack.pop();
        forth_stack.push(a ^ b);
    })", {2, 1, true});
    
    defineBuiltinWord("NOT", "forth_stack.push(~forth_stack.pop())", {1, 1, true});
}

auto ForthDictionary::initializeControlWords() -> void {
    // Control flow words are handled specially by the parser
    // These entries are for reference/documentation
    defineBuiltinWord("IF", "/* Handled by parser */", {1, 0, true});
    defineBuiltinWord("THEN", "/* Handled by parser */", {0, 0, true});
    defineBuiltinWord("ELSE", "/* Handled by parser */", {0, 0, true});
    defineBuiltinWord("BEGIN", "/* Handled by parser */", {0, 0, true});
    defineBuiltinWord("UNTIL", "/* Handled by parser */", {1, 0, true});
    defineBuiltinWord("DO", "/* Handled by parser */", {2, 0, true});
    defineBuiltinWord("LOOP", "/* Handled by parser */", {0, 0, true});
}

auto ForthDictionary::initializeStackWords() -> void {
    // Additional stack manipulation words
    defineBuiltinWord("2DUP", R"({
        auto a = forth_stack.pop();
        auto b = forth_stack.top();
        forth_stack.push(a);
        forth_stack.push(b);
        forth_stack.push(a);
    })", {2, 4, true});
    
    defineBuiltinWord("2DROP", R"({
        forth_stack.pop();
        forth_stack.pop();
    })", {2, 0, true});
    
    defineBuiltinWord("2SWAP", R"({
        auto a = forth_stack.pop();
        auto b = forth_stack.pop();
        auto c = forth_stack.pop();
        auto d = forth_stack.pop();
        forth_stack.push(b);
        forth_stack.push(a);
        forth_stack.push(d);
        forth_stack.push(c);
    })", {4, 4, true});
}

auto ForthDictionary::initializeMemoryWords() -> void {
    // Memory access words
    defineBuiltinWord("@", "forth_stack.push(*reinterpret_cast<int32_t*>(forth_stack.pop()))", {1, 1, true});
    defineBuiltinWord("!", R"({
        auto addr = forth_stack.pop();
        auto value = forth_stack.pop();
        *reinterpret_cast<int32_t*>(addr) = value;
    })", {2, 0, true});
    
    defineBuiltinWord("C@", "forth_stack.push(*reinterpret_cast<char*>(forth_stack.pop()))", {1, 1, true});
    defineBuiltinWord("C!", R"({
        auto addr = forth_stack.pop();
        auto value = forth_stack.pop();
        *reinterpret_cast<char*>(addr) = static_cast<char>(value);
    })", {2, 0, true});
}

[[nodiscard]] auto ForthDictionary::normalizeWordName(const std::string& name) const -> std::string {
    return ForthUtils::toUpper(name);
}

auto ForthDictionary::printDictionary() const -> void {
    std::cout << "\n=== FORTH Dictionary ===\n";
    
    std::cout << "\nWords (" << words.size() << "):\n";
    for (const auto& [name, entry] : words) {
        std::cout << "  " << name << " (";
        switch (entry->type) {
            case WordEntry::WordType::BUILTIN: std::cout << "BUILTIN"; break;
            case WordEntry::WordType::USER_DEFINED: std::cout << "USER"; break;
            case WordEntry::WordType::MATH_BUILTIN: std::cout << "MATH"; break;
            case WordEntry::WordType::CONTROL_FLOW: std::cout << "CONTROL"; break;
            default: std::cout << "OTHER"; break;
        }
        std::cout << ")\n";
    }
    
    if (!variables.empty()) {
        std::cout << "\nVariables (" << variables.size() << "):\n";
        for (const auto& [name, entry] : variables) {
            std::cout << "  " << name << "\n";
        }
    }
    
    if (!constants.empty()) {
        std::cout << "\nConstants (" << constants.size() << "):\n";
        for (const auto& [name, entry] : constants) {
            std::cout << "  " << name << "\n";
        }
    }
}

[[nodiscard]] auto ForthDictionary::getDictionarySize() const -> size_t {
    return words.size() + variables.size() + constants.size();
}

// Dictionary Factory Implementation
[[nodiscard]] auto DictionaryFactory::create(Configuration config) -> std::unique_ptr<ForthDictionary> {
    auto dict = std::make_unique<ForthDictionary>();
    
    switch (config) {
        case Configuration::MINIMAL:
            // Default initialization is minimal
            break;
            
        case Configuration::STANDARD:
            // Add extended FORTH words
            dict->defineBuiltinWord("DEPTH", "forth_stack.push(forth_stack.size())", {0, 1, true});
            dict->defineBuiltinWord(".", "std::cout << forth_stack.pop() << ' '", {1, 0, true});
            dict->defineBuiltinWord("EMIT", "std::cout << static_cast<char>(forth_stack.pop())", {1, 0, true});
            break;
            
        case Configuration::MATH_ENHANCED:
            // Add advanced mathematical functions
            dict->defineBuiltinWord("ASIN", "forth_stack.push(asin(forth_stack.pop()))", {1, 1, true});
            dict->defineBuiltinWord("ACOS", "forth_stack.push(acos(forth_stack.pop()))", {1, 1, true});
            dict->defineBuiltinWord("ATAN", "forth_stack.push(atan(forth_stack.pop()))", {1, 1, true});
            dict->defineBuiltinWord("LOG", "forth_stack.push(log(forth_stack.pop()))", {1, 1, true});
            dict->defineBuiltinWord("EXP", "forth_stack.push(exp(forth_stack.pop()))", {1, 1, true});
            dict->defineBuiltinWord("POW", R"({
                auto b = forth_stack.pop();
                auto a = forth_stack.pop();
                forth_stack.push(pow(a, b));
            })", {2, 1, true});
            break;
            
        case Configuration::ESP32_OPTIMIZED:
            // Add ESP32-specific optimizations and functions
            dict->defineBuiltinWord("GPIO-SET", R"({
                auto pin = forth_stack.pop();
                auto level = forth_stack.pop();
                gpio_set_level(static_cast<gpio_num_t>(pin), level);
            })", {2, 0, true});
            
            dict->defineBuiltinWord("GPIO-GET", R"({
                auto pin = forth_stack.pop();
                forth_stack.push(gpio_get_level(static_cast<gpio_num_t>(pin)));
            })", {1, 1, true});
            
            dict->defineBuiltinWord("DELAY-MS", "vTaskDelay(forth_stack.pop() / portTICK_PERIOD_MS)", {1, 0, true});
            break;
    }
    
    return dict;
}

[[nodiscard]] auto ForthDictionary::getAllWords() const -> std::vector<const WordEntry*> {
    std::vector<const WordEntry*> result;
    result.reserve(words.size() + variables.size() + constants.size());
    
    for (const auto& [name, entry] : words) {
        result.push_back(entry.get());
    }
    for (const auto& [name, entry] : variables) {
        result.push_back(entry.get());
    }
    for (const auto& [name, entry] : constants) {
        result.push_back(entry.get());
    }
    
    return result;
}

[[nodiscard]] auto ForthDictionary::getUserDefinedWords() const -> std::vector<const WordEntry*> {
    std::vector<const WordEntry*> result;
    
    for (const auto& [name, entry] : words) {
        if (entry->type == WordEntry::WordType::USER_DEFINED) {
            result.push_back(entry.get());
        }
    }
    
    return result;
}

[[nodiscard]] auto ForthDictionary::getBuiltinWords() const -> std::vector<const WordEntry*> {
    std::vector<const WordEntry*> result;
    
    for (const auto& [name, entry] : words) {
        if (entry->type == WordEntry::WordType::BUILTIN || 
            entry->type == WordEntry::WordType::MATH_BUILTIN) {
            result.push_back(entry.get());
        }
    }
    
    return result;
}

auto ForthDictionary::clear() -> void {
    words.clear();
    variables.clear();
    constants.clear();
    definitionStack.clear();
    
    // Reinitialize built-in words
    initializeBuiltinWords();
    initializeMathWords();
    initializeControlWords();
    initializeStackWords();
    initializeMemoryWords();
}

[[nodiscard]] auto ForthDictionary::clone() const -> std::unique_ptr<ForthDictionary> {
    auto newDict = std::make_unique<ForthDictionary>();
    
    // Clear the new dictionary's default initialization
    newDict->words.clear();
    newDict->variables.clear();
    newDict->constants.clear();
    
    // Copy all entries (simplified deep copy)
    for (const auto& [name, entry] : words) {
        auto newEntry = std::make_unique<WordEntry>(entry->name, entry->type, entry->isImmediate);
        newEntry->cppImplementation = entry->cppImplementation;
        newEntry->stackEffect = entry->stackEffect;
        newEntry->isCompiled = entry->isCompiled;
        newEntry->compiledCode = entry->compiledCode;
        // Note: definition is not deep-copied for simplicity
        newDict->words[name] = std::move(newEntry);
    }
    
    for (const auto& [name, entry] : variables) {
        auto newEntry = std::make_unique<WordEntry>(entry->name, entry->type, entry->isImmediate);
        newEntry->stackEffect = entry->stackEffect;
        newDict->variables[name] = std::move(newEntry);
    }
    
    for (const auto& [name, entry] : constants) {
        auto newEntry = std::make_unique<WordEntry>(entry->name, entry->type, entry->isImmediate);
        newEntry->stackEffect = entry->stackEffect;
        newDict->constants[name] = std::move(newEntry);
    }
    
    return newDict;
}

// Forward reference handling
auto ForthDictionary::markForwardReference(const std::string& name) -> void {
    const auto normalizedName = normalizeWordName(name);
    
    // Create a placeholder entry
    auto entry = std::make_unique<WordEntry>(normalizedName, WordEntry::WordType::USER_DEFINED);
    entry->stackEffect = {0, 0, false}; // Unknown effect
    entry->isCompiled = false;
    
    words[normalizedName] = std::move(entry);
}

auto ForthDictionary::resolveForwardReference(const std::string& name, std::unique_ptr<ASTNode> definition) -> void {
    const auto normalizedName = normalizeWordName(name);
    
    auto it = words.find(normalizedName);
    if (it != words.end() && !it->second->isCompiled) {
        it->second->definition = std::move(definition);
        it->second->isCompiled = false; // Will be compiled later
    }
}

[[nodiscard]] auto ForthDictionary::hasUnresolvedReferences() const -> bool {
    for (const auto& [name, entry] : words) {
        if (entry->type == WordEntry::WordType::USER_DEFINED && 
            !entry->definition && 
            !entry->isCompiled) {
            return true;
        }
    }
    return false;
}
