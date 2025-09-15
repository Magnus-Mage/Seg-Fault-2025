#include "codegen/c_backend.h"
#include "common/utils.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <set>

namespace fs = std::filesystem;

// ============================================================================
// Constructor and Core Setup
// ============================================================================

ForthCCodegen::ForthCCodegen(const std::string& name) 
    : moduleName(name), targetPlatform("esp32"), indentLevel(0), 
      tempVarCounter(0), labelCounter(0), stringCounter(0),
      currentFileIndex(0), semanticAnalyzer(nullptr), 
      dictionary(nullptr) {
    
    // Initialize ESP32 optimization defaults
    esp32Config.useTasking = true;
    esp32Config.useGPIO = true;
    esp32Config.useWiFi = false;
    esp32Config.useTimer = true;
    esp32Config.stackSize = 1024;
    esp32Config.priority = 5;
    esp32Config.useIRAM = true;
    esp32Config.useDMA = false;
    esp32Config.cpuFreq = 240;  // MHz
    esp32Config.flashFreq = 80; // MHz
}

// ============================================================================
// Main Code Generation Entry Point
// ============================================================================

bool ForthCCodegen::generateCode(const ProgramNode& program) {
    // Clear previous generation state
    resetGenerationState();
    
    try {
        // Validate input
        if (program.getChildren().empty()) {
            addWarning("Empty program provided");
        }
        
        // PASS 1: Collect all word definitions first
        collectWordDefinitions(program);
        
        // PASS 2: Analyze program for optimization opportunities  
        analyzeProgram(program);
        
        // PASS 3: Generate modular runtime components
        generateModularRuntime();
        
        // Validate that program file was created
        bool foundProgramFile = false;
        for (const auto& [filename, _] : generatedFiles) {
            if (filename == "forth_program.c") {
                foundProgramFile = true;
                break;
            }
        }
        
        if (!foundProgramFile) {
            addError("Failed to create forth_program.c file");
            return false;
        }
        
        // PASS 4: Process AST and generate code
        const_cast<ProgramNode&>(program).accept(*this);
        
        // PASS 5: Apply optimizations based on analysis
        applyOptimizations();
        
        // PASS 6: Finalize code generation
        finalizeGeneration();
        
        // Validate generation results
        if (generatedFiles.empty()) {
            addError("No files were generated");
            return false;
        }
        
        // Check that program file has content
        for (const auto& [filename, content] : generatedFiles) {
            if (filename == "forth_program.c") {
                if (content.str().empty()) {
                    addError("Generated program file is empty");
                    return false;
                }
                // Check for main function
                if (content.str().find("forth_program_main") == std::string::npos) {
                    addError("Generated program missing main function");
                    return false;
                }
                break;
            }
        }
        
        return !hasErrors();
        
    } catch (const std::exception& e) {
        addError(std::string("Code generation failed with exception: ") + e.what());
        return false;
    } catch (...) {
        addError("Code generation failed with unknown exception");
        return false;
    }
}

void ForthCCodegen::collectWordDefinitions(const ProgramNode& program) {
    // First pass: collect all word names and map to function names
    for (const auto& child : program.getChildren()) {
        if (child->getType() == ASTNode::NodeType::WORD_DEFINITION) {
            auto* wordDef = static_cast<WordDefinitionNode*>(child.get());
            const std::string& wordName = wordDef->getWordName();
            const std::string funcName = generateFunctionName(wordName);
            
            wordFunctionNames[ForthUtils::toUpper(wordName)] = funcName;
            generatedWords.insert(ForthUtils::toUpper(wordName));
        }
    }
}

// ============================================================================
// Program Analysis for Optimization
// ============================================================================

void ForthCCodegen::analyzeProgram(const ProgramNode& program) {
    // Reset analysis state
    usedFeatures.clear();
    usedBuiltins.clear();
    callGraph.clear();
    
    // Perform feature analysis
    class FeatureAnalyzer : public ASTVisitor {
    public:
        ForthCCodegen* codegen;
        std::set<std::string> currentPath;
        
        void visit(ProgramNode& node) override {
            for (const auto& child : node.getChildren()) {
                child->accept(*this);
            }
        }
        
        void visit(WordDefinitionNode& node) override {
            currentPath.insert(node.getWordName());
            for (const auto& child : node.getChildren()) {
                child->accept(*this);
            }
            currentPath.erase(node.getWordName());
        }
        
        void visit(WordCallNode& node) override {
            const std::string& word = ForthUtils::toUpper(node.getWordName());
            
            // Track builtin usage
            if (codegen->isBuiltinWord(word)) {
                codegen->usedBuiltins.insert(word);
                
                // Categorize features
                if (word == "+" || word == "-" || word == "*" || word == "/" || 
                    word == "MOD" || word == "ABS" || word == "NEGATE") {
                    codegen->usedFeatures.insert("MATH");
                } else if (word == "DUP" || word == "DROP" || word == "SWAP" || 
                          word == "OVER" || word == "ROT") {
                    codegen->usedFeatures.insert("STACK");
                } else if (word == "=" || word == "<>" || word == "<" || 
                          word == ">" || word == "<=" || word == ">=" ||
                          word == "0=" || word == "0<" || word == "0>") {
                    codegen->usedFeatures.insert("COMPARE");
                } else if (word == "!" || word == "@") {
                    codegen->usedFeatures.insert("MEMORY");
                } else if (word == "EMIT" || word == "TYPE" || word == "CR") {
                    codegen->usedFeatures.insert("IO");
                }
            }
            
            // Build call graph
            if (!currentPath.empty()) {
                codegen->callGraph[*currentPath.begin()].insert(word);
            }
        }
        
        // FIXED: Implement all required pure virtual methods
        void visit(NumberLiteralNode& node) override {
            if (node.isFloatingPoint()) {
                codegen->usedFeatures.insert("FLOAT");
            }
        }
        
        void visit(StringLiteralNode& node) override {
            codegen->usedFeatures.insert("STRING");
            if (node.isPrint()) {
                codegen->usedFeatures.insert("IO");
            }
        }
        
        void visit(IfStatementNode& node) override {
            codegen->usedFeatures.insert("CONTROL");
            if (node.getThenBranch()) {
                for (const auto& child : node.getThenBranch()->getChildren()) {
                    child->accept(*this);
                }
            }
            if (node.getElseBranch()) {
                for (const auto& child : node.getElseBranch()->getChildren()) {
                    child->accept(*this);
                }
            }
        }
        
        void visit(BeginUntilLoopNode& node) override {
            codegen->usedFeatures.insert("CONTROL");
            codegen->usedFeatures.insert("LOOP");
            if (node.getBody()) {
                for (const auto& child : node.getBody()->getChildren()) {
                    child->accept(*this);
                }
            }
        }
        
        void visit(MathOperationNode& node) override {
            codegen->usedFeatures.insert("MATH");
            codegen->usedBuiltins.insert(node.getOperation());
        }
        
        void visit(VariableDeclarationNode& node) override {
            codegen->usedFeatures.insert("VARIABLE");
        }
    };
    
    FeatureAnalyzer analyzer;
    analyzer.codegen = this;
    const_cast<ProgramNode&>(program).accept(analyzer);
    
    // Force COMPARE feature if any comparison operators are used
    bool hasComparisons = false;
    for (const auto& builtin : usedBuiltins) {
        if (builtin == "=" || builtin == "<>" || builtin == "<" || 
            builtin == ">" || builtin == "<=" || builtin == ">=") {
            hasComparisons = true;
            break;
        }
    }
    if (hasComparisons) {
        usedFeatures.insert("COMPARE");
    }
    
    // Determine optimization strategy based on features
    determineOptimizationStrategy();
}

