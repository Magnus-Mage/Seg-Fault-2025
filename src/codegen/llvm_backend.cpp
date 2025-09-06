// We'll use simplified LLVM-like IR generation for now
// In a real implementation, this would include full LLVM headers
// For testing purposes, we'll create a mock implementation

#include "codegen/llvm_backend.h"
#include <sstream>
#include <iostream>
#include <algorithm>

// Mock LLVM classes for testing
namespace llvm {
    class LLVMContext {
    public:
        LLVMContext() = default;
    };
    
    class Type {
    public:
        static Type* getInt32Ty(LLVMContext& context) {
            static Type instance;
            return &instance;
        }
        static Type* getVoidTy(LLVMContext& context) {
            static Type instance;
            return &instance;
        }
    };
    
    class Value {
    protected:
        std::string name;
    public:
        Value() = default;
        Value(const std::string& n) : name(n) {}
        virtual ~Value() = default;
        const std::string& getName() const { return name; }
    };
    
    class BasicBlock : public Value {
    public:
        BasicBlock(const std::string& name) : Value(name) {}
    };
    
    class Function : public Value {
    private:
        std::vector<std::unique_ptr<BasicBlock>> blocks;
    public:
        Function(const std::string& name) : Value(name) {}
        auto createBasicBlock(const std::string& name) -> BasicBlock* {
            blocks.push_back(std::make_unique<BasicBlock>(name));
            return blocks.back().get();
        }
        auto getFunctionList() -> std::vector<std::unique_ptr<BasicBlock>>& { return blocks; }
    };
    
    class Module {
    private:
        std::vector<std::unique_ptr<Function>> functions;
        std::vector<std::unique_ptr<Value>> globals;
        std::string targetTriple;
        std::stringstream irStream;
        
    public:
        Module(const std::string& name) {
            irStream << "; ModuleID = '" << name << "'\n";
            irStream << "target datalayout = \"e-m:e-p:32:32-i1:8:32-i8:8:32-i16:16:32-i64:64-f128:128-a:0:32-n32-S128\"\n";
        }
        
        auto getOrInsertFunction(const std::string& name, Type* retType) -> Function* {
            for (auto& func : functions) {
                if (func->getName() == name) {
                    return func.get();
                }
            }
            functions.push_back(std::make_unique<Function>(name));
            return functions.back().get();
        }
        
        auto getFunctionList() -> std::vector<std::unique_ptr<Function>>& { return functions; }
        
        auto setTargetTriple(const std::string& triple) -> void {
            targetTriple = triple;
            irStream << "target triple = \"" << triple << "\"\n\n";
        }
        
        auto addGlobalVariable(const std::string& name, Type* type) -> void {
            globals.push_back(std::make_unique<Value>(name));
            irStream << "@" << name << " = global i32 0\n";
        }
        
        auto getIR() -> std::string {
            return irStream.str();
        }
        
        auto addToIR(const std::string& code) -> void {
            irStream << code;
        }
    };
    
    template<typename T = void>
    class IRBuilder {
    private:
        Module* module;
        BasicBlock* currentBlock;
        std::stringstream& irStream;
        int tempCounter = 0;
        
    public:
        IRBuilder(LLVMContext& context) : module(nullptr), currentBlock(nullptr), irStream(*(new std::stringstream())) {}
        IRBuilder(Module* mod) : module(mod), currentBlock(nullptr), irStream(*(new std::stringstream())) {}
        
        auto SetInsertPoint(BasicBlock* block) -> void {
            currentBlock = block;
        }
        
        auto CreateAlloca(Type* type, const std::string& name = "") -> Value* {
            auto var = std::make_unique<Value>(name.empty() ? ("%" + std::to_string(tempCounter++)) : name);
            if (module) {
                module->addToIR("  " + var->getName() + " = alloca i32\n");
            }
            return var.release();
        }
        
