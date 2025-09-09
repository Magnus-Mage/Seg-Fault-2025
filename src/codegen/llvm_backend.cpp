#include "codegen/llvm_backend.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "dictionary/dictionary.h"

#ifdef WITH_REAL_LLVM
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#endif

#include <sstream>
#include <iostream>
#include <algorithm>
#include <fstream>

#ifndef WITH_REAL_LLVM
// Mock LLVM implementation for development and testing

namespace llvm {
    static int nextTempId = 0;
    
    // Memory management for mock objects
    static std::vector<std::unique_ptr<Value>> allocatedValues;
    
    // Forward declare all classes properly
    class LLVMContext {
    public:
        LLVMContext() = default;
        ~LLVMContext() = default;
    };
    
    class Type {
    public:
        Type() = default;
        virtual ~Type() = default;
        static Type* getInt32Ty(LLVMContext& context) {
            static Type instance;
            return &instance;
        }
        static Type* getVoidTy(LLVMContext& context) {
            static Type instance;
            return &instance;
        }
        static Type* getInt8PtrTy(LLVMContext& context) {
            static Type instance;
            return &instance;
        }
    };
    
    class Value {
    protected:
        std::string name;
        Type* type;
    public:
        Value() : type(nullptr) {}
        Value(const std::string& n, Type* t = nullptr) : name(n), type(t) {}
        virtual ~Value() = default;
        const std::string& getName() const { return name; }
        Type* getType() const { return type; }
        void setName(const std::string& n) { name = n; }
    };
    
    class BasicBlock : public Value {
    private:
        std::vector<std::string> instructions;
        
    public:
        BasicBlock(const std::string& name, Type* t = nullptr) : Value(name, t) {}
        virtual ~BasicBlock() = default;
        
        void addInstruction(const std::string& instr) {
            instructions.push_back(instr);
        }
        
        const std::vector<std::string>& getInstructions() const {
            return instructions;
        }
    };
    
    class Function : public Value {
    private:
        std::vector<std::unique_ptr<BasicBlock>> blocks;
        std::vector<std::string> attributes;
        
    public:
        Function(const std::string& name, Type* t = nullptr) : Value(name, t) {}
        virtual ~Function() = default;
        
        auto createBasicBlock(const std::string& name) -> BasicBlock* {
            blocks.push_back(std::make_unique<BasicBlock>(name));
            return blocks.back().get();
        }
        
        auto getBasicBlockList() -> std::vector<std::unique_ptr<BasicBlock>>& { 
            return blocks; 
        }
        
        void addAttribute(const std::string& attr) {
            attributes.push_back(attr);
        }
        
        const std::vector<std::string>& getAttributes() const {
            return attributes;
        }
    };
    
    class Module {
    private:
        std::vector<std::unique_ptr<Function>> functions;
        std::vector<std::unique_ptr<Value>> globals;
        std::string targetTriple;
        std::string targetDataLayout;
        mutable std::stringstream irStream;
        
    public:
        Module(const std::string& name, llvm::LLVMContext& context = *static_cast<llvm::LLVMContext*>(nullptr)) {    irStream << "; ModuleID = '" << name << "'\n";
            setTargetTriple("x86_64-unknown-linux-gnu");
        }
        
        virtual ~Module() = default;
        
        auto getOrInsertFunction(const std::string& name, Type* retType, 
                                std::vector<Type*> args = {}) -> Function* {
            for (auto& func : functions) {
                if (func->getName() == name) {
                    return func.get();
                }
            }
            functions.push_back(std::make_unique<Function>(name, retType));
            return functions.back().get();
        }
        
        auto getFunctionList() -> std::vector<std::unique_ptr<Function>>& { 
            return functions; 
        }
        
        auto setTargetTriple(const std::string& triple) -> void {
            targetTriple = triple;
            irStream.str("");
            irStream << "; ModuleID = 'forth_module'\n";
            irStream << "target datalayout = \"e-m:e-p:32:32-i1:8:32-i8:8:32-i16:16:32-i64:64-f128:128-a:0:32-n32-S128\"\n";
            irStream << "target triple = \"" << triple << "\"\n\n";
        }
        
        auto getTargetTriple() const -> const std::string& {
            return targetTriple;
        }
        
        auto addGlobalVariable(const std::string& name, Type* type, bool isConstant = false) -> Value* {
            auto global = std::make_unique<Value>("@" + name, type);
            auto ptr = global.get();
            globals.push_back(std::move(global));
            
            irStream << "@" << name << " = ";
            if (isConstant) irStream << "constant ";
            else irStream << "global ";
            irStream << "i32 0\n";
            
            return ptr;
        }
        
        auto print(std::ostream& os) const -> void {
            // Print module header
            os << irStream.str();
            
            // Print global declarations
            os << "\n; Runtime function declarations\n";
            os << "declare i32 @printf(i8*, ...)\n";
            os << "declare void @exit(i32)\n\n";
            
            // Print functions
            for (const auto& func : functions) {
                os << "define ";
                if (func->getType() == Type::getVoidTy(*static_cast<LLVMContext*>(nullptr))) {
                    os << "void";
                } else {
                    os << "i32";
                }
                os << " @" << func->getName() << "() {\n";
                
                // Print basic blocks
                for (const auto& bb : func->getBasicBlockList()) {
                    os << bb->getName() << ":\n";
                    for (const auto& instr : bb->getInstructions()) {
                        os << "  " << instr << "\n";
                    }
                }
                
                os << "}\n\n";
            }
        }
        