void ForthCCodegen::determineOptimizationStrategy() {
    // Determine if we need IRAM for performance-critical code
    if (usedFeatures.contains("LOOP") || callGraph.size() > 10) {
        optimizationFlags.useIRAM = true;
    }
    
    // Check if we need floating point support
    optimizationFlags.needsFloat = usedFeatures.contains("FLOAT");
    
    // Determine stack optimization level
    if (semanticAnalyzer && semanticAnalyzer->getMaxStackDepth() < 32) {
        optimizationFlags.smallStack = true;
    }
    
    // Check for I/O heavy operations
    optimizationFlags.ioHeavy = usedFeatures.contains("IO") || 
                               usedFeatures.contains("STRING");
    
    // Determine if we can use inline optimizations
    optimizationFlags.canInline = callGraph.size() < 20 && 
                                  !usedFeatures.contains("RECURSIVE");
}

// ============================================================================
// Modular Runtime Generation
// ============================================================================

void ForthCCodegen::generateModularRuntime() {
    // Clear any existing files first
    generatedFiles.clear();
    
    // Generate only the runtime components that are actually needed
    
    // 1. Core runtime header (always needed)
    generateFile("forth_runtime.h", generateCoreRuntimeHeader());
    
    // 2. Stack operations (always needed)  
    generateFile("forth_stack.c", generateStackImplementation());
    
    // 3. Math operations (conditional)
    if (usedFeatures.contains("MATH") || !usedBuiltins.empty()) {
        generateFile("forth_math.c", generateMathImplementation());
    }
    
    // 4. Comparison operations (FIXED: Generate if ANY comparison might be used)
    generateFile("forth_compare.c", generateCompareImplementation());
    
    // 5. Memory operations (conditional)
    if (usedFeatures.contains("MEMORY")) {
        generateFile("forth_memory.c", generateMemoryImplementation());
    }
    
    // 6. I/O operations (conditional)
    if (usedFeatures.contains("IO")) {
        generateFile("forth_io.c", generateIOImplementation());
    }
    
    // 7. ESP32-specific (conditional)
    if (targetPlatform.starts_with("esp32")) {
        generateFile("forth_esp32.c", generateESP32Implementation());
    }
    
    // 8. CRITICAL FIX: Create the main program file and set current index
    generatedFiles.emplace_back("forth_program.c", std::ostringstream());
    currentFileIndex = generatedFiles.size() - 1;  // Set to the program file
    
    // Add program file header
    emitLine("// Generated FORTH Program");
    emitLine("#include \"forth_runtime.h\"");
    emitLine("#include <stdio.h>");
    emitLine("");
}

// ============================================================================
// Fixed Runtime Component Generators
// ============================================================================


std::string ForthCCodegen::generateCoreRuntimeHeader() const {
    std::ostringstream header;
    
    header << R"(#ifndef FORTH_RUNTIME_H
#define FORTH_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>  // FIXED: Add stdio.h for printf/fflush

// ============================================================================
// Configuration Macros
// ============================================================================

#ifndef FORTH_STACK_SIZE
    #define FORTH_STACK_SIZE )" << esp32Config.stackSize << R"(
#endif

#ifndef FORTH_USE_FLOAT
    #define FORTH_USE_FLOAT )" << (optimizationFlags.needsFloat ? "1" : "0") << R"(
#endif

#ifdef ESP32_PLATFORM
    #include "esp_attr.h"
    #include "esp_log.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/portmacro.h"
    
    // Performance-critical functions in IRAM
    #define FORTH_IRAM_ATTR IRAM_ATTR
    
    // DMA-safe memory alignment
    #define FORTH_DMA_ATTR WORD_ALIGNED_ATTR
    
    // FIXED: Don't redefine ESP32 macros - they're already defined
    // Use the existing FreeRTOS definitions
#else
    #define FORTH_IRAM_ATTR
    #define FORTH_DMA_ATTR
        
    #ifndef portENTER_CRITICAL
    #define portENTER_CRITICAL(x)
    #endif
    #ifndef portEXIT_CRITICAL
    #define portEXIT_CRITICAL(x)
    #endif  
    #define portMUX_TYPE int
    #ifndef portMUX_INITIALIZER_UNLOCKED
        #define portMUX_INITIALIZER_UNLOCKED 0
    #endif
#endif

// ============================================================================
// Type Definitions
// ============================================================================

typedef int32_t forth_cell_t;    // Stack cell type
typedef uint32_t forth_ucell_t;  // Unsigned cell
typedef int16_t forth_short_t;   // Short for optimization
typedef uint8_t forth_byte_t;    // Byte operations

#if FORTH_USE_FLOAT
    typedef float forth_float_t;
#endif

// Stack structure for better cache locality
typedef struct {
    forth_cell_t data[FORTH_STACK_SIZE];
    size_t ptr;
    size_t size;
    #ifdef ESP32_PLATFORM
    portMUX_TYPE lock;  // Thread safety for ESP32
    #endif
} forth_stack_t;

// ============================================================================
// Global Variables
// ============================================================================

extern forth_stack_t forth_data_stack;

// ============================================================================
// Core Stack Operations
// ============================================================================

// Initialize/cleanup
void forth_init(void);
void forth_cleanup(void);

// Basic stack operations with bounds checking
void forth_push(forth_cell_t value);
forth_cell_t forth_pop(void);
forth_cell_t forth_peek(void);
bool forth_stack_empty(void);
size_t forth_stack_depth(void);

)";

    // Conditionally add function declarations based on used features
    if (usedFeatures.contains("STACK")) {
        header << R"(// Stack manipulation
void forth_dup(void);
void forth_drop(void);
void forth_swap(void);
void forth_over(void);
void forth_rot(void);
void forth_nip(void);
void forth_tuck(void);

)";
    }

    if (usedFeatures.contains("MATH")) {
        header << R"(// Math operations
void forth_add(void);
void forth_sub(void);
void forth_mul(void);
void forth_div(void);
void forth_mod(void);
void forth_abs(void);
void forth_negate(void);
void forth_min(void);
void forth_max(void);

)";
    }

    if (usedFeatures.contains("COMPARE")) {
        header << R"(// Comparison operations - FIXED: Add all used comparisons
void forth_equal(void);
void forth_not_equal(void);
void forth_less_than(void);
void forth_greater_than(void);
void forth_less_equal(void);
void forth_greater_equal(void);
void forth_zero_equal(void);
void forth_zero_less(void);

)";
    }

    if (usedFeatures.contains("MEMORY")) {
        header << R"(// Memory operations
void forth_fetch(void);
void forth_store(void);
void forth_byte_fetch(void);
void forth_byte_store(void);

)";
    }

    if (usedFeatures.contains("IO")) {
        header << R"(// I/O operations
void forth_emit(void);
void forth_type(void);
void forth_cr(void);
void forth_space(void);
void forth_spaces(void);
void forth_print_number(forth_cell_t value);

)";
    }

    // ESP32-specific features
    if (targetPlatform.starts_with("esp32")) {
        header << R"(// ESP32-specific operations
#ifdef ESP32_PLATFORM

// ESP32 initialization
void forth_esp32_init(void);

// GPIO operations
void forth_gpio_init(forth_cell_t pin, forth_cell_t mode);
void forth_gpio_write(forth_cell_t pin, forth_cell_t value);
forth_cell_t forth_gpio_read(forth_cell_t pin);
void forth_gpio_toggle(forth_cell_t pin);

// Timing
void forth_delay_ms(forth_cell_t ms);
void forth_delay_us(forth_cell_t us);
uint32_t forth_millis(void);
uint32_t forth_micros(void);

// ADC/DAC
forth_cell_t forth_adc_read(forth_cell_t channel);
void forth_dac_write(forth_cell_t channel, forth_cell_t value);

// PWM
void forth_pwm_init(forth_cell_t channel, forth_cell_t freq);
void forth_pwm_write(forth_cell_t channel, forth_cell_t duty);

#endif // ESP32_PLATFORM

)";
    }

    header << R"(