        auto CreateLoad(Type* type, Value* ptr) -> Value* {
            auto result = std::make_unique<Value>("%" + std::to_string(tempCounter++));
            if (module) {
                module->addToIR("  " + result->getName() + " = load i32, i32* " + ptr->getName() + "\n");
            }
            return result.release();
        }
        
        auto CreateStore(Value* val, Value* ptr) -> void {
            if (module) {
                module->addToIR("  store i32 " + val->getName() + ", i32* " + ptr->getName() + "\n");
            }
        }
        
        auto CreateAdd(Value* lhs, Value* rhs) -> Value* {
            auto result = std::make_unique<Value>("%" + std::to_string(tempCounter++));
            if (module) {
                module->addToIR("  " + result->getName() + " = add i32 " + lhs->getName() + ", " + rhs->getName() + "\n");
            }
            return result.release();
        }
        
        auto CreateSub(Value* lhs, Value* rhs) -> Value* {
            auto result = std::make_unique<Value>("%" + std::to_string(tempCounter++));
            if (module) {
                module->addToIR("  " + result->getName() + " = sub i32 " + lhs->getName() + ", " + rhs->getName() + "\n");
            }
            return result.release();
        }
        
        auto CreateMul(Value* lhs, Value* rhs) -> Value* {
            auto result = std::make_unique<Value>("%" + std::to_string(tempCounter++));
            if (module) {
                module->addToIR("  " + result->getName() + " = mul i32 " + lhs->getName() + ", " + rhs->getName() + "\n");
            }
            return result.release();
        }
        
        auto CreateICmpSLT(Value* lhs, Value* rhs) -> Value* {
            auto result = std::make_unique<Value>("%" + std::to_string(tempCounter++));
            if (module) {
                module->addToIR("  " + result->getName() + " = icmp slt i32 " + lhs->getName() + ", " + rhs->getName() + "\n");
            }
            return result.release();
        }
        
        auto CreateCondBr(Value* cond, BasicBlock* trueBlock, BasicBlock* falseBlock) -> void {
            if (module) {
                module->addToIR("  br i1 " + cond->getName() + ", label %" + trueBlock->getName() + 
                              ", label %" + falseBlock->getName() + "\n");
            }
        }
        
        auto CreateBr(BasicBlock* block) -> void {
            if (module) {
                module->addToIR("  br label %" + block->getName() + "\n");
            }
        }
        
        auto CreateRet(Value* val) -> void {
            if (module) {
                if (val) {
                    module->addToIR("  ret i32 " + val->getName() + "\n");
                } else {
                    module->addToIR("  ret void\n");
                }
            }
        }
        
        auto getInt32(int value) -> Value* {
            return new Value(std::to_string(value));
        }
    };
    
    class TargetMachine {
    public:
        TargetMachine() = default;
    };
    
    enum class BinaryOps {
        Add, Sub, Mul, UDiv, SDiv
    };
    
    enum class Predicate {
        ICMP_SLT, ICMP_SGT, ICMP_EQ
    };
    
    namespace Instruction {
        using BinaryOps = llvm::BinaryOps;
    }
    
    namespace CmpInst {
        using Predicate = llvm::Predicate;
    }
}

ForthLLVMCodegen::ForthLLVMCodegen(const std::string& moduleName) 
    : context(std::make_unique<llvm::LLVMContext>())
    , module(std::make_unique<llvm::Module>(moduleName))
    , builder(std::make_unique<llvm::IRBuilder<>>(module.get()))
    , targetTriple("x86_64-unknown-linux-gnu")
    , analyzer(nullptr)
    , dictionary(nullptr)
    , inWordDefinition(false) {
    
    initializeLLVM();
    createForthRuntime();
}

ForthLLVMCodegen::~ForthLLVMCodegen() = default;

auto ForthLLVMCodegen::initializeLLVM() -> void {
    // Initialize LLVM (mock implementation)
    cellType = llvm::Type::getInt32Ty(*context);
    
    // Set up basic FORTH runtime
    stackBase = builder->CreateAlloca(cellType, "stack_base");
    stackPointer = builder->CreateAlloca(cellType, "stack_pointer");
    returnStackBase = builder->CreateAlloca(cellType, "return_stack_base");
    returnStackPointer = builder->CreateAlloca(cellType, "return_stack_pointer");
}