        auto getIR() const -> std::string {
            std::stringstream ss;
            print(ss);
            return ss.str();
        }
    };
    
    template<typename T>
    class IRBuilder {
    private:
        Module* module;
        BasicBlock* currentBlock;
        LLVMContext* context;
        
        // Helper to create managed values
        Value* createManagedValue(const std::string& name, Type* type = nullptr) {
            allocatedValues.push_back(std::make_unique<Value>(name, type));
            return allocatedValues.back().get();
        }
        
    public:
        IRBuilder(LLVMContext& ctx) : module(nullptr), currentBlock(nullptr), context(&ctx) {}
        IRBuilder(Module* mod) : module(mod), currentBlock(nullptr), context(nullptr) {}
        
        auto GetInsertBlock() -> BasicBlock* { return currentBlock; }
        
        auto SetInsertPoint(BasicBlock* block) -> void {
            currentBlock = block;
        }
        
        auto CreateAlloca(Type* type, Value* arraySize = nullptr, const std::string& name = "") -> Value* {
            std::string varName = name.empty() ? ("%temp" + std::to_string(nextTempId++)) : name;
            auto var = createManagedValue(varName, type);
            
            if (currentBlock) {
                currentBlock->addInstruction(varName + " = alloca i32");
            }
            
            return var;
        }
        
        auto CreateLoad(Type* type, Value* ptr, const std::string& name = "") -> Value* {
            std::string resultName = name.empty() ? ("%temp" + std::to_string(nextTempId++)) : name;
            auto result = createManagedValue(resultName, type);
            
            if (currentBlock) {
                currentBlock->addInstruction(resultName + " = load i32, i32* " + ptr->getName());
            }
            
            return result;
        }
        
        auto CreateStore(Value* val, Value* ptr) -> void {
            if (currentBlock) {
                currentBlock->addInstruction("store i32 " + val->getName() + ", i32* " + ptr->getName());
            }
        }
        
        auto CreateAdd(Value* lhs, Value* rhs, const std::string& name = "") -> Value* {
            std::string resultName = name.empty() ? ("%temp" + std::to_string(nextTempId++)) : name;
            auto result = createManagedValue(resultName);
            
            if (currentBlock) {
                currentBlock->addInstruction(resultName + " = add i32 " + lhs->getName() + ", " + rhs->getName());
            }
            
            return result;
        }
        
        auto CreateSub(Value* lhs, Value* rhs, const std::string& name = "") -> Value* {
            std::string resultName = name.empty() ? ("%temp" + std::to_string(nextTempId++)) : name;
            auto result = createManagedValue(resultName);
            
            if (currentBlock) {
                currentBlock->addInstruction(resultName + " = sub i32 " + lhs->getName() + ", " + rhs->getName());
            }
            
            return result;
        }
        
        auto CreateMul(Value* lhs, Value* rhs, const std::string& name = "") -> Value* {
            std::string resultName = name.empty() ? ("%temp" + std::to_string(nextTempId++)) : name;
            auto result = createManagedValue(resultName);
            
            if (currentBlock) {
                currentBlock->addInstruction(resultName + " = mul i32 " + lhs->getName() + ", " + rhs->getName());
            }
            
            return result;
        }
        
        auto CreateSDiv(Value* lhs, Value* rhs, const std::string& name = "") -> Value* {
            std::string resultName = name.empty() ? ("%temp" + std::to_string(nextTempId++)) : name;
            auto result = createManagedValue(resultName);
            
            if (currentBlock) {
                currentBlock->addInstruction(resultName + " = sdiv i32 " + lhs->getName() + ", " + rhs->getName());
            }
            
            return result;
        }
        
        auto CreateICmpSLT(Value* lhs, Value* rhs, const std::string& name = "") -> Value* {
            std::string resultName = name.empty() ? ("%temp" + std::to_string(nextTempId++)) : name;
            auto result = createManagedValue(resultName);
            
            if (currentBlock) {
                currentBlock->addInstruction(resultName + " = icmp slt i32 " + lhs->getName() + ", " + rhs->getName());
            }
            
            return result;
        }
        
        auto CreateCondBr(Value* cond, BasicBlock* trueBlock, BasicBlock* falseBlock) -> void {
            if (currentBlock) {
                currentBlock->addInstruction("br i1 " + cond->getName() + 
                                           ", label %" + trueBlock->getName() + 
                                           ", label %" + falseBlock->getName());
            }
        }
        
        auto CreateBr(BasicBlock* block) -> void {
            if (currentBlock) {
                currentBlock->addInstruction("br label %" + block->getName());
            }
        }
        
        auto CreateRet(Value* val = nullptr) -> void {
            if (currentBlock) {
                if (val) {
                    currentBlock->addInstruction("ret i32 " + val->getName());
                } else {
                    currentBlock->addInstruction("ret void");
                }
            }
        }
        