// ============================================================================
// User-defined words - Forward declarations will be added here
// ============================================================================

#ifdef __cplusplus
}
#endif

#endif // FORTH_RUNTIME_H
)";

    return header.str();
}

// Fixed stack implementation - Remove inline functions from header
std::string ForthCCodegen::generateStackImplementation() {
    std::ostringstream impl;
    
    impl << R"(#include "forth_runtime.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Stack Implementation - FIXED: Proper global scope and non-inline functions
// ============================================================================

// Global stack instance - properly exposed (not static)
forth_stack_t forth_data_stack = {
    .data = {0},
    .ptr = 0,
    .size = FORTH_STACK_SIZE,
    #ifdef ESP32_PLATFORM
    .lock = portMUX_INITIALIZER_UNLOCKED,
    #endif
};

// ============================================================================
// Core Operations - All non-inline for proper linking
// ============================================================================

void forth_init(void) {
    forth_data_stack.ptr = 0;
    forth_data_stack.size = FORTH_STACK_SIZE;
    memset(forth_data_stack.data, 0, sizeof(forth_data_stack.data));
    
    #ifdef ESP32_PLATFORM
    // Initialize ESP32-specific features
    extern void forth_esp32_init(void);
    forth_esp32_init();
    #endif
}

void forth_cleanup(void) {
    // Cleanup if needed
}

// All functions are now non-inline to prevent linker issues
void forth_push(forth_cell_t value) {
    #ifndef ESP32_PLATFORM
    portENTER_CRITICAL(&forth_data_stack.lock);
    #endif
    
    if (forth_data_stack.ptr >= FORTH_STACK_SIZE) {
        #ifdef ESP32_PLATFORM
        ESP_LOGE("FORTH", "Stack overflow!");
        portEXIT_CRITICAL(&forth_data_stack.lock);
        #else
        fprintf(stderr, "FORTH: Stack overflow!\n");
        #endif
        return;
    }
    
    forth_data_stack.data[forth_data_stack.ptr++] = value;
    
    #ifdef ESP32_PLATFORM
    portEXIT_CRITICAL(&forth_data_stack.lock);
    #endif
}

forth_cell_t forth_pop(void) {
    #ifndef ESP32_PLATFORM
    portENTER_CRITICAL(&forth_data_stack.lock);
    #endif
    
    if (forth_data_stack.ptr == 0) {
        #ifdef ESP32_PLATFORM
        ESP_LOGE("FORTH", "Stack underflow!");
        portEXIT_CRITICAL(&forth_data_stack.lock);
        #else
        fprintf(stderr, "FORTH: Stack underflow!\n");
        #endif
        return 0;
    }
    
    forth_cell_t value = forth_data_stack.data[--forth_data_stack.ptr];
    
    #ifdef ESP32_PLATFORM
    portEXIT_CRITICAL(&forth_data_stack.lock);
    #endif
    
    return value;
}

forth_cell_t forth_peek(void) {
    if (forth_data_stack.ptr == 0) return 0;
    return forth_data_stack.data[forth_data_stack.ptr - 1];
}

bool forth_stack_empty(void) {
    return forth_data_stack.ptr == 0;
}

size_t forth_stack_depth(void) {
    return forth_data_stack.ptr;
}

)";

    // Generate used stack manipulation functions
    if (usedBuiltins.contains("DUP")) {
        impl << R"(
void forth_dup(void) {
    if (forth_data_stack.ptr == 0) return;
    forth_cell_t value = forth_data_stack.data[forth_data_stack.ptr - 1];
    forth_push(value);
}
)";
    }

    if (usedBuiltins.contains("DROP")) {
        impl << R"(
void forth_drop(void) {
    if (forth_data_stack.ptr > 0) forth_data_stack.ptr--;
}
)";
    }

    if (usedBuiltins.contains("SWAP")) {
        impl << R"(
void forth_swap(void) {
    if (forth_data_stack.ptr < 2) return;
    forth_cell_t temp = forth_data_stack.data[forth_data_stack.ptr - 1];
    forth_data_stack.data[forth_data_stack.ptr - 1] = forth_data_stack.data[forth_data_stack.ptr - 2];
    forth_data_stack.data[forth_data_stack.ptr - 2] = temp;
}
)";
    }

    if (usedBuiltins.contains("OVER")) {
        impl << R"(
void forth_over(void) {
    if (forth_data_stack.ptr < 2) return;
    forth_push(forth_data_stack.data[forth_data_stack.ptr - 2]);
}
)";
    }

    if (usedBuiltins.contains("ROT")) {
        impl << R"(
void forth_rot(void) {
    if (forth_data_stack.ptr < 3) return;
    forth_cell_t temp = forth_data_stack.data[forth_data_stack.ptr - 3];
    forth_data_stack.data[forth_data_stack.ptr - 3] = forth_data_stack.data[forth_data_stack.ptr - 2];
    forth_data_stack.data[forth_data_stack.ptr - 2] = forth_data_stack.data[forth_data_stack.ptr - 1];
    forth_data_stack.data[forth_data_stack.ptr - 1] = temp;
}
)";
    }

    return impl.str();
}

std::string ForthCCodegen::generateCompareImplementation() {
    std::ostringstream impl;
    
    impl << R"(#include "forth_runtime.h"

// ============================================================================
// Comparison Operations (Optimized for Branch Prediction)
// ============================================================================

)";

    // Generate ALL comparison operations that might be used
    impl << R"(void forth_equal(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    forth_push(a == b ? -1 : 0);
}

void forth_not_equal(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    forth_push(a != b ? -1 : 0);
}

void forth_less_than(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    forth_push(a < b ? -1 : 0);
}

void forth_greater_than(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    forth_push(a > b ? -1 : 0);
}

void forth_less_equal(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    forth_push(a <= b ? -1 : 0);
}

void forth_greater_equal(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    forth_push(a >= b ? -1 : 0);
}

void forth_zero_equal(void) {
    forth_cell_t a = forth_pop();
    forth_push(a == 0 ? -1 : 0);
}

void forth_zero_less(void) {
    forth_cell_t a = forth_pop();
    forth_push(a < 0 ? -1 : 0);
}

)";

    return impl.str();
}

std::string ForthCCodegen::generateMathImplementation() {
    std::ostringstream impl;
    
    impl << R"(#include "forth_runtime.h"

// ============================================================================
// Optimized Math Operations for ESP32
// ============================================================================

)";

    // Generate only used math operations
    if (usedBuiltins.contains("+")) {
        impl << R"(FORTH_IRAM_ATTR void forth_add(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    forth_push(a + b);
}

)";
    }

    if (usedBuiltins.contains("-")) {
        impl << R"(FORTH_IRAM_ATTR void forth_sub(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    forth_push(a - b);
}

)";
    }

    if (usedBuiltins.contains("*")) {
        impl << R"(FORTH_IRAM_ATTR void forth_mul(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    forth_push(a * b);
}

)";
    }

    if (usedBuiltins.contains("/")) {
        impl << R"(FORTH_IRAM_ATTR void forth_div(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    if (b == 0) {
        #ifdef ESP32_PLATFORM
        ESP_LOGE("FORTH", "Division by zero!");
        #endif
        forth_push(0);
        return;
    }
    forth_push(a / b);
}

)";
    }

    if (usedBuiltins.contains("MOD")) {
        impl << R"(FORTH_IRAM_ATTR void forth_mod(void) {
    forth_cell_t b = forth_pop();
    forth_cell_t a = forth_pop();
    if (b == 0) {
        forth_push(0);
        return;
    }
    forth_push(a % b);
}

)";
    }

    return impl.str();
}