auto ForthLLVMCodegen::createForthRuntime() -> void {
    // Create global stack arrays
    module->addGlobalVariable("forth_stack", cellType);
    module->addGlobalVariable("forth_return_stack", cellType);
    module->addGlobalVariable("forth_sp", cellType);
    module->addGlobalVariable("forth_rsp", cellType);
    
    module->addToIR("\n; FORTH Runtime globals\n");
    module->addToIR("@forth_stack = global [256 x i32] zeroinitializer\n");
    module->addToIR("@forth_return_stack = global [256 x i32] zeroinitializer\n");
    module->addToIR("@forth_sp = global i32 0\n");
    module->addToIR("@forth_rsp = global i32 0\n\n");
}

auto ForthLLVMCodegen::setTarget(const std::string& triple) -> void {
    targetTriple = triple;
    module->setTargetTriple(triple);
}

auto ForthLLVMCodegen::generateModule(ProgramNode& program) -> std::unique_ptr<llvm::Module> {
    errors.clear();
    
    visit(program);
    
    if (hasErrors()) {
        return nullptr;
    }
    
    return std::move(module);
}

void ForthLLVMCodegen::visit(ProgramNode& node) {
    module->addToIR("; Generated FORTH program\n\n");
    
    for (const auto& child : node.getChildren()) {
        child->accept(*this);
    }
    
    // Create main function if not in word definition mode
    if (!inWordDefinition) {
        auto mainFunc = module->getOrInsertFunction("main", llvm::Type::getInt32Ty(*context));
        auto entryBlock = mainFunc->createBasicBlock("entry");
        builder->SetInsertPoint(entryBlock);
        
        module->addToIR("define i32 @main() {\n");
        module->addToIR("entry:\n");
        
        // Initialize stack pointer
        module->addToIR("  store i32 0, i32* @forth_sp\n");
        
        // Process all statements
        for (const auto& child : node.getChildren()) {
            if (auto wordDef = dynamic_cast<WordDefinitionNode*>(child.get())) {
                // Word definitions are handled separately
                continue;
            } else {
                child->accept(*this);
            }
        }
        
        module->addToIR("  ret i32 0\n");
        module->addToIR("}\n\n");
    }
}

void ForthLLVMCodegen::visit(WordDefinitionNode& node) {
    const auto& wordName = node.getWordName();
    currentWordName = wordName;
    inWordDefinition = true;
    
    // Create function for this word
    auto func = createWordFunction(wordName);
    wordFunctions[wordName] = func;
    
    auto entryBlock = func->createBasicBlock("entry");
    builder->SetInsertPoint(entryBlock);
    
    module->addToIR("define void @" + wordName + "() {\n");
    module->addToIR("entry:\n");
    
    // Generate code for word body
    for (const auto& child : node.getChildren()) {
        child->accept(*this);
    }
    
    module->addToIR("  ret void\n");
    module->addToIR("}\n\n");
    
    inWordDefinition = false;
    currentWordName.clear();
}

void ForthLLVMCodegen::visit(WordCallNode& node) {
    const auto& wordName = node.getWordName();
    
    // Check if it's a built-in word
    if (dictionary && dictionary->lookupWord(wordName)) {
        generateBuiltinCall(wordName);
    } else {
        generateWordCall(wordName);
    }
}

void ForthLLVMCodegen::visit(NumberLiteralNode& node) {
    // Push number onto stack
    auto value = builder->getInt32(std::stoi(node.getValue()));
    generateStackPush(value);
}

void ForthLLVMCodegen::visit(StringLiteralNode& node) {
    if (node.isPrint()) {
        // Generate print string code
        generatePrintString(node.getValue());
    } else {
        // Create string constant and push address + length
        auto stringConst = createStringConstant(node.getValue());
        generateStackPush(stringConst); // Address
        auto length = builder->getInt32(static_cast<int>(node.getValue().length()));
        generateStackPush(length); // Length
    }
}