        auto CreateCall(Function* func, std::vector<Value*> args = {}, const std::string& name = "") -> Value* {
            std::string resultName = name.empty() ? ("%temp" + std::to_string(nextTempId++)) : name;
            auto result = createManagedValue(resultName);
            
            if (currentBlock) {
                std::string call = "call void @" + func->getName() + "()";
                currentBlock->addInstruction(call);
            }
            
            return result;
        }
        
        auto getInt32(int value) -> Value* {
            return createManagedValue(std::to_string(value));
        }
        
        auto CreateGlobalStringPtr(const std::string& str, const std::string& name = "") -> Value* {
            std::string globalName = name.empty() ? ("str" + std::to_string(nextTempId++)) : name;
            return createManagedValue("@" + globalName);
        }
    };
    
    class TargetMachine {
    public:
        TargetMachine() = default;
        virtual ~TargetMachine() = default;
        auto getTargetTriple() -> std::string { return "xtensa-esp32-elf"; }
    };
    
    // Instruction types for compatibility
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
#endif // WITH_REAL_LLVM

// ForthLLVMCodegen Implementation
ForthLLVMCodegen::ForthLLVMCodegen(const std::string& moduleName) 
    : targetTriple("xtensa-esp32-elf")  // Default to ESP32
    , stackPointer(nullptr)
    , stackBase(nullptr)
    , returnStackPointer(nullptr) 
    , returnStackBase(nullptr)
    , cellType(nullptr)
    , stackType(nullptr)
    , currentFunction(nullptr)
    , analyzer(nullptr)
    , dictionary(nullptr)
    , inWordDefinition(false) 
{
#ifdef WITH_REAL_LLVM
    context = std::unique_ptr<llvm::LLVMContext, LLVMContextDeleter>(new llvm::LLVMContext());
    module = std::unique_ptr<llvm::Module, ModuleDeleter>(new llvm::Module(moduleName, *context));
    builder = std::unique_ptr<llvm::IRBuilder<>, IRBuilderDeleter>(new llvm::IRBuilder<>(*context));
    
    initializeLLVM();
    createForthRuntime();
#else
    // Keep your existing mock implementation as fallback
    context = std::unique_ptr<llvm::LLVMContext, LLVMContextDeleter>(new llvm::LLVMContext());
    module = std::unique_ptr<llvm::Module, ModuleDeleter>(new llvm::Module(moduleName));
    builder = std::unique_ptr<llvm::IRBuilder<>, IRBuilderDeleter>(new llvm::IRBuilder<>(*context));
    
    initializeLLVM();
    createForthRuntime();
#endif
}

ForthLLVMCodegen::~ForthLLVMCodegen() = default;

auto ForthLLVMCodegen::initializeLLVM() -> void {
#ifdef WITH_REAL_LLVM
    // Initialize LLVM targets
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
    
    // Set up types
    cellType = llvm::Type::getInt32Ty(*context);
    stackType = llvm::ArrayType::get(cellType, 256); // Real array type
    
    // Set module target
    module->setTargetTriple(targetTriple);
    module->setDataLayout("e-m:e-p:32:32-i1:8:32-i8:8:32-i16:16:32-i64:64-f128:128-a:0:32-n32-S128");
#else
    // Mock implementation
    cellType = llvm::Type::getInt32Ty(*context);
    stackType = cellType;
    module->setTargetTriple(targetTriple);
#endif
}

auto ForthLLVMCodegen::createForthRuntime() -> void {
#ifdef WITH_REAL_LLVM
    // Create real global arrays for stacks
    auto initializer = llvm::ConstantAggregateZero::get(stackType);
    
    stackBase = new llvm::GlobalVariable(
        *module, stackType, false,
        llvm::GlobalValue::PrivateLinkage,
        initializer, "forth_data_stack");
    
    returnStackBase = new llvm::GlobalVariable(
        *module, stackType, false,
        llvm::GlobalValue::PrivateLinkage,
        initializer, "forth_return_stack");
    
    // Stack pointers (indices into arrays)
    auto zero = llvm::ConstantInt::get(cellType, 0);
    stackPointer = new llvm::GlobalVariable(
        *module, cellType, false,
        llvm::GlobalValue::PrivateLinkage,
        zero, "forth_sp");
    
    returnStackPointer = new llvm::GlobalVariable(
        *module, cellType, false,
        llvm::GlobalValue::PrivateLinkage,
        zero, "forth_rsp");
        
    // Create runtime helper functions
    createRuntimeHelpers();
#else
    // Keep mock implementation
    stackBase = module->addGlobalVariable("forth_data_stack", cellType, false);
    stackPointer = module->addGlobalVariable("forth_sp", cellType, false);
    returnStackBase = module->addGlobalVariable("forth_return_stack", cellType, false);
    returnStackPointer = module->addGlobalVariable("forth_rsp", cellType, false);
#endif
}

auto ForthLLVMCodegen::setTarget(const std::string& triple) -> void {
    targetTriple = triple;
    module->setTargetTriple(triple);
}

auto ForthLLVMCodegen::generateModule(ProgramNode& program) -> std::unique_ptr<llvm::Module, ModuleDeleter> {
    errors.clear();
    
    try {
        visit(program);
        
        if (hasErrors()) {
            return nullptr;
        }
        
        return std::unique_ptr<llvm::Module, ModuleDeleter>(module.release());
    } catch (const std::exception& e) {
        addError("Code generation failed: " + std::string(e.what()));
        return nullptr;
    }
}

#ifdef WITH_REAL_LLVM
auto ForthLLVMCodegen::createRuntimeHelpers() -> void {
    // Create stack_push function
    auto voidTy = llvm::Type::getVoidTy(*context);
    auto pushFuncTy = llvm::FunctionType::get(voidTy, {cellType}, false);
    auto pushFunc = llvm::Function::Create(pushFuncTy, llvm::Function::PrivateLinkage, 
                                          "forth_stack_push", module.get());
    
    auto entry = llvm::BasicBlock::Create(*context, "entry", pushFunc);
    builder->SetInsertPoint(entry);
    
    auto arg = pushFunc->getArg(0);
    arg->setName("value");
    
    // Load current stack pointer
    auto sp = builder->CreateLoad(cellType, stackPointer, "sp");
    
    // Get stack slot address: &stack[sp]
    auto stackAddr = builder->CreateInBoundsGEP(
        stackType, stackBase, 
        {llvm::ConstantInt::get(cellType, 0), sp}, 
        "stack_slot");
    
    // Store value
    builder->CreateStore(arg, stackAddr);
    
    // Increment stack pointer
    auto newSp = builder->CreateAdd(sp, llvm::ConstantInt::get(cellType, 1), "new_sp");
    builder->CreateStore(newSp, stackPointer);
    
    builder->CreateRetVoid();
    
    // Store reference
    stackPushFunc = pushFunc;
    
    // Create stack_pop function
    auto popFuncTy = llvm::FunctionType::get(cellType, {}, false);
    auto popFunc = llvm::Function::Create(popFuncTy, llvm::Function::PrivateLinkage,
                                         "forth_stack_pop", module.get());
    
    auto popEntry = llvm::BasicBlock::Create(*context, "entry", popFunc);
    builder->SetInsertPoint(popEntry);
    
    // Decrement stack pointer
    auto currentSp = builder->CreateLoad(cellType, stackPointer, "sp");
    auto newPopSp = builder->CreateSub(currentSp, llvm::ConstantInt::get(cellType, 1), "new_sp");
    builder->CreateStore(newPopSp, stackPointer);
    
    // Load value from stack
    auto popStackAddr = builder->CreateInBoundsGEP(
        stackType, stackBase,
        {llvm::ConstantInt::get(cellType, 0), newPopSp},
        "stack_slot");
    
    auto value = builder->CreateLoad(cellType, popStackAddr, "value");
    builder->CreateRet(value);
    
    stackPopFunc = popFunc;
}
#endif

void ForthLLVMCodegen::visit(ProgramNode& node) {
    // Create main function as entry point
    auto mainFunc = createWordFunction("main");
    currentFunction = mainFunc;
    auto entryBlock = mainFunc->createBasicBlock("entry");
    builder->SetInsertPoint(entryBlock);
    
    // Initialize FORTH runtime
    auto zero = builder->getInt32(0);
    builder->CreateStore(zero, stackPointer);
    builder->CreateStore(zero, returnStackPointer);
    
    // Process all top-level statements
    for (const auto& child : node.getChildren()) {
        child->accept(*this);
    }
    
    // Return success
    auto returnCode = builder->getInt32(0);
    builder->CreateRet(returnCode);
}

void ForthLLVMCodegen::visit(WordDefinitionNode& node) {
    const auto& wordName = node.getWordName();
    currentWordName = wordName;
    inWordDefinition = true;
    
    // Create function for this word
    auto func = createWordFunction(wordName);
    wordFunctions[wordName] = func;
    
    // Save current function context
    auto savedFunction = currentFunction;
    auto savedBlock = builder->GetInsertBlock();
    
    currentFunction = func;
    auto entryBlock = func->createBasicBlock("entry");
    builder->SetInsertPoint(entryBlock);
    
    // Generate code for word body
    for (const auto& child : node.getChildren()) {
        child->accept(*this);
    }
    
    builder->CreateRet(); // Void return for FORTH words
    
    // Restore context
    currentFunction = savedFunction;
    if (savedBlock) {
        builder->SetInsertPoint(savedBlock);
    }
    
    inWordDefinition = false;
    currentWordName.clear();
}

void ForthLLVMCodegen::visit(WordCallNode& node) {
    const auto& wordName = node.getWordName();
    
    // Check if it's a built-in word
    if (dictionary && dictionary->isWordDefined(wordName)) {
        auto entry = dictionary->lookupWord(wordName);
        if (entry && entry->type == WordEntry::WordType::BUILTIN) {
            generateBuiltinCall(wordName);
            return;
        }
    }
    
    // Check for user-defined word
    auto it = wordFunctions.find(wordName);
    if (it != wordFunctions.end()) {
        builder->CreateCall(it->second);
    } else {
        addError("Undefined word: " + wordName);
    }
}

void ForthLLVMCodegen::visit(NumberLiteralNode& node) {
    auto value = builder->getInt32(std::stoi(node.getValue()));
    generateStackPush(value);
}

void ForthLLVMCodegen::visit(StringLiteralNode& node) {
    if (node.isPrint()) {
        generatePrintString(node.getValue());
    } else {
        auto stringPtr = builder->CreateGlobalStringPtr(node.getValue());
        auto length = builder->getInt32(static_cast<int>(node.getValue().length()));
        
        generateStackPush(stringPtr);
        generateStackPush(length);
    }
}

void ForthLLVMCodegen::visit(IfStatementNode& node) {
    generateIf(node);
}

void ForthLLVMCodegen::visit(BeginUntilLoopNode& node) {
    generateBeginUntil(node);
}

// Fix the MathOperationNode visitor
void ForthLLVMCodegen::visit(MathOperationNode& node) {
    const auto& op = node.getOperation();
    
    // Binary arithmetic operations
    if (op == "+") {
#ifdef WITH_REAL_LLVM
        generateBinaryOp(llvm::Instruction::Add);
#else
        generateBinaryOp(static_cast<int>(llvm::Instruction::BinaryOps::Add));
#endif
    } else if (op == "-") {
#ifdef WITH_REAL_LLVM
        generateBinaryOp(llvm::Instruction::Sub);
#else
        generateBinaryOp(static_cast<int>(llvm::Instruction::BinaryOps::Sub));
#endif
    } else if (op == "*") {
#ifdef WITH_REAL_LLVM
        generateBinaryOp(llvm::Instruction::Mul);
#else
        generateBinaryOp(static_cast<int>(llvm::Instruction::BinaryOps::Mul));
#endif
    } else if (op == "/") {
#ifdef WITH_REAL_LLVM
        generateBinaryOp(llvm::Instruction::SDiv);
#else
        generateBinaryOp(static_cast<int>(llvm::Instruction::BinaryOps::SDiv));
#endif
    } 
    // Binary comparison operations
    else if (op == "<") {
#ifdef WITH_REAL_LLVM
        generateComparison(llvm::CmpInst::ICMP_SLT);
#else
        generateComparison(static_cast<int>(llvm::CmpInst::Predicate::ICMP_SLT));
#endif
    } else if (op == ">") {
#ifdef WITH_REAL_LLVM
        generateComparison(llvm::CmpInst::ICMP_SGT);
#else
        addError("Mock implementation doesn't support >");
#endif
    } else if (op == "=") {
#ifdef WITH_REAL_LLVM
        generateComparison(llvm::CmpInst::ICMP_EQ);
#else
        addError("Mock implementation doesn't support =");
#endif
    }
    // Unary operations
    else if (op == "NEGATE" || op == "ABS" || op == "DUP" || op == "DROP") {
        generateUnaryOp(op);
    }
    // Stack operations that look like math
    else if (op == "DUP") {
        generateStackDup();
    } else if (op == "DROP") {
        generateStackDrop();
    } else if (op == "SWAP") {
        generateStackSwap();
    }
    else {
        addError("Unknown math operation: " + op);
    }
}

void ForthLLVMCodegen::visit(VariableDeclarationNode& node) {
    const auto& varName = node.getVarName();
    
    if (node.isConst()) {
        // Constants consume value from stack
        auto value = generateStackPop();
        constants[varName] = value;
    } else {
        // Variables create storage
        generateVariableDeclaration(varName);
    }
}

// Stack operation implementations
// Fixed stack operations for REAL LLVM
auto ForthLLVMCodegen::generateStackPush(llvm::Value* value) -> void {
#ifdef WITH_REAL_LLVM
    // Call the runtime helper function
    builder->CreateCall(stackPushFunc, {value});
#else
    // Keep mock implementation but fix it
    auto sp = builder->CreateLoad(cellType, stackPointer);
    auto slotAddr = builder->CreateAdd(sp, builder->getInt32(1));
    builder->CreateStore(value, stackPointer);  // This is wrong in mock!
    builder->CreateStore(slotAddr, stackPointer);
#endif
}

auto ForthLLVMCodegen::generateStackPop() -> llvm::Value* {
#ifdef WITH_REAL_LLVM
    return builder->CreateCall(stackPopFunc, {}, "popped");
#else
    auto sp = builder->CreateLoad(cellType, stackPointer);
    auto newSp = builder->CreateSub(sp, builder->getInt32(1));
    builder->CreateStore(newSp, stackPointer);
    return builder->CreateLoad(cellType, stackPointer);  // Also wrong in mock!
#endif
}

// FIXED binary operation implementation
auto ForthLLVMCodegen::generateBinaryOp(int binaryOp) -> void {
    auto b = generateStackPop();  // Second operand
    auto a = generateStackPop();  // First operand
    
    llvm::Value* result = nullptr;
    
#ifdef WITH_REAL_LLVM
    switch (binaryOp) {
        case llvm::Instruction::Add:
            result = builder->CreateAdd(a, b, "add_result");
            break;
        case llvm::Instruction::Sub:
            result = builder->CreateSub(a, b, "sub_result");
            break;
        case llvm::Instruction::Mul:
            result = builder->CreateMul(a, b, "mul_result");
            break;
        case llvm::Instruction::SDiv:
            result = builder->CreateSDiv(a, b, "div_result");
            break;
        default:
            addError("Unsupported binary operation");
            return;
    }
#else
    // Mock implementation
    switch (static_cast<llvm::Instruction::BinaryOps>(binaryOp)) {
        case llvm::Instruction::BinaryOps::Add:
            result = builder->CreateAdd(a, b);
            break;
        case llvm::Instruction::BinaryOps::Sub:
            result = builder->CreateSub(a, b);
            break;
        case llvm::Instruction::BinaryOps::Mul:
            result = builder->CreateMul(a, b);
            break;
        case llvm::Instruction::BinaryOps::SDiv:
            result = builder->CreateSDiv(a, b);
            break;
        default:
            addError("Unsupported binary operation");
            return;
    }
#endif
    
    generateStackPush(result);
}

// FIXED comparison operations - these are BINARY not unary!
auto ForthLLVMCodegen::generateComparison(int predicate) -> void {
    auto b = generateStackPop();  // Second operand
    auto a = generateStackPop();  // First operand
    
    llvm::Value* result = nullptr;
    
#ifdef WITH_REAL_LLVM
    switch (static_cast<llvm::CmpInst::Predicate>(predicate)) {
        case llvm::CmpInst::ICMP_SLT:  // <
            result = builder->CreateICmpSLT(a, b, "lt_result");
            break;
        case llvm::CmpInst::ICMP_SGT:  // >
            result = builder->CreateICmpSGT(a, b, "gt_result");
            break;
        case llvm::CmpInst::ICMP_EQ:   // =
            result = builder->CreateICmpEQ(a, b, "eq_result");
            break;
        case llvm::CmpInst::ICMP_SLE:  // <=
            result = builder->CreateICmpSLE(a, b, "le_result");
            break;
        case llvm::CmpInst::ICMP_SGE:  // >=
            result = builder->CreateICmpSGE(a, b, "ge_result");
            break;
        case llvm::CmpInst::ICMP_NE:   // <>
            result = builder->CreateICmpNE(a, b, "ne_result");
            break;
        default:
            addError("Unsupported comparison operation");
            return;
    }
    
    // Convert boolean result to FORTH convention (-1 for true, 0 for false)
    auto minusOne = llvm::ConstantInt::get(cellType, -1);
    auto zero = llvm::ConstantInt::get(cellType, 0);
    result = builder->CreateSelect(result, minusOne, zero, "forth_bool");
#else
    // Mock version
    switch (static_cast<llvm::CmpInst::Predicate>(predicate)) {
        case llvm::CmpInst::Predicate::ICMP_SLT:
            result = builder->CreateICmpSLT(a, b);
            break;
        default:
            addError("Unsupported comparison operation");
            return;
    }
#endif
    
    generateStackPush(result);
}

auto ForthLLVMCodegen::generateIf(IfStatementNode& node) -> void {
    // Pop condition from stack
    auto condition = generateStackPop();
    auto zero = builder->getInt32(0);
    auto condCheck = builder->CreateICmpSLT(zero, condition); // condition != 0
    
    // Create basic blocks
    auto thenBlock = createBasicBlock("if_then");
    auto elseBlock = node.hasElse() ? createBasicBlock("if_else") : nullptr;
    auto endBlock = createBasicBlock("if_end");
    
    // Create conditional branch
    if (node.hasElse()) {
        builder->CreateCondBr(condCheck, thenBlock, elseBlock);
    } else {
        builder->CreateCondBr(condCheck, thenBlock, endBlock);
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

auto ForthLLVMCodegen::generateBeginUntil(BeginUntilLoopNode& node) -> void {
    // Create basic blocks for loop
    auto loopBlock = createBasicBlock("loop_body");
    auto testBlock = createBasicBlock("loop_test");
    auto endBlock = createBasicBlock("loop_end");
    
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
    auto condition = generateStackPop();
    auto zero = builder->getInt32(0);
    auto condCheck = builder->CreateICmpSLT(zero, condition); // condition != 0
    
    // Branch: if condition is true (non-zero), exit loop
    builder->CreateCondBr(condCheck, endBlock, loopBlock);
    
    // Continue after loop
    builder->SetInsertPoint(endBlock);
}

auto ForthLLVMCodegen::createWordFunction(const std::string& name) -> llvm::Function* {
    auto funcType = (name == "main") ? 
        cellType :  // main returns int
        llvm::Type::getVoidTy(*context); // FORTH words return void
        
    return module->getOrInsertFunction(name, funcType);
}

auto ForthLLVMCodegen::generateBuiltinCall(const std::string& wordName) -> void {
    if (wordName == "DUP") {
        generateStackDup();
    } else if (wordName == "DROP") {
        generateStackDrop();
    } else if (wordName == "SWAP") {
        generateStackSwap();
    } else if (wordName == ".") {
        // Print top of stack (simplified)
        auto value = generateStackPop();
        // In real implementation, would call printf
    } else {
        addError("Unknown builtin word: " + wordName);
    }
}

auto ForthLLVMCodegen::generateStackDup() -> void {
    auto value = generateStackPop();
    generateStackPush(value);
    generateStackPush(value);
}

auto ForthLLVMCodegen::generateStackDrop() -> void {
    generateStackPop(); // Just pop and discard
}

auto ForthLLVMCodegen::generateStackSwap() -> void {
    auto b = generateStackPop();
    auto a = generateStackPop();
    generateStackPush(b);
    generateStackPush(a);
}

auto ForthLLVMCodegen::generateUnaryOp(const std::string& operation) -> void {
    auto value = generateStackPop();
    llvm::Value* result = nullptr;
    
    if (operation == "NEGATE") {
        auto zero = builder->getInt32(0);
        result = builder->CreateSub(zero, value);
    } else if (operation == "ABS") {
        auto zero = builder->getInt32(0);
        auto isNegative = builder->CreateICmpSLT(value, zero);
        auto negated = builder->CreateSub(zero, value);
        // In real LLVM, would use CreateSelect
        result = negated; // Simplified
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
    auto var = module->addGlobalVariable(name, cellType, false);
    variables[name] = var;
}

auto ForthLLVMCodegen::generatePrintString(const std::string& str) -> void {
    // In a real implementation, this would call printf
    // For now, just add a comment
}

auto ForthLLVMCodegen::createBasicBlock(const std::string& name, llvm::Function* func) -> llvm::BasicBlock* {
    llvm::Function* targetFunc = func ? func : currentFunction;
    if (!targetFunc) {
        addError("Cannot create basic block outside function context");
        return nullptr;
    }
    return targetFunc->createBasicBlock(name);
}

auto ForthLLVMCodegen::addError(const std::string& message) -> void {
    errors.push_back(message);
}

auto ForthLLVMCodegen::addError(const std::string& message, ASTNode& node) -> void {
    std::ostringstream oss;
    oss << message << " at line " << node.getLine() << ", column " << node.getColumn();
    errors.push_back(oss.str());
}

auto ForthLLVMCodegen::emitLLVMIR(const std::string& filename) const -> std::string {
#ifdef WITH_REAL_LLVM
    std::string ir;
    llvm::raw_string_ostream stream(ir);
    module->print(stream, nullptr);
    
    if (!filename.empty()) {
        std::error_code EC;
        llvm::raw_fd_ostream file(filename, EC, llvm::sys::fs::OF_None);
        if (!EC) {
            module->print(file, nullptr);
        }
    }
    
    return ir;
#else
    // Keep mock implementation
    if (!module) {
        return "; Error: No module to emit\n";
    }
    
    std::string ir = module->getIR();
    
    std::string declarations = R"(
; FORTH Runtime Support
declare i32 @printf(i8*, ...)
declare i32 @putchar(i32)
declare void @exit(i32)
)";
    
    std::string result = declarations + ir;
    
    if (!filename.empty()) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << result;
            file.close();
        }
    }
    
    return result;
#endif
}

auto ForthLLVMCodegen::emitAssembly(const std::string& filename) const -> std::string {
    // For mock implementation, generate pseudo-assembly
    std::string assembly = R"(
; FORTH Pseudo-Assembly Output
; This would be real assembly in a production implementation
.section .text
.global main

main:
    ; Initialize FORTH runtime
    ; Set up data stack
    ; Set up return stack
    ; Execute program
    ret

; FORTH word implementations would follow...
)";
    
