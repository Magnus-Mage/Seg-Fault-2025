#include "codegen/cpp_codegen.h"
#include "common/utils.h"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>

CppCodeGenerator::CppCodeGenerator(std::unique_ptr<ForthDictionary> dict, 
                                   const GenerationConfig& cfg)
    : config(cfg), dictionary(std::move(dict)), indentLevel(0) {
    
    // Initialize stack state
    stackState = {0, 0, true};
    
    // Add required ESP-IDF includes
    if (config.generateESP32Specific) {
        requiredIncludes.insert("freertos/FreeRTOS.h");
        requiredIncludes.insert("freertos/task.h");
        requiredIncludes.insert("driver/gpio.h");
        requiredIncludes.insert("esp_log.h");
    }
    
    // Always include basic C++ headers
    requiredIncludes.insert("iostream");
    requiredIncludes.insert("stack");
    requiredIncludes.insert("cmath");
    requiredIncludes.insert("cstdint");
}

auto CppCodeGenerator::generateCode(ASTNode& root) -> GeneratedCode {
    // Reset generation state
    currentOutput.str("");
    headerStream.str("");
    methodStream.str("");
    mainStream.str("");
    generatedMethods.clear();
    userDefinedWords.clear();
    indentLevel = 0;
    stackState = {0, 0, true};
    
    // Generate includes
    generateESP32Includes();
    
    // Generate stack class
    generateStackClass();
    
    // Start main class generation
    headerStream << "\nclass ForthProgram {\nprivate:\n";
    headerStream << "    ForthStack forth_stack;\n";
    headerStream << "    static const char* TAG;\n";
    
    // Generate variable and constant declarations
    generateVariableDeclarations();
    generateConstantDeclarations();
    
    headerStream << "\npublic:\n";
    headerStream << "    ForthProgram() : forth_stack(" << config.defaultStackSize << ") {}\n";
    headerStream << "    ~ForthProgram() = default;\n\n";
    
    // Visit the AST to generate methods
    root.accept(*this);
    
    // Generate main ESP32 functions
    generateESP32Setup();
    generateUtilityFunctions();
    
    // Close class
    headerStream << "};\n\n";
    headerStream << "const char* ForthProgram::TAG = \"FORTH\";\n\n";
    
    // Generate main function
    generateESP32TaskStructure();
    
    // Apply optimizations if enabled
    if (config.optimizeStackOperations) {
        optimizeStackOperations();
    }
    
    // Format the final code
    formatGeneratedCode();
    
    // Assemble the complete generated code
    GeneratedCode result;
    result.headerIncludes = headerStream.str();
    result.classDeclaration = "";  // Included in headerIncludes
    result.methodImplementations = methodStream.str();
    result.mainFunction = mainStream.str();
    result.cmakeListsContent = generateCMakeLists(result, "forth_program");
    result.requiredESPComponents = {"driver", "esp_common", "freertos", "log"};
    
    return result;
}

// AST Visitor implementations
auto CppCodeGenerator::visit(ProgramNode& node) -> void {
    emitComment("Generated from FORTH Program");
    
    for (size_t i = 0; i < node.getChildCount(); ++i) {
        auto child = node.getChild(i);
        if (child) {
            child->accept(*this);
        }
    }
}

auto CppCodeGenerator::visit(WordDefinitionNode& node) -> void {
    const std::string methodName = sanitizeIdentifier(node.getWordName());
    
    if (generatedMethods.find(methodName) != generatedMethods.end()) {
        return; // Already generated
    }
    
    generatedMethods.insert(methodName);
    userDefinedWords.push_back(node.getWordName());
    
    // Generate method signature
    methodStream << "    auto " << methodName << "() -> void {\n";
    
    if (config.includeDebugInfo) {
        methodStream << "        ESP_LOGD(TAG, \"Executing word: " << node.getWordName() << "\");\n";
    }
    
    // Generate stack check if enabled
    if (config.includeStackChecks) {
        auto effect = node.getStackEffect();
        if (effect.isKnown && effect.consumed > 0) {
            methodStream << "        if (forth_stack.size() < " << effect.consumed << ") {\n";
            methodStream << "            ESP_LOGE(TAG, \"Stack underflow in " << node.getWordName() << "\");\n";
            methodStream << "            return;\n";
            methodStream << "        }\n";
        }
    }
    
    indentLevel = 2;
    
    // Generate method body
    for (size_t i = 0; i < node.getChildCount(); ++i) {
        auto child = node.getChild(i);
        if (child) {
            child->accept(*this);
        }
    }
    
    methodStream << "    }\n\n";
    indentLevel = 0;
}

auto CppCodeGenerator::visit(WordCallNode& node) -> void {
    const std::string wordName = node.getWordName();
    
    if (isBuiltinWord(wordName)) {
        generateBuiltinWord(wordName);
    } else {
        // User-defined word call
        const std::string methodName = sanitizeIdentifier(wordName);
        emitIndented(methodName + "();");
    }
    
    // Update stack state
    auto effect = dictionary->getStackEffect(wordName);
    updateStackState(effect);
}