void ForthLLVMCodegen::visit(IfStatementNode& node) {
    generateIf(node);
}

void ForthLLVMCodegen::visit(BeginUntilLoopNode& node) {
    generateBeginUntil(node);
}

void ForthLLVMCodegen::visit(MathOperationNode& node) {
    const auto& op = node.getOperation();
    
    if (op == "+") {
        generateBinaryOp(llvm::Instruction::BinaryOps::Add);
    } else if (op == "-") {
        generateBinaryOp(llvm::Instruction::BinaryOps::Sub);
    } else if (op == "*") {
        generateBinaryOp(llvm::Instruction::BinaryOps::Mul);
    } else if (op == "/") {
        generateBinaryOp(llvm::Instruction::BinaryOps::SDiv);
    } else {
        generateUnaryOp(op);
    }
}

void ForthLLVMCodegen::visit(VariableDeclarationNode& node) {
    const auto& varName = node.getVarName();
    
    if (node.isConst()) {
        // Constants consume value from stack
        auto value = generateStackPop();
        constants[varName] = value;
        module->addToIR("  ; Constant " + varName + " defined\n");
    } else {
        // Variables create storage
        generateVariableDeclaration(varName);
    }
}

// Stack operation implementations
auto ForthLLVMCodegen::generateStackPush(llvm::Value* value) -> void {
    module->addToIR("  ; Stack push\n");
    module->addToIR("  %sp_val = load i32, i32* @forth_sp\n");
    module->addToIR("  %stack_ptr = getelementptr [256 x i32], [256 x i32]* @forth_stack, i32 0, i32 %sp_val\n");
    module->addToIR("  store i32 " + value->getName() + ", i32* %stack_ptr\n");
    module->addToIR("  %new_sp = add i32 %sp_val, 1\n");
    module->addToIR("  store i32 %new_sp, i32* @forth_sp\n");
}

auto ForthLLVMCodegen::generateStackPop() -> llvm::Value* {
    module->addToIR("  ; Stack pop\n");
    module->addToIR("  %sp_val = load i32, i32* @forth_sp\n");
    module->addToIR("  %prev_sp = sub i32 %sp_val, 1\n");
    module->addToIR("  store i32 %prev_sp, i32* @forth_sp\n");
    module->addToIR("  %stack_ptr = getelementptr [256 x i32], [256 x i32]* @forth_stack, i32 0, i32 %prev_sp\n");
    module->addToIR("  %pop_val = load i32, i32* %stack_ptr\n");
    
    return new llvm::Value("%pop_val");
}

auto ForthLLVMCodegen::generateBinaryOp(llvm::Instruction::BinaryOps op) -> void {
    auto b = generateStackPop();
    auto a = generateStackPop();
    
    llvm::Value* result = nullptr;
    switch (op) {
        case llvm::Instruction::BinaryOps::Add:
            result = builder->CreateAdd(a, b);
            break;
        case llvm::Instruction::BinaryOps::Sub:
            result = builder->CreateSub(a, b);
            break;
        case llvm::Instruction::BinaryOps::Mul:
            result = builder->CreateMul(a, b);
            break;
        default:
            addError("Unsupported binary operation");
            return;
    }
    
    generateStackPush(result);
}

auto ForthLLVMCodegen::generateIf(IfStatementNode& node) -> void {
    // Pop condition from stack
    auto condition = generateStackPop();
    
    // Create basic blocks
    auto func = builder->GetInsertBlock()->getParent();
    auto thenBlock = createBasicBlock("if_then", func);
    auto elseBlock = node.hasElse() ? createBasicBlock("if_else", func) : nullptr;
    auto endBlock = createBasicBlock("if_end", func);
    
    // Create conditional branch
    if (node.hasElse()) {
        builder->CreateCondBr(condition, thenBlock, elseBlock);
    } else {
        builder->CreateCondBr(condition, thenBlock, endBlock);
    }
    
    // Generate THEN branch
    builder->SetInsertPoint(thenBlock);
    if (node.getThenBranch()) {
        for (const auto& child : node.getThenBranch()->getChildren()) {
            child->accept(*this);
        }
    }
    builder->CreateBr(endBlock);
    
    // Generate ELSE branch if exists
    if (node.hasElse() && elseBlock) {
        builder->SetInsertPoint(elseBlock);
        if (node.getElseBranch()) {
            for (const auto& child : node.getElseBranch()->getChildren()) {
                child->accept(*this);
            }
        }
        builder->CreateBr(endBlock);
    }
    
    // Continue with end block
    builder->SetInsertPoint(endBlock);
}