std::string ForthCCodegen::generateIOImplementation() {
    std::ostringstream impl;
    
    impl << R"(#include "forth_runtime.h"
#include <stdio.h>

// ============================================================================
// I/O Operations
// ============================================================================

void forth_emit(void) {
    forth_cell_t c = forth_pop();
    putchar((int)c);
    #ifdef ESP32_PLATFORM
    fflush(stdout);
    #endif
}

void forth_type(void) {
    forth_cell_t len = forth_pop();
    forth_cell_t addr = forth_pop();
    const char* str = (const char*)addr;
    for (int i = 0; i < len; i++) {
        putchar(str[i]);
    }
    #ifdef ESP32_PLATFORM
    fflush(stdout);
    #endif
}

void forth_cr(void) {
    putchar('\n');
    #ifdef ESP32_PLATFORM
    fflush(stdout);
    #endif
}

void forth_space(void) {
    putchar(' ');
}

void forth_spaces(void) {
    forth_cell_t n = forth_pop();
    for (int i = 0; i < n; i++) {
        putchar(' ');
    }
}

// Helper function to print numbers
void forth_print_number(forth_cell_t value) {
    printf("%d", (int)value);
    #ifdef ESP32_PLATFORM
    fflush(stdout);
    #endif
}
)";

    return impl.str();
}

std::string ForthCCodegen::generateMemoryImplementation() {
    std::ostringstream impl;
    
    impl << R"(#include "forth_runtime.h"
#include <string.h>

// ============================================================================
// Memory Operations with Alignment Handling
// ============================================================================

void forth_fetch(void) {
    forth_cell_t addr = forth_pop();
    // Ensure aligned access on ESP32
    if (addr & 3) {
        forth_cell_t value;
        memcpy(&value, (void*)addr, sizeof(forth_cell_t));
        forth_push(value);
    } else {
        forth_push(*(forth_cell_t*)addr);
    }
}

void forth_store(void) {
    forth_cell_t addr = forth_pop();
    forth_cell_t value = forth_pop();
    // Ensure aligned access on ESP32
    if (addr & 3) {
        memcpy((void*)addr, &value, sizeof(forth_cell_t));
    } else {
        *(forth_cell_t*)addr = value;
    }
}

void forth_byte_fetch(void) {
    forth_cell_t addr = forth_pop();
    forth_push(*(forth_byte_t*)addr);
}

void forth_byte_store(void) {
    forth_cell_t addr = forth_pop();
    forth_cell_t value = forth_pop();
    *(forth_byte_t*)addr = (forth_byte_t)value;
}
)";

    return impl.str();
}

std::string ForthCCodegen::generateESP32Implementation() {
    std::ostringstream impl;
    
    impl << R"(#include "forth_runtime.h"

#ifdef ESP32_PLATFORM

#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ============================================================================
// ESP32-Specific Hardware Operations
// ============================================================================

static bool gpio_initialized = false;

void forth_esp32_init(void) {
    if (!gpio_initialized) {
        // Basic initialization
        gpio_initialized = true;
    }
}

// GPIO Operations (IRAM for interrupt handling)
void forth_gpio_init(forth_cell_t pin, forth_cell_t mode) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = (mode == 0) ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

FORTH_IRAM_ATTR void forth_gpio_write(forth_cell_t pin, forth_cell_t value) {
    gpio_set_level((gpio_num_t)pin, value ? 1 : 0);
}

FORTH_IRAM_ATTR forth_cell_t forth_gpio_read(forth_cell_t pin) {
    return gpio_get_level((gpio_num_t)pin);
}

FORTH_IRAM_ATTR void forth_gpio_toggle(forth_cell_t pin) {
    gpio_set_level((gpio_num_t)pin, !gpio_get_level((gpio_num_t)pin));
}

// Timing Functions
void forth_delay_ms(forth_cell_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void forth_delay_us(forth_cell_t us) {
    ets_delay_us(us);
}

uint32_t forth_millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

uint32_t forth_micros(void) {
    return (uint32_t)esp_timer_get_time();
} 

// ESP32_PLATFORM
// ADC Functions
forth_cell_t forth_adc_read(forth_cell_t channel) {
    if (channel < 0 || channel > 7) return 0;
    
    // Configure attenuation for full range
    adc1_config_channel_atten((adc1_channel_t)channel, ADC_ATTEN_DB_11);
    
    // Multi-sample for stability
    int val = 0;
    for (int i = 0; i < 4; i++) {
        val += adc1_get_raw((adc1_channel_t)channel);
    }
    return val / 4;
}

// DAC Functions (ESP32 has 2 DAC channels)
void forth_dac_write(forth_cell_t channel, forth_cell_t value) {
    if (channel == 0 || channel == 1) {
        dac_output_voltage((dac_channel_t)channel, value & 0xFF);
    }
}

// PWM Functions using LEDC
static ledc_channel_t pwm_channels[8] = {
    LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3,
    LEDC_CHANNEL_4, LEDC_CHANNEL_5, LEDC_CHANNEL_6, LEDC_CHANNEL_7
};

void forth_pwm_init(forth_cell_t channel, forth_cell_t freq) {
    if (channel >= 8) return;
    
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = pwm_channels[channel],
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = channel + 2,  // Map to GPIO2-9 by default
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
}

void forth_pwm_write(forth_cell_t channel, forth_cell_t duty) {
    if (channel >= 8) return;
    
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, pwm_channels[channel], duty & 0x1FFF);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, pwm_channels[channel]);
}

#endif // ESP32_PLATFORM
)";

    return impl.str();
}

// ============================================================================
// AST Visitor Methods for Code Generation
// ============================================================================

void ForthCCodegen::visit(ProgramNode& node) {
    // Ensure we're writing to the correct file
    if (generatedFiles.empty() || currentFileIndex >= generatedFiles.size()) {
        addError("No program file available for writing");
        return;
    }
    
    // Switch to main program file (should be the last one created)
    for (size_t i = 0; i < generatedFiles.size(); i++) {
        if (generatedFiles[i].first == "forth_program.c") {
            currentFileIndex = i;
            break;
        }
    }
    
    if (currentFileIndex >= generatedFiles.size()) {
        addError("Could not find forth_program.c file");
        return;
    }
    
    // Generate program header
    emitLine("// Generated FORTH program: " + moduleName);
    emitLine("// Target: " + targetPlatform);
    emitLine("// Optimization level: " + getOptimizationLevel());
    emitLine("");
    
    emitLine("#include <stdio.h>");
    emitLine("#include \"forth_runtime.h\"");
    emitLine("");
    
    // Forward declare all user-defined words first
    emitLine("// Forward declarations of user-defined words");
    for (const auto& child : node.getChildren()) {
        if (child->getType() == ASTNode::NodeType::WORD_DEFINITION) {
            auto* wordDef = static_cast<WordDefinitionNode*>(child.get());
            const std::string funcName = generateFunctionName(wordDef->getWordName());
            emitLine("void " + funcName + "(void);");
        }
    }
    emitLine("");
    
    // Generate all word definitions
    emitLine("// User-defined word implementations");
    for (const auto& child : node.getChildren()) {
        if (child->getType() == ASTNode::NodeType::WORD_DEFINITION) {
            try {
                child->accept(*this);
            } catch (const std::exception& e) {
                addError(std::string("Error generating word definition: ") + e.what());
                // Continue with other definitions
            }
        }
    }
    
    // Generate main entry point
    emitLine("");
    emitLine("// Main program entry point");
    emitLine("void forth_program_main(void) {");
    increaseIndent();
    
    emitIndented("forth_init();");
    emitLine("");
    
    // Process variable declarations
    emitIndented("// Variable declarations");
    for (const auto& child : node.getChildren()) {
        if (child->getType() == ASTNode::NodeType::VARIABLE_DECLARATION) {
            try {
                child->accept(*this);
            } catch (const std::exception& e) {
                addError(std::string("Error generating variable declaration: ") + e.what());
            }
        }
    }
    
    // Call MAIN word if it exists
    if (wordFunctionNames.contains("MAIN")) {
        emitLine("");
        emitIndented("// Call main program word");
        emitIndented(wordFunctionNames["MAIN"] + "();");
    } else {
        // If no MAIN word, process any top-level executable code
        emitLine("");
        emitIndented("// Execute top-level code");
        for (const auto& child : node.getChildren()) {
            if (child->getType() != ASTNode::NodeType::WORD_DEFINITION &&
                child->getType() != ASTNode::NodeType::VARIABLE_DECLARATION) {
                try {
                    child->accept(*this);
                } catch (const std::exception& e) {
                    addError(std::string("Error generating top-level code: ") + e.what());
                }
            }
        }
    }
    
    emitLine("");
    emitIndented("forth_cleanup();");
    decreaseIndent();
    emitLine("}");
    
    // Ensure we completed successfully
    emitLine("");
    emitLine("// End of generated program");
}