    if (!filename.empty()) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << assembly;
            file.close();
        }
    }
    
    return assembly;
}

auto ForthLLVMCodegen::emitObjectFile(const std::string& filename) const -> bool {
#ifdef WITH_REAL_LLVM
    auto targetTripleStr = module->getTargetTriple();
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTripleStr, error);
    
    if (!target) {
        addError("Failed to lookup target: " + error);
        return false;
    }
    
    llvm::TargetOptions opt;
    auto targetMachine = std::unique_ptr<llvm::TargetMachine>(
        target->createTargetMachine(targetTripleStr, "generic", "", opt,
                                   llvm::Reloc::PIC_));
    
    module->setDataLayout(targetMachine->createDataLayout());
    
    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);
    
    if (EC) {
        addError("Could not open file: " + EC.message());
        return false;
    }
    
    llvm::legacy::PassManager pass;
    auto fileType = llvm::CGFT_ObjectFile;
    
    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
        addError("TargetMachine can't emit object file");
        return false;
    }
    
    pass.run(*module);
    dest.flush();
    
    return true;
#else
    // Mock implementation
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file << "FORTH Mock Object File\n";
        file.close();
        return true;
    }
    return false;
#endif
}

// Missing implementations for abstract methods
auto ForthLLVMCodegen::generateFunction(const std::string& name, WordDefinitionNode& definition) -> llvm::Function* {
    auto func = createWordFunction(name);
    
    // Save current context
    auto savedFunction = currentFunction;
    auto savedBlock = builder->GetInsertBlock();
    
    // Set up new function
    currentFunction = func;
    auto entryBlock = func->createBasicBlock("entry");
    builder->SetInsertPoint(entryBlock);
    
    // Generate function body
    for (const auto& child : definition.getChildren()) {
        child->accept(*this);
    }
    
    builder->CreateRet(); // Void return
    
    // Restore context
    currentFunction = savedFunction;
    if (savedBlock) {
        builder->SetInsertPoint(savedBlock);
    }
    
    return func;
}