auto ForthLLVMCodegen::createWordFunction(const std::string& name) -> llvm::Function* {
    return module->getOrInsertFunction(name, llvm::Type::getVoidTy(*context));
}

auto ForthLLVMCodegen::generateWordCall(const std::string& wordName) -> void {
    module->addToIR("  call void @" + wordName + "()\n");
}

auto ForthLLVMCodegen::generateBuiltinCall(const std::string& wordName) -> void {
    if (wordName == "DUP") {
        generateStackDup();
    } else if (wordName == "DROP") {
        generateStackDrop();
    } else if (wordName == "SWAP") {
        generateStackSwap();
    } else if (wordName == ".") {
        // Print top of stack
        auto value = generateStackPop();
        module->addToIR("  ; Print value\n");
        module->addToIR("  call i32 @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 " + 
                        value->getName() + ")\n");
    } else {
        addError("Unknown builtin word: " + wordName);
    }
}

auto ForthLLVMCodegen::generateStackDup() -> void {
    module->addToIR("  ; DUP operation\n");
    module->addToIR("  %sp_val = load i32, i32* @forth_sp\n");
    module->addToIR("  %prev_sp = sub i32 %sp_val, 1\n");
    module->addToIR("  %stack_ptr = getelementptr [256 x i32], [256 x i32]* @forth_stack, i32 0, i32 %prev_sp\n");
    module->addToIR("  %dup_val = load i32, i32* %stack_ptr\n");
    module->addToIR("  %new_stack_ptr = getelementptr [256 x i32], [256 x i32]* @forth_stack, i32 0, i32 %sp_val\n");
    module->addToIR("  store i32 %dup_val, i32* %new_stack_ptr\n");
    module->addToIR("  %new_sp = add i32 %sp_val, 1\n");
    module->addToIR("  store i32 %new_sp, i32* @forth_sp\n");
}

auto ForthLLVMCodegen::generateStackDrop() -> void {
    module->addToIR("  ; DROP operation\n");
    module->addToIR("  %sp_val = load i32, i32* @forth_sp\n");
    module->addToIR("  %new_sp = sub i32 %sp_val, 1\n");
    module->addToIR("  store i32 %new_sp, i32* @forth_sp\n");
}

auto ForthLLVMCodegen::generateStackSwap() -> void {
    module->addToIR("  ; SWAP operation\n");
    auto b = generateStackPop();
    auto a = generateStackPop();
    generateStackPush(b);
    generateStackPush(a);
}

auto ForthLLVMCodegen::generateBeginUntil(BeginUntilLoopNode& node) -> void {
    // Create basic blocks for loop
    auto func = builder->GetInsertBlock()->getParent();
    auto loopBlock = createBasicBlock("loop_body", func);
    auto testBlock = createBasicBlock("loop_test", func);
    auto endBlock = createBasicBlock("loop_end", func);
    
    // Jump to loop body
    builder->CreateBr(loopBlock);
    
    // Generate loop body
    builder->SetInsertPoint(loopBlock);
    if (node.getBody()) {
        for (const auto& child : node.getBody()->getChildren()) {
            child->accept(*this);
        }
    }
    builder->CreateBr(testBlock);
    
    // Generate test condition
    builder->SetInsertPoint(testBlock);
    auto condition = generateStackPop(); // UNTIL condition
    
    // Branch: if condition is true (non-zero), exit loop
    builder->CreateCondBr(condition, endBlock, loopBlock);
    
    // Continue after loop
    builder->SetInsertPoint(endBlock);
}