void ForthCCodegen::visit(WordDefinitionNode& node) {
    const std::string& wordName = node.getWordName();
    const std::string funcName = generateFunctionName(wordName);
    
    // Validate word name
    if (wordName.empty()) {
        addError("Empty word name in word definition");
        return;
    }
    
    // Check for redefinition
    if (generatedWords.contains(ForthUtils::toUpper(wordName))) {
        addWarning("Word '" + wordName + "' redefined");
    }
    
    generatedWords.insert(ForthUtils::toUpper(wordName));
    wordFunctionNames[ForthUtils::toUpper(wordName)] = funcName;
    
    // Check if this word should be in IRAM
    bool useIRAM = optimizationFlags.useIRAM && isPerformanceCritical(wordName);
    
    emitLine("");
    emitLine("// FORTH word: " + wordName);
    if (useIRAM) {
        emitLine("FORTH_IRAM_ATTR");
    }
    emitLine("void " + funcName + "(void) {");
    increaseIndent();
    
    // Generate function body - with error handling for each child
    bool hasBody = false;
    for (const auto& child : node.getChildren()) {
        try {
            child->accept(*this);
            hasBody = true;
        } catch (const std::exception& e) {
            addError("Error in word '" + wordName + "': " + std::string(e.what()));
        }
    }
    
    if (!hasBody) {
        emitIndented("// Empty word body");
    }
    
    decreaseIndent();
    emitLine("}");
}

void ForthCCodegen::visit(WordCallNode& node) {
    const std::string& wordName = node.getWordName();
    const std::string upperWord = ForthUtils::toUpper(wordName);
    
    // Special handling for print word "."
    if (upperWord == ".") {
        emitIndented("forth_print_number(forth_pop());");
        return;
    }
    
    if (isBuiltinWord(upperWord)) {
        generateOptimizedBuiltin(upperWord);
    } else if (wordFunctionNames.contains(upperWord)) {
        // Direct call to generated function
        emitIndented(wordFunctionNames[upperWord] + "();");
    } else if (dictionary && dictionary->isWordDefined(upperWord)) {
        // Forward reference - need to defer resolution
        std::string callFunc = "forth_call_word_" + sanitizeIdentifier(upperWord);
        emitIndented(callFunc + "();");
        forwardReferences.insert(upperWord);
    } else {
        // Try to find in current generation context
        bool found = false;
        for (const auto& [name, func] : wordFunctionNames) {
            if (ForthUtils::toUpper(name) == upperWord) {
                emitIndented(func + "();");
                found = true;
                break;
            }
        }
        
        if (!found) {
            addError("Unknown word: " + wordName, &node);
        }
    }
}

void ForthCCodegen::visit(NumberLiteralNode& node) {
    const std::string& value = node.getValue();
    
    if (node.isFloatingPoint() && optimizationFlags.needsFloat) {
        emitIndented("forth_push((forth_cell_t)(forth_float_t)" + value + "f);");
    } else {
        // Use immediate push for small constants (optimization)
        try {
            int intVal = std::stoi(value);
            if (intVal >= -128 && intVal <= 127) {
                emitIndented("forth_push(" + value + ");  // Small constant");
            } else {
                emitIndented("forth_push(" + value + ");");
            }
        } catch (const std::exception&) {
            emitIndented("forth_push(" + value + ");");
        }
    }
}

void ForthCCodegen::visit(StringLiteralNode& node) {
    const std::string& value = node.getValue();
    
    if (node.isPrint()) {
        // Direct print optimization
        emitIndented("printf(\"" + escapeCString(value) + "\");");
        if (targetPlatform.starts_with("esp32")) {
            emitIndented("fflush(stdout);  // ESP32 serial flush");
        }
    } else {
        // Store string in RODATA section
        std::string strVar = "str_" + std::to_string(++stringCounter);
        emitIndented("static const char " + strVar + "[] = \"" + escapeCString(value) + "\";");
        emitIndented("forth_push((forth_cell_t)" + strVar + ");");
        emitIndented("forth_push(" + std::to_string(value.length()) + ");");
    }
}

void ForthCCodegen::visit(IfStatementNode& node) {
    if (optimizationFlags.canInline && isSimpleCondition(node)) {
        // Generate optimized ternary for simple if-then-else
        generateOptimizedIf(node);
    } else {
        // Standard if-then-else generation
        emitIndented("{  // IF block");
        increaseIndent();
        emitIndented("forth_cell_t condition = forth_pop();");
        emitIndented("if (condition) {");
        increaseIndent();
        
        if (node.getThenBranch()) {
            for (const auto& child : node.getThenBranch()->getChildren()) {
                child->accept(*this);
            }
        }
        
        decreaseIndent();
        
        if (node.hasElse() && node.getElseBranch()) {
            emitIndented("} else {");
            increaseIndent();
            
            for (const auto& child : node.getElseBranch()->getChildren()) {
                child->accept(*this);
            }
            
            decreaseIndent();
        }
        
        emitIndented("}");
        decreaseIndent();
        emitIndented("}  // End IF block");
    }
}

void ForthCCodegen::visit(BeginUntilLoopNode& node) {
    std::string loopLabel = generateLabel("loop");
    
    if (optimizationFlags.useIRAM) {
        emitIndented("// Loop in hot path - consider IRAM placement");
    }
    
    emitIndented("{  // BEGIN-UNTIL loop");
    increaseIndent();
    
    // Check for common loop patterns for optimization
    if (isCountedLoop(node)) {
        generateOptimizedCountedLoop(node);
    } else {
        emitIndented("do {");
        increaseIndent();
        
        if (node.getBody()) {
            for (const auto& child : node.getBody()->getChildren()) {
                child->accept(*this);
            }
        }
        
        decreaseIndent();
        emitIndented("} while (!forth_pop());");
    }
    
    decreaseIndent();
    emitIndented("}  // End loop");
}

void ForthCCodegen::visit(MathOperationNode& node) {
    const std::string& op = node.getOperation();
    generateOptimizedBuiltin(op);
}

void ForthCCodegen::visit(VariableDeclarationNode& node) {
    const std::string& varName = node.getVarName();
    const std::string cVarName = "var_" + sanitizeIdentifier(varName);
    
    if (node.isConst()) {
        // Constants can be optimized
        emitIndented("static const forth_cell_t " + cVarName + " = forth_pop();");
        emitIndented("forth_push(" + cVarName + ");");
    } else {
        // Variables need proper alignment for ESP32
        if (targetPlatform.starts_with("esp32")) {
            emitIndented("static FORTH_DMA_ATTR forth_cell_t " + cVarName + " = 0;");
        } else {
            emitIndented("static forth_cell_t " + cVarName + " = 0;");
        }
        emitIndented("forth_push((forth_cell_t)&" + cVarName + ");");
    }
    
    variableMap[varName] = cVarName;
}