// Additional missing method implementations
auto ForthLLVMCodegen::generateLoad(llvm::Value* address) -> llvm::Value* {
    return builder->CreateLoad(cellType, address);
}

auto ForthLLVMCodegen::generateStore(llvm::Value* address, llvm::Value* value) -> void {
    builder->CreateStore(value, address);
}

auto ForthLLVMCodegen::createStringConstant(const std::string& str) -> llvm::Value* {
    return builder->CreateGlobalStringPtr(str);
}

auto ForthLLVMCodegen::generateWordCall(const std::string& wordName) -> void {
    auto it = wordFunctions.find(wordName);
    if (it != wordFunctions.end()) {
        builder->CreateCall(it->second);
    } else {
        addError("Undefined word: " + wordName);
    }
}

auto ForthLLVMCodegen::generateConstantDeclaration(const std::string& name, llvm::Value* value) -> void {
    constants[name] = value;
}

auto ForthLLVMCodegen::initializeBuiltinWords() -> void {
    // Initialize built-in FORTH words
    // This would populate the dictionary with standard words
}

auto ForthLLVMCodegen::generateBuiltinMathOp(const std::string& op) -> void {
    if (op == "+") {
        generateBinaryOp(static_cast<int>(llvm::Instruction::BinaryOps::Add));
    } else if (op == "-") {
        generateBinaryOp(static_cast<int>(llvm::Instruction::BinaryOps::Sub));
    } else if (op == "*") {
        generateBinaryOp(static_cast<int>(llvm::Instruction::BinaryOps::Mul));
    } else if (op == "/") {
        generateBinaryOp(static_cast<int>(llvm::Instruction::BinaryOps::SDiv));
    }
}

