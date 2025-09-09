#ifndef FORTH_CPP_CODEGEN_H
#define FORTH_CPP_CODEGEN_H

#include <string>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <memory>
#include "parser/ast.h"
#include "dictionary/dictionary.h"

class CppCodeGenerator : public ASTVisitor {
public:
    struct GenerationConfig {
        bool includeDebugInfo = false;
        bool optimizeStackOperations = true;
        bool includeStackChecks = true;
        bool generateESP32Specific = true;
        std::string stackType = "int32_t";
        size_t defaultStackSize = 1024;
    };

    struct GeneratedCode {
        std::string headerIncludes;
        std::string classDeclaration;
        std::string methodImplementations;
        std::string mainFunction;
        std::string cmakeListsContent;
        std::vector<std::string> requiredESPComponents;
    };

private:
    GenerationConfig config;
    std::unique_ptr<ForthDictionary> dictionary;
    
    // Code generation state
    std::ostringstream currentOutput;
    std::ostringstream headerStream;
    std::ostringstream methodStream;
    std::ostringstream mainStream;
    
    // Generation tracking
    std::unordered_set<std::string> generatedMethods;
    std::unordered_set<std::string> requiredIncludes;
    std::vector<std::string> userDefinedWords;
    int indentLevel;
    
    // Stack analysis
    struct StackState {
        int currentDepth;
        int maxDepth;
        bool depthKnown;
    };
    StackState stackState;

public:
    explicit CppCodeGenerator(std::unique_ptr<ForthDictionary> dict, 
                             const GenerationConfig& cfg = GenerationConfig{});
    
    ~CppCodeGenerator() = default;

    // Main generation interface
    [[nodiscard]] auto generateCode(ASTNode& root) -> GeneratedCode;
    
    // AST Visitor implementation
    auto visit(ProgramNode& node) -> void override;
    auto visit(WordDefinitionNode& node) -> void override;
    auto visit(WordCallNode& node) -> void override;
    auto visit(NumberLiteralNode& node) -> void override;
    auto visit(StringLiteralNode& node) -> void override;
    auto visit(IfStatementNode& node) -> void override;
    auto visit(BeginUntilLoopNode& node) -> void override;
    auto visit(MathOperationNode& node) -> void override;
    auto visit(VariableDeclarationNode& node) -> void override;

    // Configuration
    auto setConfig(const GenerationConfig& cfg) -> void { config = cfg; }
    [[nodiscard]] auto getConfig() const -> const GenerationConfig& { return config; }

    // Utility methods for external use
    [[nodiscard]] static auto generateESPIDFProject(const GeneratedCode& code, 
                                                   const std::string& projectName) -> std::string;
    [[nodiscard]] static auto generateCMakeLists(const GeneratedCode& code,
                                                const std::string& projectName) -> std::string;

private:
    // Code generation helpers
    auto emit(const std::string& code) -> void;
    auto emitLine(const std::string& line) -> void;
    auto emitIndented(const std::string& code) -> void;
    auto emitComment(const std::string& comment) -> void;
    
    // Indentation management
    auto increaseIndent() -> void { indentLevel++; }
    auto decreaseIndent() -> void { if (indentLevel > 0) indentLevel--; }
    [[nodiscard]] auto getIndent() const -> std::string;
    
    // Stack management code generation
    auto generateStackPush(const std::string& value) -> void;
    auto generateStackPop(const std::string& variable = "") -> void;
    auto generateStackTop() -> std::string;
    auto generateStackCheck(int requiredDepth) -> void;
    auto updateStackState(const ASTNode::StackEffect& effect) -> void;
    
    // Method generation
    auto generateMethodSignature(const std::string& methodName, 
                                const std::string& returnType = "void") -> void;
    auto generateMethodHeader(const std::string& methodName) -> void;
    auto generateMethodFooter() -> void;
    
    // Built-in word handling
    auto generateBuiltinWord(const std::string& wordName) -> void;
    auto isBuiltinWord(const std::string& wordName) const -> bool;
    
    // Control flow generation
    auto generateConditionalStart() -> std::string; // Returns label
    auto generateConditionalElse(const std::string& label) -> std::string;
    auto generateConditionalEnd(const std::string& label) -> void;
    auto generateLoopStart() -> std::string; // Returns label
    auto generateLoopEnd(const std::string& label) -> void;
    
    // ESP32 specific generation
    auto generateESP32Setup() -> void;
    auto generateESP32Includes() -> void;
    auto generateESP32TaskStructure() -> void;
    auto isESP32SpecificWord(const std::string& wordName) const -> bool;
    
    // Optimization passes
    auto optimizeStackOperations() -> void;
    auto eliminateDeadCode() -> void;
    auto optimizeConstants() -> void;
    
    // Code formatting
    auto formatGeneratedCode() -> void;
    [[nodiscard]] auto sanitizeIdentifier(const std::string& name) const -> std::string;
    
    // Error handling
    auto generateErrorCheck(const std::string& condition, const std::string& message) -> void;
    
    // Memory management
    auto generateVariableDeclarations() -> void;
    auto generateConstantDeclarations() -> void;
    
    // Utility code generation
    auto generateStackClass() -> void;
    auto generateUtilityFunctions() -> void;
    auto generateDebugCode() -> void;
};

// Utility functions for ESP-IDF project generation
namespace ESPIDFGenerator {
    struct ProjectStructure {
        std::string mainCppContent;
        std::string cmakeListsContent;
        std::string partitionsContent;
        std::string menuConfigContent;
    };
    
    [[nodiscard]] auto generateProjectStructure(const CppCodeGenerator::GeneratedCode& code,
                                               const std::string& projectName) -> ProjectStructure;
    
    [[nodiscard]] auto generateComponentCMake(const std::vector<std::string>& components) -> std::string;
    
    [[nodiscard]] auto generateSDKConfig(bool includeDebug = false) -> std::string;
}

#endif // FORTH_CPP_CODEGEN_H