auto CppCodeGenerator::visit(NumberLiteralNode& node) -> void {
    const std::string value = node.getValue();
    
    if (node.isFloatingPoint()) {
        emitIndented("forth_stack.push(static_cast<" + config.stackType + ">(" + value + "));");
    } else {
        emitIndented("forth_stack.push(" + value + ");");
    }
    
    updateStackState({0, 1, true});
}

auto CppCodeGenerator::visit(StringLiteralNode& node) -> void {
    if (node.isPrint()) {
        // Print string immediately
        emitIndented("std::cout << \"" + node.getValue() + "\";");
        if (config.generateESP32Specific) {
            emitIndented("ESP_LOGI(TAG, \"%s\", \"" + node.getValue() + "\");");
        }
    } else {
        // Push string address and length (simplified for now)
        emitIndented("// String literal: " + node.getValue());
        emitIndented("forth_stack.push(reinterpret_cast<" + config.stackType + ">(\"" + node.getValue() + "\"));");
        emitIndented("forth_stack.push(" + std::to_string(node.getValue().length()) + ");");
        updateStackState({0, 2, true});
    }
}

auto CppCodeGenerator::visit(IfStatementNode& node) -> void {
    std::string label = generateConditionalStart();
    
    // Generate condition check
    emitIndented("if (forth_stack.pop() != 0) {");
    increaseIndent();
    
    // Generate THEN branch
    if (node.getThenBranch()) {
        for (const auto& child : node.getThenBranch()->getChildren()) {
            child->accept(*this);
        }
    }
    
    decreaseIndent();
    
    if (node.hasElse()) {
        emitIndented("} else {");
        increaseIndent();
        
        // Generate ELSE branch
        if (node.getElseBranch()) {
            for (const auto& child : node.getElseBranch()->getChildren()) {
                child->accept(*this);
            }
        }
        
        decreaseIndent();
    }
    
    emitIndented("}");
    
    updateStackState({1, 0, true}); // IF consumes one item
}

auto CppCodeGenerator::visit(BeginUntilLoopNode& node) -> void {
    std::string label = generateLoopStart();
    
    emitIndented("do {");
    increaseIndent();
    
    // Generate loop body
    if (node.getBody()) {
        for (const auto& child : node.getBody()->getChildren()) {
            child->accept(*this);
        }
    }
    
    decreaseIndent();
    emitIndented("} while (forth_stack.pop() == 0);");
    
    updateStackState({1, 0, true}); // UNTIL consumes one item
}

auto CppCodeGenerator::visit(MathOperationNode& node) -> void {
    generateBuiltinWord(node.getOperation());
    updateStackState(node.getStackEffect());
}

auto CppCodeGenerator::visit(VariableDeclarationNode& node) -> void {
    // Variables are handled in generateVariableDeclarations()
    // This visit is for runtime variable access
    const std::string varName = sanitizeIdentifier(node.getVarName());
    
    if (node.isConst()) {
        emitIndented("forth_stack.push(" + varName + ");");
        updateStackState({0, 1, true});
    } else {
        emitIndented("forth_stack.push(reinterpret_cast<" + config.stackType + ">(&" + varName + "));");
        updateStackState({0, 1, true});
    }
}

// Helper method implementations
auto CppCodeGenerator::generateBuiltinWord(const std::string& wordName) -> void {
    auto entry = dictionary->lookupWord(wordName);
    if (entry && !entry->cppImplementation.empty()) {
        std::string code = entry->cppImplementation;
        
        // Replace any placeholder patterns if needed
        // For now, just emit the code directly
        emitIndented(code);
    } else {
        emitComment("Unknown builtin word: " + wordName);
        emitIndented("// TODO: Implement " + wordName);
    }
}

auto CppCodeGenerator::isBuiltinWord(const std::string& wordName) const -> bool {
    auto entry = dictionary->lookupWord(wordName);
    return entry && (entry->type == WordEntry::WordType::BUILTIN || 
                    entry->type == WordEntry::WordType::MATH_BUILTIN);
}

auto CppCodeGenerator::generateStackClass() -> void {
    headerStream << R"(
// FORTH Stack Implementation
class ForthStack {
private:
    std::vector<)" << config.stackType << R"(> stack;
    size_t maxSize;

public:
    explicit ForthStack(size_t max_size = 1024) : maxSize(max_size) {
        stack.reserve(max_size);
    }
    
    auto push()" << config.stackType << R"( value) -> void {
        if (stack.size() >= maxSize) {
            throw std::runtime_error("Stack overflow");
        }
        stack.push_back(value);
    }
    
    auto pop() -> )" << config.stackType << R"( {
        if (stack.empty()) {
            throw std::runtime_error("Stack underflow");
        }
        auto value = stack.back();
        stack.pop_back();
        return value;
    }
    
    auto top() -> )" << config.stackType << R"(& {
        if (stack.empty()) {
            throw std::runtime_error("Stack empty");
        }
        return stack.back();
    }
    
    auto size() const -> size_t { return stack.size(); }
    auto empty() const -> bool { return stack.empty(); }
    auto clear() -> void { stack.clear(); }
};
)";
}