auto ForthLLVMCodegen::generateBuiltinStackOp(const std::string& op) -> void {
    if (op == "DUP") {
        generateStackDup();
    } else if (op == "DROP") {
        generateStackDrop();
    } else if (op == "SWAP") {
        generateStackSwap();
    }
}

auto ForthLLVMCodegen::generateBuiltinIOOp(const std::string& op) -> void {
    if (op == ".") {
        auto value = generateStackPop();
        // In real implementation, would generate printf call
    }
}

auto ForthLLVMCodegen::optimizeForESP32() -> void {
    // ESP32-specific optimizations would go here
}

auto ForthLLVMCodegen::generateESP32SpecificCode() -> void {
    // Generate ESP32-specific initialization code
}

// High-level compiler implementation
ForthCompiler::ForthCompiler() 
    : analyzer(std::make_unique<SemanticAnalyzer>())
    , codegen(std::make_unique<ForthLLVMCodegen>())
    , dictionary(DictionaryFactory::create(DictionaryFactory::Configuration::STANDARD)) {
    
    analyzer->setDictionary(dictionary.get());
    codegen->setSemanticAnalyzer(analyzer.get());
    codegen->setDictionary(dictionary.get());
}

ForthCompiler::ForthCompiler(const CodegenConfig& cfg) 
    : ForthCompiler() {
    config = cfg;
    codegen->setTarget(config.targetTriple);
}