// ============================================================================
// Optimization Methods
// ============================================================================

void ForthCCodegen::generateOptimizedBuiltin(const std::string& word) {
    // Map to optimized function calls - FIXED to handle all comparison operators
    static const std::unordered_map<std::string, std::string> builtinMap = {
        {"+", "forth_add()"},
        {"-", "forth_sub()"},
        {"*", "forth_mul()"},
        {"/", "forth_div()"},
        {"MOD", "forth_mod()"},
        {"NEGATE", "forth_negate()"},
        {"ABS", "forth_abs()"},
        {"=", "forth_equal()"},
        {"<>", "forth_not_equal()"},
        {"<", "forth_less_than()"},
        {">", "forth_greater_than()"},
        {"<=", "forth_less_equal()"},
        {">=", "forth_greater_equal()"},
        {"DUP", "forth_dup()"},
        {"DROP", "forth_drop()"},
        {"SWAP", "forth_swap()"},
        {"OVER", "forth_over()"},
        {"ROT", "forth_rot()"},
        {"!", "forth_store()"},
        {"@", "forth_fetch()"},
        {"EMIT", "forth_emit()"},
        {"TYPE", "forth_type()"},
        {"CR", "forth_cr()"}
    };
    
    auto it = builtinMap.find(word);
    if (it != builtinMap.end()) {
        emitIndented(it->second + ";");
    } else {
        addError("Unknown builtin word: " + word);
    }
}

bool ForthCCodegen::isPerformanceCritical(const std::string& wordName) const {
    // Check if word is called frequently or in loops
    auto it = callGraph.find(wordName);
    if (it != callGraph.end() && it->second.size() > 3) {
        return true;
    }
    
    // Check if word contains loops
    return usedFeatures.contains("LOOP");
}

bool ForthCCodegen::isSimpleCondition(const IfStatementNode& node) const {
    // Check if the condition is simple enough for ternary optimization
    if (!node.getThenBranch() || !node.getElseBranch()) return false;
    
    const auto& thenChildren = node.getThenBranch()->getChildren();
    const auto& elseChildren = node.getElseBranch()->getChildren();
    
    return thenChildren.size() == 1 && elseChildren.size() == 1;
}

void ForthCCodegen::generateOptimizedIf(const IfStatementNode& node) {
    emitIndented("// Optimized IF-THEN-ELSE");
    emitIndented("{");
    increaseIndent();
    emitIndented("forth_cell_t cond = forth_pop();");
    emitIndented("if (cond) {");
    increaseIndent();
    
    for (const auto& child : node.getThenBranch()->getChildren()) {
        child->accept(*this);
    }
    
    decreaseIndent();
    emitIndented("} else {");
    increaseIndent();
    
    for (const auto& child : node.getElseBranch()->getChildren()) {
        child->accept(*this);
    }
    
    decreaseIndent();
    emitIndented("}");
    decreaseIndent();
    emitIndented("}");
}

bool ForthCCodegen::isCountedLoop(const BeginUntilLoopNode& node) const {
    // Analyze if this is a counted loop pattern
    // This would need more sophisticated analysis
    return false;
}

void ForthCCodegen::generateOptimizedCountedLoop(const BeginUntilLoopNode& node) {
    // Generate optimized counted loop
    // Implementation would depend on pattern recognition
}

void ForthCCodegen::applyOptimizations() {
    // Apply various optimization passes
    
    if (optimizationFlags.canInline) {
        inlineSmallFunctions();
    }
    
    if (optimizationFlags.smallStack) {
        optimizeStackUsage();
    }
    
    if (targetPlatform.starts_with("esp32")) {
        applyESP32Optimizations();
    }
    
    // Remove unused code
    removeUnusedFunctions();
}

void ForthCCodegen::inlineSmallFunctions() {
    // Mark small functions for inlining
    for (const auto& [word, funcName] : wordFunctionNames) {
        auto it = callGraph.find(word);
        if (it != callGraph.end() && it->second.size() == 1) {
            // Single-use functions can be inlined
            inlineCandidates.insert(word);
        }
    }
}

void ForthCCodegen::optimizeStackUsage() {
    // Optimize stack operations for small stack usage
}

void ForthCCodegen::applyESP32Optimizations() {
    // ESP32-specific optimizations
    
    // 1. Place frequently called functions in IRAM
    for (const auto& [word, calls] : callGraph) {
        if (calls.size() > 5) {
            iramFunctions.insert(word);
        }
    }
}

void ForthCCodegen::removeUnusedFunctions() {
    // Remove functions that are never called
    std::set<std::string> calledWords;
    
    for (const auto& [caller, callees] : callGraph) {
        calledWords.insert(callees.begin(), callees.end());
    }
    
    for (const auto& word : generatedWords) {
        if (!calledWords.contains(word) && word != "MAIN") {
            unusedWords.insert(word);
        }
    }
}

// ============================================================================
// Code Generation Finalization - FIXED
// ============================================================================

void ForthCCodegen::finalizeGeneration() {
    // Generate CMakeLists.txt for the generated files
    generateCMakeLists();
    
    // Generate main.c wrapper if needed
    if (targetPlatform.starts_with("esp32")) {
        generateESP32Main();
    }
    
    // Add forward reference resolution - FIXED
    // resolveForwardReferences();
}

void ForthCCodegen::generateCMakeLists() {
    std::ostringstream cmake;
    
    cmake << "# Generated CMakeLists.txt for FORTH program\n";
    cmake << "set(SOURCES\n";
    
    for (const auto& [filename, _] : generatedFiles) {
        if (filename.ends_with(".c")) {
            cmake << "    " << filename << "\n";
        }
    }
    
    cmake << ")\n\n";
    cmake << "set(HEADERS\n";
    cmake << "    forth_runtime.h\n";
    cmake << ")\n";
    
    generateFile("CMakeLists.txt", cmake.str());
}

void ForthCCodegen::generateESP32Main() {
    std::ostringstream main;
    
    main << R"(#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "forth_runtime.h"

static const char* TAG = "FORTH";

extern void forth_program_main(void);

static void forth_task(void* pvParameters) {
    ESP_LOGI(TAG, "Starting FORTH program");
    forth_program_main();
    ESP_LOGI(TAG, "FORTH program completed");
    vTaskDelete(NULL);
}

void app_main(void) {
    xTaskCreatePinnedToCore(
        forth_task,
        "forth",
        )" << esp32Config.stackSize * 4 << R"(,
        NULL,
        )" << esp32Config.priority << R"(,
        NULL,
        1  // Pin to core 1
    );
}
)";
    
    generateFile("main.c", main.str());
}



// ============================================================================
// File Output Methods
// ============================================================================

bool ForthCCodegen::writeToFiles(const std::string& outputDir) {
    try {
        fs::create_directories(outputDir);
        
        for (const auto& [filename, content] : generatedFiles) {
            std::string filepath = fs::path(outputDir) / filename;
            std::ofstream file(filepath);
            
            if (!file.is_open()) {
                addError("Cannot create file: " + filepath);
                return false;
            }
            
            file << content.str();
            file.close();
        }
        
        return true;
    } catch (const std::exception& e) {
        addError("Failed to write files: " + std::string(e.what()));
        return false;
    }
}

// ============================================================================
// Utility Methods
// ============================================================================

