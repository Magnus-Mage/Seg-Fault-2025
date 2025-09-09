#ifndef FORTH_C_CODEGEN_H
#define FORTH_C_CODEGEN_H

#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "parser/ast.h"
#include "semantic/analyzer.h"
#include "dictionary/dictionary.h"

class ForthCCodegen : public ASTVisitor {
private:
    // Output streams
    std::ostringstream headerStream;
    std::ostringstream sourceStream;
    std::ostringstream functionsStream;
    
    // Code generation state
    std::string moduleName;
    std::string targetPlatform;
    int indentLevel;
    int tempVarCounter;
    int labelCounter;
    
    // Analysis context
    const SemanticAnalyzer* semanticAnalyzer;
    const ForthDictionary* dictionary;
    
    // Generated code tracking
    std::unordered_set<std::string> generatedWords;
    std::unordered_map<std::string, std::string> wordFunctionNames;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

public:
    // ESP32 specific settings
    struct ESP32Config {
        bool useTasking = false;
        bool useWiFi = false;
        bool useGPIO = true;
        bool useTimer = false;
        size_t stackSize = 1024;
        int priority = 5;
    } esp32Config;

    explicit ForthCCodegen(const std::string& name = "forth_program");
    ~ForthCCodegen() = default;
    
    // Configuration
    void setTarget(const std::string& target) { targetPlatform = target; }
    void setSemanticAnalyzer(const SemanticAnalyzer* analyzer) { semanticAnalyzer = analyzer; }
    void setDictionary(const ForthDictionary* dict) { dictionary = dict; }
    void setESP32Config(const ESP32Config& config) { esp32Config = config; }
    
    // Main generation interface
    bool generateCode(const ProgramNode& program);
    
    // Output methods
    std::string getHeaderCode() const { return headerStream.str(); }
    std::string getSourceCode() const { return sourceStream.str(); }
    std::string getCompleteCode() const;
    
    // File output
    bool writeToFiles(const std::string& baseName);
    bool writeHeaderFile(const std::string& filename);
    bool writeSourceFile(const std::string& filename);
    
    // ESP-IDF integration
    std::string generateESPIDFComponent() const;
    std::string generateMainComponent() const;
    bool writeESPIDFProject(const std::string& projectPath);
    
    // Error handling
    bool hasErrors() const { return !errors.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }
    const std::vector<std::string>& getWarnings() const { return warnings; }
    void clearErrors() { errors.clear(); warnings.clear(); }
    
    // ASTVisitor implementation
    void visit(ProgramNode& node) override;
    void visit(WordDefinitionNode& node) override;
    void visit(WordCallNode& node) override;
    void visit(NumberLiteralNode& node) override;
    void visit(StringLiteralNode& node) override;
    void visit(IfStatementNode& node) override;
    void visit(BeginUntilLoopNode& node) override;
    void visit(MathOperationNode& node) override;
    void visit(VariableDeclarationNode& node) override;
    
    // Statistics
    struct CodeGenStats {
        size_t linesGenerated;
        size_t functionsGenerated;
        size_t variablesGenerated;
        size_t mathOperations;
        size_t controlStructures;
        bool usesFloatingPoint;
        bool usesStrings;
        size_t estimatedStackDepth;
    };
    
    CodeGenStats getStatistics() const;

private:
    // Code generation utilities
    void emit(const std::string& code);
    void emitLine(const std::string& line);
    void emitIndented(const std::string& line);
    void increaseIndent() { indentLevel++; }
    void decreaseIndent() { if (indentLevel > 0) indentLevel--; }
    std::string getIndent() const;
    
    // Code generation helpers
    std::string generateTempVar();
    std::string generateLabel(const std::string& prefix = "L");
    std::string sanitizeIdentifier(const std::string& name);
    std::string generateFunctionName(const std::string& wordName);
    
    // Runtime generation
    void generateRuntimeHeader();
    void generateRuntimeImplementation();
    void generateStackOperations();
    void generateMathOperations();
    void generateIOOperations();
    void generateESP32Specific();
    
    // Word code generation
    void generateWordFunction(const std::string& wordName, const std::vector<std::unique_ptr<ASTNode>>& body);
    void generateBuiltinWord(const std::string& wordName);
    
    // Expression and statement generation
    void generateExpression(ASTNode* node);
    void generateStatement(ASTNode* node);
    void generateControlFlow(ASTNode* node);
    
    // Type and conversion helpers
    std::string getForthType(ASTNode* node);
    std::string generateTypeConversion(const std::string& value, const std::string& fromType, const std::string& toType);
    
    // Helper methods (these were missing!)
    bool isBuiltinWord(const std::string& word) const;
    std::string escapeCString(const std::string& str);
    
    // Error and warning management
    void addError(const std::string& message);
    void addWarning(const std::string& message);
    void addError(const std::string& message, ASTNode* node);
    
    // Optimization helpers
    void optimizeGeneratedCode();
    void removeUnusedFunctions();
    void inlineSimpleFunctions();
};

// Utility classes for ESP-IDF integration
class ESPIDFProjectGenerator {
public:
    static bool generateProject(const std::string& projectPath, 
                               const std::string& projectName,
                               const ForthCCodegen& codegen);
    
    static std::string generateRootCMakeLists(const std::string& projectName);
    static std::string generateMainCMakeLists();
    static std::string generateComponentCMakeLists(const std::string& componentName);
    static std::string generateMainCpp(const std::string& forthProgram);
    static std::string generatePartitionTable();
    static std::string generateSDKConfig();
};

// Factory for creating code generators with different configurations
class ForthCodegenFactory {
public:
    enum class TargetType {
        ESP32_GENERIC,
        ESP32_C3,
        ESP32_S3,
        NATIVE_LINUX,
        NATIVE_WINDOWS
    };
    
    static std::unique_ptr<ForthCCodegen> create(TargetType target);
    static ForthCCodegen::ESP32Config getESP32Config(TargetType target);
};

#endif // FORTH_C_CODEGEN_H
