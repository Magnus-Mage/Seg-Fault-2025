#ifndef FORTH_C_CODEGEN_H
#define FORTH_C_CODEGEN_H

#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>
#include <utility>
#include "parser/ast.h"
#include "semantic/analyzer.h"
#include "dictionary/dictionary.h"

// Forward declarations
class SemanticAnalyzer;
class ForthDictionary;

// ============================================================================
// Main Code Generator Class
// ============================================================================

class ForthCCodegen : public ASTVisitor {
public:
    // ========================================================================
    // Configuration Structures
    // ========================================================================
    
    // ESP32 hardware configuration
    struct ESP32Config {
        bool useTasking;      // Use FreeRTOS tasks
        bool useGPIO;         // Enable GPIO support
        bool useWiFi;         // Include WiFi stack
        bool useTimer;        // Hardware timer support
        bool useIRAM;         // Place critical code in IRAM
        bool useDMA;          // Enable DMA operations
        size_t stackSize;     // Stack size in cells
        int priority;         // Task priority
        int cpuFreq;          // CPU frequency in MHz
        int flashFreq;        // Flash frequency in MHz
        
        ESP32Config() : useTasking(true), useGPIO(true), useWiFi(false),
                       useTimer(true), useIRAM(true), useDMA(false),
                       stackSize(1024), priority(5), cpuFreq(240), flashFreq(80) {}
    };
    
    // Optimization configuration
    struct OptimizationFlags {
        bool useIRAM;         // Place hot code in IRAM
        bool canInline;       // Allow function inlining
        bool smallStack;      // Optimize for small stack
        bool needsFloat;      // Requires floating point
        bool ioHeavy;         // I/O intensive program
        
        OptimizationFlags() : useIRAM(false), canInline(false), 
                              smallStack(false), needsFloat(false), 
                              ioHeavy(false) {}
    };
    
    // Code generation statistics
    struct CodeGenStats {
        size_t linesGenerated;
        size_t functionsGenerated;
        size_t variablesGenerated;
        size_t filesGenerated;
        size_t optimizationsApplied;
        bool usesFloatingPoint;
        bool usesStrings;
        size_t estimatedStackDepth;
        size_t iramUsage;
        size_t flashUsage;
    };

    // ========================================================================
    // Constructor and Configuration
    // ========================================================================
    
    explicit ForthCCodegen(const std::string& name = "forth_program");
    ~ForthCCodegen() = default;
    
    // Configuration setters
    void setTarget(const std::string& target) { targetPlatform = target; }
    void setSemanticAnalyzer(const SemanticAnalyzer* analyzer) { 
        semanticAnalyzer = analyzer; 
    }
    void setDictionary(const ForthDictionary* dict) { 
        dictionary = dict; 
    }
    void setESP32Config(const ESP32Config& config) { 
        esp32Config = config; 
    }
    void setOptimizationLevel(int level);
    
    // ========================================================================
    // Main Code Generation Interface
    // ========================================================================
    
    bool generateCode(const ProgramNode& program);
    bool writeToFiles(const std::string& outputDir);
    
    // Get generated content
    const std::vector<std::pair<std::string, std::ostringstream>>& 
        getGeneratedFiles() const { return generatedFiles; }
    
    // ========================================================================
    // Error Handling
    // ========================================================================
    
    bool hasErrors() const { return !errors.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }
    const std::vector<std::string>& getWarnings() const { return warnings; }
    void clearErrors() { errors.clear(); warnings.clear(); }
    
    // ========================================================================
    // Statistics and Analysis
    // ========================================================================
    
    CodeGenStats getStatistics() const;
    const std::set<std::string>& getUsedFeatures() const { return usedFeatures; }
    const std::set<std::string>& getUsedBuiltins() const { return usedBuiltins; }
    
    // ========================================================================
    // ASTVisitor Implementation - Only existing node types
    // ========================================================================
    
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
    // ========================================================================
    // Code Generation State
    // ========================================================================
    
    // Module information
    std::string moduleName;
    std::string targetPlatform;
    
    // Generation context
    int indentLevel;
    int tempVarCounter;
    int labelCounter;
    int stringCounter;
    size_t currentFileIndex;
    
    // External dependencies
    const SemanticAnalyzer* semanticAnalyzer;
    const ForthDictionary* dictionary;
    
    // Configuration
    ESP32Config esp32Config;
    OptimizationFlags optimizationFlags;
    
    // ========================================================================
    // Generated Code Tracking
    // ========================================================================
    
    // Output files (filename, content)
    std::vector<std::pair<std::string, std::ostringstream>> generatedFiles;
    
    // Legacy streams (for compatibility - to be removed)
    std::ostringstream headerStream;
    std::ostringstream sourceStream;
    std::ostringstream functionsStream;
    
    // Tracking maps
    std::unordered_set<std::string> generatedWords;
    std::unordered_map<std::string, std::string> wordFunctionNames;
    std::unordered_map<std::string, std::string> variableMap;
    
    // Feature detection
    std::set<std::string> usedFeatures;
    std::set<std::string> usedBuiltins;
    
    // Call graph for optimization
    std::map<std::string, std::set<std::string>> callGraph;
    
    // Optimization tracking
    std::set<std::string> forwardReferences;
    std::set<std::string> inlineCandidates;
    std::set<std::string> iramFunctions;
    std::set<std::string> unusedWords;
    