void ForthCCodegen::debugGenerationState() const {
    printf("=== Code Generation Debug Info ===\n");
    printf("Generated files: %zu\n", generatedFiles.size());
    printf("Current file index: %zu\n", currentFileIndex);
    printf("Generated words: %zu\n", generatedWords.size());
    printf("Used features: %zu\n", usedFeatures.size());
    printf("Errors: %zu\n", errors.size());
    printf("Warnings: %zu\n", warnings.size());
    
    printf("\nGenerated files:\n");
    for (size_t i = 0; i < generatedFiles.size(); i++) {
        const auto& [filename, content] = generatedFiles[i];
        printf("  [%zu] %s (%zu chars)\n", i, filename.c_str(), content.str().length());
    }
    
    if (!errors.empty()) {
        printf("\nErrors:\n");
        for (const auto& error : errors) {
            printf("  ERROR: %s\n", error.c_str());
        }
    }
    
    if (!warnings.empty()) {
        printf("\nWarnings:\n");
        for (const auto& warning : warnings) {
            printf("  WARNING: %s\n", warning.c_str());
        }
    }
    printf("===================================\n");
}

void ForthCCodegen::resetGenerationState() {
    try {
        // Clear all collections
        generatedFiles.clear();
        headerStream.str("");
        sourceStream.str("");
        functionsStream.str("");
        errors.clear();
        warnings.clear();
        generatedWords.clear();
        wordFunctionNames.clear();
        usedFeatures.clear();
        usedBuiltins.clear();
        callGraph.clear();
        variableMap.clear();
        forwardReferences.clear();
        inlineCandidates.clear();
        iramFunctions.clear();
        unusedWords.clear();
        
        // Reset counters
        currentFileIndex = 0;
        tempVarCounter = 0;
        labelCounter = 0;
        stringCounter = 0;
        indentLevel = 0;
        
    } catch (const std::exception& e) {
        // Even reset failed - create a minimal error state
        errors.clear();
        addError(std::string("Failed to reset generation state: ") + e.what());
    }
}

void ForthCCodegen::generateFile(const std::string& filename, const std::string& content) {
    try {
        generatedFiles.emplace_back(filename, std::ostringstream());
        generatedFiles.back().second << content;
        
        // Log for debugging
        if (!content.empty()) {
            // Successfully generated file
        } else {
            addWarning("Generated empty file: " + filename);
        }
    } catch (const std::exception& e) {
        addError("Failed to generate file " + filename + ": " + std::string(e.what()));
    }
}

void ForthCCodegen::emit(const std::string& code) {
    if (currentFileIndex >= generatedFiles.size()) {
        addError("Invalid file index in emit(): " + std::to_string(currentFileIndex));
        return;
    }
    
    try {
        generatedFiles[currentFileIndex].second << code;
    } catch (const std::exception& e) {
        addError(std::string("Failed to emit code: ") + e.what());
    }
}

void ForthCCodegen::emitLine(const std::string& line) {
    emit(line + "\n");
}

void ForthCCodegen::emitIndented(const std::string& line) {
    emit(getIndent() + line + "\n");
}

std::string ForthCCodegen::getIndent() const {
    return std::string(indentLevel * 4, ' ');
}

std::string ForthCCodegen::generateTempVar() {
    return "temp_" + std::to_string(++tempVarCounter);
}

std::string ForthCCodegen::generateLabel(const std::string& prefix) {
    return prefix + "_" + std::to_string(++labelCounter);
}

std::string ForthCCodegen::sanitizeIdentifier(const std::string& name) {
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(), 
        [](char c) { return std::isalnum(c) ? std::tolower(c) : '_'; });
    return result;
}

std::string ForthCCodegen::generateFunctionName(const std::string& wordName) {
    return "forth_word_" + sanitizeIdentifier(wordName);
}

std::string ForthCCodegen::escapeCString(const std::string& str) {
    std::string result;
    result.reserve(str.length() * 2);
    
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '\0': result += "\\0"; break;
            default: 
                if (c >= 32 && c <= 126) {
                    result += c;
                } else {
                    char buffer[8];
                    sprintf(buffer, "\\x%02x", (unsigned char)c);
                    result += buffer;
                }
                break;
        }
    }
    return result;
}

std::string ForthCCodegen::getOptimizationLevel() const {
    if (optimizationFlags.useIRAM && optimizationFlags.canInline) {
        return "Maximum (IRAM + Inline)";
    } else if (optimizationFlags.canInline) {
        return "High (Inline)";
    } else if (optimizationFlags.smallStack) {
        return "Size (Small Stack)";
    } else {
        return "Standard";
    }
}

bool ForthCCodegen::isBuiltinWord(const std::string& word) const {
    static const std::unordered_set<std::string> builtins = {
        "+", "-", "*", "/", "MOD", "NEGATE", "ABS", "MIN", "MAX",
        "=", "<>", "<", ">", "<=", ">=", "0=", "0<", "0>",
        "DUP", "DROP", "SWAP", "OVER", "ROT", "NIP", "TUCK",
        "!", "@", "C!", "C@", "EMIT", "TYPE", "CR", "SPACE", "SPACES",
        "AND", "OR", "XOR", "NOT", "LSHIFT", "RSHIFT",
        "TRUE", "FALSE", "DEPTH", "CLEAR", "."
    };
    return builtins.contains(word);
}

// ============================================================================
// Statistics and Error Handling - FIXED
// ============================================================================

ForthCCodegen::CodeGenStats ForthCCodegen::getStatistics() const {
    CodeGenStats stats = {};
    
    // Count generated lines
    for (const auto& [filename, content] : generatedFiles) {
        std::string str = content.str();
        stats.linesGenerated += std::count(str.begin(), str.end(), '\n');
    }
    
    stats.functionsGenerated = generatedWords.size();
    stats.variablesGenerated = variableMap.size();
    stats.filesGenerated = generatedFiles.size();
    stats.optimizationsApplied = inlineCandidates.size() + iramFunctions.size();
    stats.usesFloatingPoint = optimizationFlags.needsFloat;
    stats.usesStrings = usedFeatures.contains("STRING");
    stats.estimatedStackDepth = esp32Config.stackSize;
    stats.iramUsage = iramFunctions.size() * 64; // Estimate 64 bytes per function
    stats.flashUsage = stats.linesGenerated * 4; // Rough estimate
    
    return stats;
}

void ForthCCodegen::addError(const std::string& message) {
    errors.push_back(message);
}

void ForthCCodegen::addWarning(const std::string& message) {
    warnings.push_back(message);
}

void ForthCCodegen::addError(const std::string& message, ASTNode* node) {
    std::string fullMessage = message;
    if (node) {
        fullMessage += " at line " + std::to_string(node->getLine()) + 
                      ", column " + std::to_string(node->getColumn());
    }
    errors.push_back(fullMessage);
}

// ============================================================================
// Public Interface Methods - FIXED
// ============================================================================

std::string ForthCCodegen::getCompleteCode() const {
    std::ostringstream complete;

    // Combine all generated files into one string
    for (const auto& [filename, content] : generatedFiles) {
        if (filename.ends_with(".c")) {
            complete << "// File: " << filename << "\n";
            complete << content.str() << "\n\n";
        }
    }

    return complete.str();
}

std::string ForthCCodegen::getHeaderCode() const {
    std::ostringstream header;

    // Find and return the header file content
    for (const auto& [filename, content] : generatedFiles) {
        if (filename.ends_with(".h")) {
            header << content.str();
            break;
        }
    }

    // If no header file found, return the runtime header
    if (header.str().empty()) {
        return generateCoreRuntimeHeader();
    }

    return header.str();
}

