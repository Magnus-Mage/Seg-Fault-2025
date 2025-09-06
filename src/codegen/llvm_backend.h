#ifndef FORTH_LLVM_BACKEND_H
#define FORTH_LLVM_BACKEND_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "parser/ast.h"
#include "semantic/analyzer.h"
#include "dictionary/dictionary.h"

// Forward declarations for LLVM (to avoid including heavy headers in header file)
namespace llvm {
    class LLVMContext;
    class Module;
    class Function;
    class Value;
    class Type;
    class BasicBlock;
    class TargetMachine;
    
    // Template forward declaration for IRBuilder
    template<typename T = void>
    class IRBuilder;
    
    // Enum forward declarations
    enum class BinaryOps;
    enum class Predicate;
}

// Custom deleters for forward-declared LLVM types
struct LLVMContextDeleter {
    void operator()(llvm::LLVMContext* ptr);
};

struct ModuleDeleter {
    void operator()(llvm::Module* ptr);
};

struct IRBuilderDeleter {
    void operator()(llvm::IRBuilder<>* ptr);
};

struct TargetMachineDeleter {
    void operator()(llvm::TargetMachine* ptr);
};

// LLVM code generation for FORTH
class ForthLLVMCodegen : public ASTVisitor {
private:
    // LLVM core objects with custom deleters
    std::unique_ptr<llvm::LLVMContext, LLVMContextDeleter> context;
    std::unique_ptr<llvm::Module, ModuleDeleter> module;
    std::unique_ptr<llvm::IRBuilder<>, IRBuilderDeleter> builder;
    
    // Target configuration
    std::unique_ptr<llvm::TargetMachine, TargetMachineDeleter> targetMachine;
    std::string targetTriple;
    
    // FORTH runtime state
    llvm::Value* stackPointer;          // Current stack pointer
    llvm::Value* stackBase;             // Base of data stack
    llvm::Value* returnStackPointer;    // Return stack pointer
    llvm::Value* returnStackBase;       // Base of return stack
    
    llvm::Type* cellType;               // FORTH cell type (i32)
    llvm::Type* stackType;              // Array type for stacks
    
    // Function management
    llvm::Function* currentFunction;
    std::unordered_map<std::string, llvm::Function*> wordFunctions;
    std::unordered_map<std::string, llvm::Value*> variables;
    std::unordered_map<std::string, llvm::Value*> constants;
    
    // Control flow management
    std::vector<llvm::BasicBlock*> breakTargets;
    std::vector<llvm::BasicBlock*> continueTargets;
    
    // Analysis context
    const SemanticAnalyzer* analyzer;
    const ForthDictionary* dictionary;
    
    // Code generation state
    bool inWordDefinition;
    std::string currentWordName;
    std::vector<std::string> errors;
    
public:
    explicit ForthLLVMCodegen(const std::string& moduleName = "forth_module");
    ~ForthLLVMCodegen();
    
    // Move-only type (prevent copying due to unique_ptr with custom deleters)
    ForthLLVMCodegen(const ForthLLVMCodegen&) = delete;
    ForthLLVMCodegen& operator=(const ForthLLVMCodegen&) = delete;
    ForthLLVMCodegen(ForthLLVMCodegen&&) = default;
    ForthLLVMCodegen& operator=(ForthLLVMCodegen&&) = default;
    
    // Configuration
    auto setTarget(const std::string& triple) -> void;
    auto setSemanticAnalyzer(const SemanticAnalyzer* sa) -> void { analyzer = sa; }
    auto setDictionary(const ForthDictionary* dict) -> void { dictionary = dict; }
    
    // Main code generation interface
    auto generateModule(ProgramNode& program) -> std::unique_ptr<llvm::Module, ModuleDeleter>;
    auto generateFunction(const std::string& name, WordDefinitionNode& definition) -> llvm::Function*;
    
    // LLVM module management
    auto getModule() -> llvm::Module* { return module.get(); }
    auto releaseModule() -> std::unique_ptr<llvm::Module, ModuleDeleter> { return std::move(module); }
    
    // Error handling
    [[nodiscard]] auto hasErrors() const -> bool { return !errors.empty(); }
    [[nodiscard]] auto getErrors() const -> const std::vector<std::string>& { return errors; }
    auto clearErrors() -> void { errors.clear(); }
    
    // Output generation - Fixed const correctness
    auto emitLLVMIR(const std::string& filename = "") const -> std::string;
    auto emitAssembly(const std::string& filename = "") const -> std::string;
    auto emitObjectFile(const std::string& filename) const -> bool;
    
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
    // LLVM initialization
    auto initializeLLVM() -> void;
    auto initializeTarget() -> void;
    auto createForthRuntime() -> void;
    
    // Stack operations
    auto generateStackPush(llvm::Value* value) -> void;
    auto generateStackPop() -> llvm::Value*;
    auto generateStackDup() -> void;
    auto generateStackSwap() -> void;
    auto generateStackDrop() -> void;
    