auto ForthLLVMCodegen::generateUnaryOp(const std::string& operation) -> void {
    auto value = generateStackPop();
    llvm::Value* result = nullptr;
    
    if (operation == "NEGATE") {
        module->addToIR("  %neg_result = sub i32 0, " + value->getName() + "\n");
        result = new llvm::Value("%neg_result");
    } else if (operation == "ABS") {
        module->addToIR("  %is_negative = icmp slt i32 " + value->getName() + ", 0\n");
        module->addToIR("  %negated = sub i32 0, " + value->getName() + "\n");
        module->addToIR("  %abs_result = select i1 %is_negative, i32 %negated, i32 " + value->getName() + "\n");
        result = new llvm::Value("%abs_result");
    } else if (operation == "DUP") {
        generateStackPush(value); // Push it back
        generateStackPush(value); // Duplicate
        return;
    } else {
        addError("Unknown unary operation: " + operation);
        return;
    }
    
    if (result) {
        generateStackPush(result);
    }
}

auto ForthLLVMCodegen::generateVariableDeclaration(const std::string& name) -> void {
    module->addToIR("@" + name + " = global i32 0\n");
    auto var = new llvm::Value("@" + name);
    variables[name] = var;
}

auto ForthLLVMCodegen::createStringConstant(const std::string& str) -> llvm::Value* {
    static int stringCounter = 0;
    std::string constName = ".str." + std::to_string(stringCounter++);
    
    module->addToIR("@" + constName + " = private unnamed_addr constant [" + 
                   std::to_string(str.length() + 1) + " x i8] c\"" + str + "\\00\"\n");
    
    return new llvm::Value("@" + constName);
}

auto ForthLLVMCodegen::generatePrintString(const std::string& str) -> void {
    auto stringConst = createStringConstant(str);
    module->addToIR("  ; Print string: " + str + "\n");
    module->addToIR("  call i32 @printf(i8* getelementptr inbounds ([" + 
                   std::to_string(str.length() + 1) + " x i8], [" + 
                   std::to_string(str.length() + 1) + " x i8]* " + 
                   stringConst->getName() + ", i32 0, i32 0))\n");
}

auto ForthLLVMCodegen::createBasicBlock(const std::string& name, llvm::Function* func) -> llvm::BasicBlock* {
    if (!func) {
        // In a real implementation, we'd get the current function from the builder
        return new llvm::BasicBlock(name);
    }
    return func->createBasicBlock(name);
}

auto ForthLLVMCodegen::addError(const std::string& message) -> void {
    errors.push_back(message);
}

auto ForthLLVMCodegen::addError(const std::string& message, ASTNode& node) -> void {
    std::ostringstream oss;
    oss << message << " at line " << node.getLine() << ", column " << node.getColumn();
    errors.push_back(oss.str());
}

auto ForthLLVMCodegen::emitLLVMIR(const std::string& filename) -> std::string {
    std::string ir = module->getIR();
    
    // Add necessary declarations
    std::string declarations = R"(
; External function declarations
declare i32 @printf(i8*, ...)

; String format for printing integers
@.str = private unnamed_addr constant [4 x i8] c"%d \00"

)";
    
    return declarations + ir;
}

// High-level compiler implementation
ForthCompiler::ForthCompiler() 
    : analyzer(std::make_unique<SemanticAnalyzer>())
    , codegen(std::make_unique<ForthLLVMCodegen>())
    , dictionary(DictionaryFactory::create(DictionaryFactory::Configuration::STANDARD)) {
    
    analyzer.reset(new SemanticAnalyzer(dictionary.get()));
    codegen->setSemanticAnalyzer(analyzer.get());
    codegen->setDictionary(dictionary.get());
}

ForthCompiler::ForthCompiler(const CodegenConfig& cfg) 
    : ForthCompiler() {
    config = cfg;
    codegen->setTarget(config.targetTriple);
}