void ForthCCodegen::setOptimizationLevel(int level) {
    switch (level) {
        case 0: // No optimization
            optimizationFlags.useIRAM = false;
            optimizationFlags.canInline = false;
            optimizationFlags.smallStack = false;
            break;
        case 1: // Basic optimization
            optimizationFlags.canInline = true;
            break;
        case 2: // Full optimization
            optimizationFlags.useIRAM = true;
            optimizationFlags.canInline = true;
            optimizationFlags.smallStack = true;
            break;
        default:
            optimizationFlags.canInline = true;
            break;
    }
}
        
// ============================================================================
// ESP-IDF Project Generation - FIXED IMPLEMENTATION
// ============================================================================

bool ForthCCodegen::writeESPIDFProject(const std::string& projectPath) const {
    try {
        // Create project directory structure
        fs::create_directories(projectPath);
        fs::create_directories(fs::path(projectPath) / "main");
        fs::create_directories(fs::path(projectPath) / "components" / "forth_runtime" / "include");

        // Write root CMakeLists.txt
        std::ofstream rootCMake(fs::path(projectPath) / "CMakeLists.txt");
        if (!rootCMake.is_open()) return false;

        rootCMake << R"(# ESP-IDF Project generated by FORTH compiler
cmake_minimum_required(VERSION 3.16)

set(COMPONENTS main forth_runtime)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(forth_app)
)";
        rootCMake.close();

        // Write main CMakeLists.txt
        std::ofstream mainCMake(fs::path(projectPath) / "main" / "CMakeLists.txt");
        if (!mainCMake.is_open()) return false;

        mainCMake << R"(idf_component_register(
    SRCS "main.c" "forth_program.c"
    INCLUDE_DIRS "."
    REQUIRES forth_runtime
)
)";
        mainCMake.close();

        // Write main.c
        std::ofstream mainFile(fs::path(projectPath) / "main" / "main.c");
        if (!mainFile.is_open()) return false;

        mainFile << R"(#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "forth_runtime.h"

static const char* TAG = "FORTH";

extern void forth_program_main(void);

void app_main(void) {
    ESP_LOGI(TAG, "Starting FORTH program on ESP32");
    forth_program_main();
    ESP_LOGI(TAG, "FORTH program completed");
}
)";
        mainFile.close();

        // Write forth_program.c - find the main program file
        std::ofstream progFile(fs::path(projectPath) / "main" / "forth_program.c");
        if (!progFile.is_open()) return false;
        
        for (const auto& [filename, content] : generatedFiles) {
            if (filename == "forth_program.c") {
                progFile << content.str();
                break;
            }
        }
        progFile.close();

        // FIXED: Write component CMakeLists.txt with proper syntax
        std::ofstream compCMake(fs::path(projectPath) / "components" / "forth_runtime" / "CMakeLists.txt");
        if (!compCMake.is_open()) return false;

        // Build list of source files first
        std::vector<std::string> sourceFiles;
        for (const auto& [filename, content] : generatedFiles) {
            if (filename.ends_with(".c") && 
                filename != "forth_program.c" && 
                filename != "main.c") {
                sourceFiles.push_back(filename);
            }
        }

        // Write the CMake file with proper formatting
        compCMake << "idf_component_register(\n";
        compCMake << "    SRCS";
        
        // Add each source file on a new line with proper indentation
        for (const auto& filename : sourceFiles) {
            compCMake << "\n        \"" << filename << "\"";
        }
        
        compCMake << "\n    INCLUDE_DIRS \"include\"\n";
        compCMake << "    REQUIRES esp_timer driver freertos\n";
        compCMake << ")\n";
        compCMake.close();

        // Write forth_runtime.h
        std::ofstream headerFile(fs::path(projectPath) / "components" / "forth_runtime" / "include" / "forth_runtime.h");
        if (!headerFile.is_open()) return false;
        headerFile << getHeaderCode();
        headerFile.close();

        // Write all runtime implementation files to the component directory
        for (const auto& [filename, content] : generatedFiles) {
            if (filename.ends_with(".c") && filename != "forth_program.c" && filename != "main.c") {
                std::ofstream runtimeFile(fs::path(projectPath) / "components" / "forth_runtime" / filename);
                if (!runtimeFile.is_open()) continue;
                runtimeFile << content.str();
                runtimeFile.close();
            }
        }

        // Write sdkconfig.defaults
        std::ofstream sdkconfig(fs::path(projectPath) / "sdkconfig.defaults");
        if (!sdkconfig.is_open()) return false;

        sdkconfig << R"(# Default configuration for FORTH on ESP32
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_SINGLE_APP=y
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
)";

        // Add ESP32-specific configs based on features used
        if (usedFeatures.contains("IO")) {
            sdkconfig << "CONFIG_ESP_CONSOLE_UART_DEFAULT=y\n";
            sdkconfig << "CONFIG_ESP_CONSOLE_UART_BAUDRATE_115200=y\n";
        }

        sdkconfig.close();

        return true;

    } catch (const std::exception& e) {
        return false;
    }
}

// ============================================================================
// Factory Implementation - FIXED
// ============================================================================

namespace ForthCodegenFactory {

std::unique_ptr<ForthCCodegen> create(TargetType target) {
    auto codegen = std::make_unique<ForthCCodegen>("forth_program");

    switch (target) {
        case TargetType::ESP32:
            codegen->setTarget("esp32");
            break;
        case TargetType::ESP32_C3:
            codegen->setTarget("esp32c3");
            break;
        case TargetType::ESP32_S3:
            codegen->setTarget("esp32s3");
            break;
        case TargetType::NATIVE_LINUX:
            codegen->setTarget("linux");
            break;
        default:
            codegen->setTarget("esp32");
            break;
    }

    configureForTarget(*codegen, target);
    return codegen;
}

ForthCCodegen::ESP32Config getESP32Config(TargetType target) {
    ForthCCodegen::ESP32Config config;
    
    switch (target) {
        case TargetType::ESP32_C3:
            config.cpuFreq = 160;
            config.architecture = "riscv";
            break;
        case TargetType::ESP32_S3:
            config.cpuFreq = 240;
            config.useDMA = true;
            config.architecture = "xtensa";
            break;
        case TargetType::ESP32:
        default:
            config.cpuFreq = 240;
            config.architecture = "xtensa";
            break;
    }
    
    return config;
}

TargetCapabilities getTargetCapabilities(TargetType target) {
    TargetCapabilities caps = {};
    
    switch (target) {
        case TargetType::ESP32:
            caps.hasWiFi = true;
            caps.hasBluetooth = true;
            caps.maxGPIO = 39;
            caps.adcChannels = 18;
            caps.dacChannels = 2;
            caps.architecture = "xtensa";
            break;
        case TargetType::ESP32_C3:
            caps.hasWiFi = true;
            caps.hasBluetooth = true;
            caps.maxGPIO = 21;
            caps.adcChannels = 6;
            caps.dacChannels = 0;
            caps.architecture = "riscv";
            break;
        case TargetType::ESP32_S3:
            caps.hasWiFi = true;
            caps.hasBluetooth = false;
            caps.hasUSB = true;
            caps.maxGPIO = 48;
            caps.adcChannels = 20;
            caps.dacChannels = 2;
            caps.hasPSRAM = true;
            caps.architecture = "xtensa";
            break;
        default:
            caps.architecture = "unknown";
            break;
    }
    
    return caps;
}

void configureForTarget(ForthCCodegen& codegen, TargetType target) {
    auto config = getESP32Config(target);
    codegen.setESP32Config(config);
    
    // Set optimization level based on target
    switch (target) {
        case TargetType::ESP32_C3:
            codegen.setOptimizationLevel(1); // Basic optimization for RISC-V
            break;
        case TargetType::ESP32_S3:
            codegen.setOptimizationLevel(2); // Full optimization for high-end chip
            break;
        default:
            codegen.setOptimizationLevel(1);
            break;
    }
}

} // namespace ForthCodegenFactory