auto CppCodeGenerator::generateESP32Includes() -> void {
    for (const auto& include : requiredIncludes) {
        if (include.find('/') != std::string::npos) {
            // ESP-IDF style include
            headerStream << "#include \"" << include << "\"\n";
        } else {
            // Standard C++ include
            headerStream << "#include <" << include << ">\n";
        }
    }
    headerStream << "\n";
}

auto CppCodeGenerator::generateVariableDeclarations() -> void {
    for (const auto& entry : dictionary->getAllWords()) {
        if (entry->type == WordEntry::WordType::VARIABLE) {
            headerStream << "    " << config.stackType << " " 
                        << sanitizeIdentifier(entry->name) << " = 0;\n";
        }
    }
}

auto CppCodeGenerator::generateConstantDeclarations() -> void {
    for (const auto& entry : dictionary->getAllWords()) {
        if (entry->type == WordEntry::WordType::CONSTANT) {
            headerStream << "    static constexpr " << config.stackType << " " 
                        << sanitizeIdentifier(entry->name) << " = 0; // TODO: Get actual value\n";
        }
    }
}

auto CppCodeGenerator::generateESP32Setup() -> void {
    methodStream << R"(    auto setup() -> void {
        ESP_LOGI(TAG, "FORTH Program Starting");
        
        // Initialize GPIO if needed
        gpio_install_isr_service(0);
        
        // Run main FORTH program
        run();
    }

    auto run() -> void {
        ESP_LOGI(TAG, "Running FORTH program");
        
        // Execute main program (user-defined words)
)";

    for (const std::string& word : userDefinedWords) {
        if (word != "MAIN" && word != "SETUP") {
            methodStream << "        // " << word << "();\n";
        }
    }

    methodStream << R"(    }
)";
}

auto CppCodeGenerator::generateESP32TaskStructure() -> void {
    mainStream << R"(
// ESP32 Task Function
extern "C" void app_main() {
    ForthProgram program;
    program.setup();
    
    // Keep the program running
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
)";
}

// Utility methods
auto CppCodeGenerator::emit(const std::string& code) -> void {
    currentOutput << code;
}

auto CppCodeGenerator::emitLine(const std::string& line) -> void {
    currentOutput << line << "\n";
}

auto CppCodeGenerator::emitIndented(const std::string& code) -> void {
    methodStream << getIndent() << code << "\n";
}

auto CppCodeGenerator::emitComment(const std::string& comment) -> void {
    methodStream << getIndent() << "// " << comment << "\n";
}

auto CppCodeGenerator::getIndent() const -> std::string {
    return std::string(indentLevel * 4, ' ');
}

auto CppCodeGenerator::sanitizeIdentifier(const std::string& name) const -> std::string {
    std::string result = name;
    
    // Replace invalid characters
    std::replace(result.begin(), result.end(), '-', '_');
    std::replace(result.begin(), result.end(), '+', '_PLUS');
    std::replace(result.begin(), result.end(), '*', '_MUL');
    std::replace(result.begin(), result.end(), '/', '_DIV');
    std::replace(result.begin(), result.end(), '=', '_EQ');
    std::replace(result.begin(), result.end(), '<', '_LT');
    std::replace(result.begin(), result.end(), '>', '_GT');
    
    // Ensure it doesn't start with a number
    if (!result.empty() && std::isdigit(result[0])) {
        result = "WORD_" + result;
    }
    
    return result;
}

auto CppCodeGenerator::generateConditionalStart() -> std::string {
    static int labelCounter = 0;
    return "if_" + std::to_string(++labelCounter);
}

auto CppCodeGenerator::generateLoopStart() -> std::string {
    static int labelCounter = 0;
    return "loop_" + std::to_string(++labelCounter);
}

auto CppCodeGenerator::updateStackState(const ASTNode::StackEffect& effect) -> void {
    if (effect.isKnown) {
        stackState.currentDepth = stackState.currentDepth - effect.consumed + effect.produced;
        if (stackState.currentDepth > stackState.maxDepth) {
            stackState.maxDepth = stackState.currentDepth;
        }
    } else {
        stackState.depthKnown = false;
    }
}

auto CppCodeGenerator::generateUtilityFunctions() -> void {
    methodStream << R"(
    // Utility functions
    auto printStackTrace() -> void {
        ESP_LOGI(TAG, "Stack size: %zu", forth_stack.size());
        // TODO: Print actual stack contents
    }
)";
}

auto CppCodeGenerator::optimizeStackOperations() -> void {
    // TODO: Implement stack operation optimizations
    // This would analyze the generated code and optimize redundant stack operations
}

auto CppCodeGenerator::formatGeneratedCode() -> void {
    // TODO: Implement code formatting if needed
}

auto CppCodeGenerator::generateCMakeLists(const GeneratedCode& code, const std::string& projectName) -> std::string {
    std::ostringstream cmake;
    
    cmake << "# Generated CMakeLists.txt for FORTH-ESP32 project\n";
    cmake << "cmake_minimum_required(VERSION 3.16)\n\n";
    cmake << "include($ENV{IDF_PATH}/tools/cmake/project.cmake)\n";
    cmake << "project(" << projectName << ")\n";
    
    return cmake.str();
}