ForthCompiler::~ForthCompiler() = default;

auto ForthCompiler::compile(const std::string& forthCode) -> std::unique_ptr<llvm::Module> {
    errors.clear();
    
    try {
        // Lexical analysis
        ForthLexer lexer;
        auto tokens = lexer.tokenize(forthCode);
        
        // Parsing
        ForthParser parser(std::move(dictionary));
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) {
            for (const auto& error : parser.getErrors()) {
                errors.push_back("Parse error: " + error);
            }
            return nullptr;
        }
        
        // Semantic analysis
        if (!analyzer->analyze(*ast)) {
            for (const auto& error : analyzer->getErrors()) {
                errors.push_back("Semantic error: " + error);
            }
            return nullptr;
        }
        
        // Code generation
        auto module = codegen->generateModule(*ast);
        
        if (codegen->hasErrors()) {
            for (const auto& error : codegen->getErrors()) {
                errors.push_back("Codegen error: " + error);
            }
            return nullptr;
        }
        
        return module;
        
    } catch (const std::exception& e) {
        errors.push_back("Compilation error: " + std::string(e.what()));
        return nullptr;
    }
}

auto ForthCompiler::generateLLVMIR(const std::string& forthCode) -> std::string {
    auto module = compile(forthCode);
    if (!module) {
        return ""; // Errors available via hasErrors()
    }
    
    return codegen->emitLLVMIR();
}

auto ForthCompiler::hasErrors() const -> bool {
    return !errors.empty() || 
           (analyzer && analyzer->hasErrors()) || 
           (codegen && codegen->hasErrors());
}

auto ForthCompiler::getAllErrors() const -> std::vector<std::string> {
    std::vector<std::string> allErrors = errors;
    
    if (analyzer) {
        for (const auto& error : analyzer->getErrors()) {
            allErrors.push_back("Semantic: " + error);
        }
    }
    
    if (codegen) {
        for (const auto& error : codegen->getErrors()) {
            allErrors.push_back("Codegen: " + error);
        }
    }
    
    return allErrors;
}

auto ForthCompiler::clearErrors() -> void {
    errors.clear();
    if (analyzer) analyzer->clearErrors();
    if (codegen) codegen->clearErrors();
}

auto ForthCompiler::setTarget(const std::string& target) -> void {
    config.targetTriple = target;
    if (codegen) {
        codegen->setTarget(target);
    }
}

auto ForthCompiler::analyzeStackEffects(const std::string& forthCode) -> bool {
    try {
        ForthLexer lexer;
        auto tokens = lexer.tokenize(forthCode);
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) {
            return false;
        }
        
        return analyzer->analyze(*ast);
        
    } catch (...) {
        return false;
    }
}

// LLVM Utilities namespace implementation
namespace LLVMUtils {
    
auto getXtensaTargetTriple() -> std::string {
    return "xtensa-esp32-elf";
}

auto createXtensaTargetMachine() -> std::unique_ptr<llvm::TargetMachine> {
    // In a real implementation, this would create an actual LLVM TargetMachine
    // for the Xtensa architecture used by ESP32
    return std::make_unique<llvm::TargetMachine>();
}

auto getForthCellType(llvm::LLVMContext& context) -> llvm::Type* {
    return llvm::Type::getInt32Ty(context);
}

auto getForthStackType(llvm::LLVMContext& context, size_t size) -> llvm::Type* {
    // In a real implementation, this would return an array type
    // ArrayType::get(getForthCellType(context), size)
    return llvm::Type::getInt32Ty(context); // Simplified
}

auto optimizeModule(llvm::Module* module, llvm::TargetMachine* target) -> void {
    // In a real implementation, this would run LLVM optimization passes
    // optimized for the target architecture
}

auto addESP32Attributes(llvm::Function* func) -> void {
    // In a real implementation, this would add ESP32-specific function attributes
    // for optimal code generation (calling conventions, etc.)
}

} // namespace LLVMUtils