    // Arithmetic operations - Fixed parameter types
    auto generateBinaryOp(int binaryOp) -> void;  // Use int instead of enum
    auto generateUnaryOp(const std::string& operation) -> void;
    auto generateComparison(int predicate) -> void;  // Use int instead of enum
    
    // Control flow generation
    auto generateIf(IfStatementNode& node) -> void;
    auto generateBeginUntil(BeginUntilLoopNode& node) -> void;
    
    // Function generation
    auto createWordFunction(const std::string& name) -> llvm::Function*;
    auto generateWordCall(const std::string& wordName) -> void;
    auto generateBuiltinCall(const std::string& wordName) -> void;
    
    // Memory operations
    auto generateLoad(llvm::Value* address) -> llvm::Value*;
    auto generateStore(llvm::Value* address, llvm::Value* value) -> void;
    
    // String operations
    auto createStringConstant(const std::string& str) -> llvm::Value*;
    auto generatePrintString(const std::string& str) -> void;
    
    // Variable/constant handling
    auto generateVariableDeclaration(const std::string& name) -> void;
    auto generateConstantDeclaration(const std::string& name, llvm::Value* value) -> void;
    
    // Utility functions
    auto createBasicBlock(const std::string& name, llvm::Function* func = nullptr) -> llvm::BasicBlock*;
    auto addError(const std::string& message) -> void;
    auto addError(const std::string& message, ASTNode& node) -> void;
    
    // Built-in word implementations
    auto initializeBuiltinWords() -> void;
    auto generateBuiltinMathOp(const std::string& op) -> void;
    auto generateBuiltinStackOp(const std::string& op) -> void;
    auto generateBuiltinIOOp(const std::string& op) -> void;
    
    // ESP32-specific optimizations
    auto optimizeForESP32() -> void;
    auto generateESP32SpecificCode() -> void;
};

// LLVM utilities
namespace LLVMUtils {
    // Target configuration helpers
    [[nodiscard]] auto getXtensaTargetTriple() -> std::string;
    [[nodiscard]] auto createXtensaTargetMachine() -> std::unique_ptr<llvm::TargetMachine, TargetMachineDeleter>;
    
    // FORTH type helpers
    [[nodiscard]] auto getForthCellType(llvm::LLVMContext& context) -> llvm::Type*;
    [[nodiscard]] auto getForthStackType(llvm::LLVMContext& context, size_t size = 256) -> llvm::Type*;
    
    // Code optimization
    auto optimizeModule(llvm::Module* module, llvm::TargetMachine* target) -> void;
    auto addESP32Attributes(llvm::Function* func) -> void;
}

// Code generation configuration
struct CodegenConfig {
    std::string targetTriple;
    size_t stackSize;
    size_t returnStackSize;
    bool generateDebugInfo;
    bool optimizeForSize;
    bool enableVectorization;
    
    CodegenConfig() 
        : stackSize(256)
        , returnStackSize(256)
        , generateDebugInfo(false)
        , optimizeForSize(true)
        , enableVectorization(false) {}
};

// High-level compiler interface
class ForthCompiler {
private:
    std::unique_ptr<SemanticAnalyzer> analyzer;
    std::unique_ptr<ForthLLVMCodegen> codegen;
    std::unique_ptr<ForthDictionary> dictionary;
    CodegenConfig config;
    std::vector<std::string> errors;
    
public:
    ForthCompiler();
    explicit ForthCompiler(const CodegenConfig& cfg);
    ~ForthCompiler();
    
    // Move-only type
    ForthCompiler(const ForthCompiler&) = delete;
    ForthCompiler& operator=(const ForthCompiler&) = delete;
    ForthCompiler(ForthCompiler&&) = default;
    ForthCompiler& operator=(ForthCompiler&&) = default;
    
    // Main compilation interface
    auto compile(const std::string& forthCode) -> std::unique_ptr<llvm::Module, ModuleDeleter>;
    auto compileToFile(const std::string& forthCode, const std::string& outputFile) -> bool;
    auto compileToObjectFile(const std::string& forthCode, const std::string& objectFile) -> bool;
    
    // Configuration
    auto setTarget(const std::string& target) -> void;
    auto setConfig(const CodegenConfig& cfg) -> void { config = cfg; }
    
    // Analysis access
    auto getAnalyzer() -> SemanticAnalyzer* { return analyzer.get(); }
    auto getDictionary() -> ForthDictionary* { return dictionary.get(); }
    
    // Error handling
    [[nodiscard]] auto hasErrors() const -> bool;
    [[nodiscard]] auto getAllErrors() const -> std::vector<std::string>;
    auto clearErrors() -> void;
    
    // Utility methods
    auto analyzeStackEffects(const std::string& forthCode) -> bool;
    auto generateLLVMIR(const std::string& forthCode) -> std::string;
};

#endif // FORTH_LLVM_BACKEND_H