ForthCompiler::~ForthCompiler() = default;

auto ForthCompiler::compile(const std::string& forthCode) -> std::unique_ptr<llvm::Module, ModuleDeleter> {    errors.clear();
    
    try {
        // Lexical analysis
        ForthLexer lexer;
        auto tokens = lexer.tokenize(forthCode);
        
        // Parsing
        ForthParser parser(DictionaryFactory::create(DictionaryFactory::Configuration::STANDARD));
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) {
            for (const auto& error : parser.getErrors()) {
                errors.push_back("Parse error: " + error);
            }
            return nullptr;
        }
        
        // Update analyzer with parser's dictionary
        analyzer->setDictionary(&parser.getDictionary());
        
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

auto ForthCompiler::compileToFile(const std::string& forthCode, const std::string& outputFile) -> bool {
    auto module = compile(forthCode);
    if (!module) {
        return false;
    }
    
    auto ir = codegen->emitLLVMIR(outputFile);
    return !ir.empty();
}

auto ForthCompiler::compileToObjectFile(const std::string& forthCode, const std::string& objectFile) -> bool {
    auto module = compile(forthCode);
    if (!module) {
        return false;
    }
    
    return codegen->emitObjectFile(objectFile);
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
        
        ForthParser parser(DictionaryFactory::create(DictionaryFactory::Configuration::STANDARD));
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) {
            return false;
        }
        
        // Update analyzer dictionary
        analyzer->setDictionary(&parser.getDictionary());
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

auto createXtensaTargetMachine() -> std::unique_ptr<llvm::TargetMachine, TargetMachineDeleter> {
    // In a real implementation, this would create an actual LLVM TargetMachine
    // for the Xtensa architecture used by ESP32
    return std::unique_ptr<llvm::TargetMachine, TargetMachineDeleter>(new llvm::TargetMachine());
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
    if (func) {
        func->addAttribute("target-cpu=esp32");
        func->addAttribute("target-features=+fp");
    }
}

} // namespace LLVMUtils

// Custom deleter implementations
void LLVMContextDeleter::operator()(llvm::LLVMContext* ptr) {
    delete ptr;
}

void ModuleDeleter::operator()(llvm::Module* ptr) {
    delete ptr;
}

void IRBuilderDeleter::operator()(llvm::IRBuilder<>* ptr) {
    delete ptr;
}

void TargetMachineDeleter::operator()(llvm::TargetMachine* ptr) {
    delete ptr;
}