    // Error tracking
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    // ========================================================================
    // Program Analysis Methods
    // ========================================================================
    
    void analyzeProgram(const ProgramNode& program);
    void determineOptimizationStrategy();
    bool isPerformanceCritical(const std::string& wordName) const;
    bool isBuiltinWord(const std::string& word) const;
    
    // ========================================================================
    // Runtime Generation Methods
    // ========================================================================
    
    void generateModularRuntime();
    std::string generateCoreRuntimeHeader();
    std::string generateStackImplementation();
    std::string generateMathImplementation();
    std::string generateCompareImplementation();
    std::string generateMemoryImplementation();
    std::string generateIOImplementation();
    std::string generateESP32Implementation();
    
    // ========================================================================
    // Code Generation Utilities
    // ========================================================================
    
    // Output helpers
    void resetGenerationState();
    void generateFile(const std::string& filename, const std::string& content);
    void emit(const std::string& code);
    void emitLine(const std::string& line = "");
    void emitIndented(const std::string& line);
    std::string getIndent() const;
    void increaseIndent() { indentLevel++; }
    void decreaseIndent() { if (indentLevel > 0) indentLevel--; }
    
    // Identifier generation
    std::string generateTempVar();
    std::string generateLabel(const std::string& prefix = "L");
    std::string sanitizeIdentifier(const std::string& name);
    std::string generateFunctionName(const std::string& wordName);
    std::string escapeCString(const std::string& str);
    
    // ========================================================================
    // Optimization Methods
    // ========================================================================
    
    void applyOptimizations();
    void generateOptimizedBuiltin(const std::string& word);
    void generateOptimizedIf(const IfStatementNode& node);
    void generateOptimizedCountedLoop(const BeginUntilLoopNode& node);
    void generateInlineAssemblyBuiltin(const std::string& word);
    
    bool isSimpleCondition(const IfStatementNode& node) const;
    bool isCountedLoop(const BeginUntilLoopNode& node) const;
    bool useInlineAssembly(const std::string& word) const;
    
    void inlineSmallFunctions();
    void optimizeStackUsage();
    void applyESP32Optimizations();
    void removeUnusedFunctions();
    
    // ========================================================================
    // Finalization Methods
    // ========================================================================
    
    void finalizeGeneration();
    void generateCMakeLists();
    void generateESP32Main();
    void resolveForwardReferences();
    
    // ========================================================================
    // Error Management
    // ========================================================================
    
    void addError(const std::string& message);
    void addWarning(const std::string& message);
    void addError(const std::string& message, ASTNode* node);
    
    // ========================================================================
    // Utility Methods
    // ========================================================================
    
    std::string getOptimizationLevel() const;
};

// ============================================================================
// ESP-IDF Project Generator
// ============================================================================

class ESPIDFProjectGenerator {
public:
    struct ProjectConfig {
        std::string projectName;
        std::string projectPath;
        std::string targetChip;     // esp32, esp32c3, esp32s3
        bool includeWiFi;
        bool includeBluetooth;
        bool includeDisplay;
        int flashSize;               // in MB
        int psramSize;              // in MB, 0 if none
    };
    
    static bool generateProject(const ProjectConfig& config,
                               const ForthCCodegen& codegen);
    
private:
    static std::string generateRootCMakeLists(const ProjectConfig& config);
    static std::string generateMainCMakeLists();
    static std::string generateComponentCMakeLists(const std::string& componentName);
    static std::string generateKConfig(const ProjectConfig& config);
    static std::string generatePartitionTable(const ProjectConfig& config);
    static std::string generateSDKConfig(const ProjectConfig& config);
};

// ============================================================================
// Code Generator Factory
// ============================================================================

class ForthCodegenFactory {
public:
    enum class TargetType {
        ESP32,          // Original ESP32
        ESP32_C3,       // RISC-V based
        ESP32_S3,       // With AI acceleration
        ESP32_C6,       // WiFi 6 support
        ESP32_H2,       // 802.15.4 support
        NATIVE_LINUX,   // Linux native
        NATIVE_WINDOWS, // Windows native
        NATIVE_MACOS    // macOS native
    };
    
    struct TargetCapabilities {
        bool hasWiFi;
        bool hasBluetooth;
        bool hasUSB;
        bool hasCAN;
        bool hasEthernet;
        bool hasCamera;
        bool hasPSRAM;
        int maxGPIO;
        int adcChannels;
        int dacChannels;
        int touchChannels;
        std::string architecture;  // xtensa, riscv
    };
    
    static std::unique_ptr<ForthCCodegen> create(TargetType target);
    static ForthCCodegen::ESP32Config getESP32Config(TargetType target);
    static TargetCapabilities getTargetCapabilities(TargetType target);
    
private:
    static void configureForTarget(ForthCCodegen& codegen, TargetType target);
};

// ============================================================================
// Utility Functions
// ============================================================================

namespace ForthCodegenUtils {
    // File I/O helpers
    bool writeFile(const std::string& filepath, const std::string& content);
    bool createDirectory(const std::string& path);
    
    // Code formatting
    std::string formatCode(const std::string& code, const std::string& style = "llvm");
    std::string addHeaderComment(const std::string& code, const std::string& description);
    
    // Validation
    bool validateGeneratedCode(const std::string& code);
    std::vector<std::string> findUndefinedSymbols(const std::string& code);
}

#endif // FORTH_C_CODEGEN_H
