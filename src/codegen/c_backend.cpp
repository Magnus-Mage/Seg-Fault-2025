#include "codegen/c_backend.h"
#include "common/utils.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iomanip>

namespace fs = std::filesystem;

ForthCCodegen::ForthCCodegen(const std::string& name) 
    : moduleName(name), targetPlatform("esp32"), indentLevel(0), 
      tempVarCounter(0), labelCounter(0), semanticAnalyzer(nullptr), dictionary(nullptr) {
}

bool ForthCCodegen::generateCode(const ProgramNode& program) {
    // Clear previous generation
    headerStream.str("");
    sourceStream.str("");
    functionsStream.str("");
    errors.clear();
    warnings.clear();
    generatedWords.clear();
    wordFunctionNames.clear();
    
    try {
        // Generate runtime infrastructure
        generateRuntimeHeader();
        generateRuntimeImplementation();
        
        // Generate main program
        const_cast<ProgramNode&>(program).accept(*this);
        
        // Optimize generated code
        optimizeGeneratedCode();
        
        return !hasErrors();
        
    } catch (const std::exception& e) {
        addError("Code generation failed: " + std::string(e.what()));
        return false;
    }
}

void ForthCCodegen::visit(ProgramNode& node) {
    emitLine("// Generated FORTH program: " + moduleName);
    emitLine("// Target: " + targetPlatform);
    emitLine("// Generated on: " + std::string(__DATE__) + " " + std::string(__TIME__));
    emitLine("");
    
    // Generate program entry point
    emitLine("void forth_program_main(void) {");
    increaseIndent();
    emitIndented("forth_stack_init();");
    emitLine("");
    
    // Process all top-level statements
    for (const auto& child : node.getChildren()) {
        child->accept(*this);
    }
    
    emitLine("");
    emitIndented("forth_stack_cleanup();");
    decreaseIndent();
    emitLine("}");
}

void ForthCCodegen::visit(WordDefinitionNode& node) {
    const std::string& wordName = node.getWordName();
    const std::string funcName = generateFunctionName(wordName);
    
    if (generatedWords.contains(wordName)) {
        addWarning("Word '" + wordName + "' redefined");
        return;
    }
    
    generatedWords.insert(wordName);
    wordFunctionNames[wordName] = funcName;
    
    // Generate function signature
    functionsStream << "// FORTH word: " << wordName << "\n";
    functionsStream << "void " << funcName << "(void) {\n";
    
    // Generate function body
    int savedIndent = indentLevel;
    indentLevel = 1;
    
    for (const auto& child : node.getChildren()) {
        std::ostringstream savedSource;
        savedSource << sourceStream.rdbuf();
        sourceStream.str("");
        
        child->accept(*this);
        
        functionsStream << sourceStream.str();
        sourceStream.str("");
        sourceStream << savedSource.str();
    }
    
    indentLevel = savedIndent;
    functionsStream << "}\n\n";
}

void ForthCCodegen::visit(WordCallNode& node) {
    const std::string& wordName = node.getWordName();
    const std::string upperWord = ForthUtils::toUpper(wordName);
    
    // Check if it's a builtin word
    if (isBuiltinWord(upperWord)) {
        generateBuiltinWord(upperWord);
    } else if (wordFunctionNames.contains(upperWord)) {
        emitIndented(wordFunctionNames[upperWord] + "();");
    } else if (dictionary && dictionary->isWordDefined(upperWord)) {
        // Forward reference - will be resolved later
        emitIndented("forth_call_word(\"" + upperWord + "\");");
        addWarning("Forward reference to word: " + wordName);
    } else {
        addError("Unknown word: " + wordName, &node);
    }
}

void ForthCCodegen::visit(NumberLiteralNode& node) {
    const std::string& value = node.getValue();
    
    if (node.isFloatingPoint()) {
        emitIndented("forth_push_float(" + value + "f);");
    } else {
        emitIndented("forth_push_int(" + value + ");");
    }
}

void ForthCCodegen::visit(StringLiteralNode& node) {
    const std::string& value = node.getValue();
    
    if (node.isPrint()) {
        // Generate print string operation
        emitIndented("printf(\"" + escapeCString(value) + "\");");
    } else {
        // Push string address and length
        std::string tempVar = generateTempVar();
        emitIndented("static const char " + tempVar + "[] = \"" + escapeCString(value) + "\";");
        emitIndented("forth_push_int((forth_int_t)" + tempVar + ");");
        emitIndented("forth_push_int(" + std::to_string(value.length()) + ");");
    }
}

void ForthCCodegen::visit(IfStatementNode& node) {
    std::string endLabel = generateLabel("if_end");
    std::string elseLabel = node.hasElse() ? generateLabel("if_else") : "";
    
    // Generate condition check
    emitIndented("if (forth_pop_int() != 0) {");
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
}

void ForthCCodegen::visit(BeginUntilLoopNode& node) {
    std::string loopLabel = generateLabel("loop_begin");
    std::string endLabel = generateLabel("loop_end");
    
    emitIndented("do {");
    increaseIndent();
    
    // Generate loop body
    if (node.getBody()) {
        for (const auto& child : node.getBody()->getChildren()) {
            child->accept(*this);
        }
    }
    
    decreaseIndent();
    emitIndented("} while (forth_pop_int() == 0);");
}

void ForthCCodegen::visit(MathOperationNode& node) {
    const std::string& op = node.getOperation();
    generateBuiltinWord(op);
}

void ForthCCodegen::visit(VariableDeclarationNode& node) {
    const std::string& varName = node.getVarName();
    const std::string cVarName = "forth_var_" + sanitizeIdentifier(varName);
    
    if (node.isConst()) {
        // Generate constant
        emitIndented("static const forth_int_t " + cVarName + " = forth_pop_int();");
        emitIndented("forth_push_int(" + cVarName + ");");
    } else {
        // Generate variable
        emitIndented("static forth_int_t " + cVarName + " = 0;");
        emitIndented("forth_push_int((forth_int_t)&" + cVarName + ");");
    }
}

// Code generation utilities
void ForthCCodegen::emit(const std::string& code) {
    sourceStream << code;
}

void ForthCCodegen::emitLine(const std::string& line) {
    sourceStream << line << "\n";
}

void ForthCCodegen::emitIndented(const std::string& line) {
    sourceStream << getIndent() << line << "\n";
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

void ForthCCodegen::generateRuntimeHeader() {
    headerStream << R"(// FORTH Runtime Header
#ifndef FORTH_RUNTIME_H
#define FORTH_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// FORTH data types
typedef int32_t forth_int_t;
typedef float forth_float_t;
typedef bool forth_bool_t;

// Stack operations
void forth_stack_init(void);
void forth_stack_cleanup(void);
void forth_push_int(forth_int_t value);
void forth_push_float(forth_float_t value);
forth_int_t forth_pop_int(void);
forth_float_t forth_pop_float(void);
void forth_dup(void);
void forth_drop(void);
void forth_swap(void);
void forth_over(void);
void forth_rot(void);

// Math operations
void forth_add(void);
void forth_sub(void);
void forth_mul(void);
void forth_div(void);
void forth_mod(void);
void forth_negate(void);
void forth_abs(void);

// Comparison operations
void forth_equal(void);
void forth_not_equal(void);
void forth_less_than(void);
void forth_greater_than(void);
void forth_less_equal(void);
void forth_greater_equal(void);

// Memory operations
void forth_fetch(void);
void forth_store(void);

// I/O operations
void forth_emit(void);
void forth_type(void);

// Control flow
void forth_call_word(const char* word_name);

// ESP32 specific (if enabled)
#ifdef ESP32_PLATFORM
void forth_gpio_init(void);
void forth_gpio_set(void);
void forth_gpio_get(void);
void forth_delay(void);
#endif

// Program entry point
void forth_program_main(void);

#ifdef __cplusplus
}
#endif

#endif // FORTH_RUNTIME_H
)";
}

void ForthCCodegen::generateRuntimeImplementation() {
    functionsStream << R"(// FORTH Runtime Implementation
#include "forth_runtime.h"
#include <stdlib.h>
#include <string.h>

// Stack implementation
#define STACK_SIZE 1024
static forth_int_t stack[STACK_SIZE];
static size_t stack_pointer = 0;

void forth_stack_init(void) {
    stack_pointer = 0;
    memset(stack, 0, sizeof(stack));
}

void forth_stack_cleanup(void) {
    // Nothing to do for simple implementation
}

void forth_push_int(forth_int_t value) {
    if (stack_pointer >= STACK_SIZE) {
        printf("ERROR: Stack overflow\n");
        return;
    }
    stack[stack_pointer++] = value;
}

void forth_push_float(forth_float_t value) {
    forth_push_int(*(forth_int_t*)&value);
}

forth_int_t forth_pop_int(void) {
    if (stack_pointer == 0) {
        printf("ERROR: Stack underflow\n");
        return 0;
    }
    return stack[--stack_pointer];
}

forth_float_t forth_pop_float(void) {
    forth_int_t temp = forth_pop_int();
    return *(forth_float_t*)&temp;
}

void forth_dup(void) {
    if (stack_pointer == 0) {
        printf("ERROR: Stack underflow in DUP\n");
        return;
    }
    forth_int_t value = stack[stack_pointer - 1];
    forth_push_int(value);
}

void forth_drop(void) {
    forth_pop_int();
}

void forth_swap(void) {
    if (stack_pointer < 2) {
        printf("ERROR: Insufficient items for SWAP\n");
        return;
    }
    forth_int_t a = forth_pop_int();
    forth_int_t b = forth_pop_int();
    forth_push_int(a);
    forth_push_int(b);
}

void forth_over(void) {
    if (stack_pointer < 2) {
        printf("ERROR: Insufficient items for OVER\n");
        return;
    }
    forth_int_t b = stack[stack_pointer - 1];
    forth_int_t a = stack[stack_pointer - 2];
    forth_push_int(a);
}

void forth_rot(void) {
    if (stack_pointer < 3) {
        printf("ERROR: Insufficient items for ROT\n");
        return;
    }
    forth_int_t c = forth_pop_int();
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(b);
    forth_push_int(c);
    forth_push_int(a);
}

// Math operations
void forth_add(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(a + b);
}

void forth_sub(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(a - b);
}

void forth_mul(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(a * b);
}

void forth_div(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    if (b == 0) {
        printf("ERROR: Division by zero\n");
        forth_push_int(0);
    } else {
        forth_push_int(a / b);
    }
}

void forth_mod(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    if (b == 0) {
        printf("ERROR: Division by zero in MOD\n");
        forth_push_int(0);
    } else {
        forth_push_int(a % b);
    }
}

void forth_negate(void) {
    forth_int_t a = forth_pop_int();
    forth_push_int(-a);
}

void forth_abs(void) {
    forth_int_t a = forth_pop_int();
    forth_push_int(a < 0 ? -a : a);
}

// Comparison operations
void forth_equal(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(a == b ? -1 : 0);
}

void forth_not_equal(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(a != b ? -1 : 0);
}

void forth_less_than(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(a < b ? -1 : 0);
}

void forth_greater_than(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(a > b ? -1 : 0);
}

void forth_less_equal(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(a <= b ? -1 : 0);
}

void forth_greater_equal(void) {
    forth_int_t b = forth_pop_int();
    forth_int_t a = forth_pop_int();
    forth_push_int(a >= b ? -1 : 0);
}

// Memory operations
void forth_fetch(void) {
    forth_int_t addr = forth_pop_int();
    forth_push_int(*(forth_int_t*)addr);
}

void forth_store(void) {
    forth_int_t addr = forth_pop_int();
    forth_int_t value = forth_pop_int();
    *(forth_int_t*)addr = value;
}

// I/O operations
void forth_emit(void) {
    forth_int_t c = forth_pop_int();
    putchar((int)c);
}

void forth_type(void) {
    forth_int_t len = forth_pop_int();
    forth_int_t addr = forth_pop_int();
    for (int i = 0; i < len; i++) {
        putchar(((char*)addr)[i]);
    }
}

// Forward declaration handling (simplified)
void forth_call_word(const char* word_name) {
    printf("ERROR: Unresolved word: %s\n", word_name);
}

#ifdef ESP32_PLATFORM
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void forth_gpio_init(void) {
    // GPIO initialization for ESP32
}

void forth_gpio_set(void) {
    forth_int_t value = forth_pop_int();
    forth_int_t pin = forth_pop_int();
    gpio_set_level((gpio_num_t)pin, value);
}

void forth_gpio_get(void) {
    forth_int_t pin = forth_pop_int();
    forth_push_int(gpio_get_level((gpio_num_t)pin));
}

void forth_delay(void) {
    forth_int_t ms = forth_pop_int();
    vTaskDelay(ms / portTICK_PERIOD_MS);
}
#endif

)";
}

// Helper methods
bool ForthCCodegen::isBuiltinWord(const std::string& word) const {
    static const std::unordered_set<std::string> builtins = {
        "+", "-", "*", "/", "MOD", "NEGATE", "ABS",
        "=", "<>", "<", ">", "<=", ">=",
        "DUP", "DROP", "SWAP", "OVER", "ROT",
        "!", "@", "EMIT", "TYPE"
    };
    return builtins.contains(word);
}

void ForthCCodegen::generateBuiltinWord(const std::string& word) {
    static const std::unordered_map<std::string, std::string> builtinMap = {
        {"+", "forth_add();"},
        {"-", "forth_sub();"},
        {"*", "forth_mul();"},
        {"/", "forth_div();"},
        {"MOD", "forth_mod();"},
        {"NEGATE", "forth_negate();"},
        {"ABS", "forth_abs();"},
        {"=", "forth_equal();"},
        {"<>", "forth_not_equal();"},
        {"<", "forth_less_than();"},
        {">", "forth_greater_than();"},
        {"<=", "forth_less_equal();"},
        {">=", "forth_greater_equal();"},
        {"DUP", "forth_dup();"},
        {"DROP", "forth_drop();"},
        {"SWAP", "forth_swap();"},
        {"OVER", "forth_over();"},
        {"ROT", "forth_rot();"},
        {"!", "forth_store();"},
        {"@", "forth_fetch();"},
        {"EMIT", "forth_emit();"},
        {"TYPE", "forth_type();"}
    };
    
    auto it = builtinMap.find(word);
    if (it != builtinMap.end()) {
        emitIndented(it->second);
    } else {
        addError("Unknown builtin word: " + word);
    }
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
            default: result += c; break;
        }
    }
    return result;
}

std::string ForthCCodegen::getCompleteCode() const {
    std::ostringstream complete;
    
    // Header includes
    complete << "#include \"forth_runtime.h\"\n\n";
    
    // Runtime implementation
    complete << functionsStream.str();
    
    // Generated word functions  
    complete << "\n// Generated FORTH word functions\n";
    complete << sourceStream.str();
    
    return complete.str();
}

bool ForthCCodegen::writeToFiles(const std::string& baseName) {
    return writeHeaderFile(baseName + ".h") && writeSourceFile(baseName + ".c");
}

bool ForthCCodegen::writeHeaderFile(const std::string& filename) {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            addError("Cannot create header file: " + filename);
            return false;
        }
        
        file << headerStream.str();
        return true;
    } catch (const std::exception& e) {
        addError("Failed to write header file: " + std::string(e.what()));
        return false;
    }
}

bool ForthCCodegen::writeSourceFile(const std::string& filename) {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            addError("Cannot create source file: " + filename);
            return false;
        }
        
        file << getCompleteCode();
        return true;
    } catch (const std::exception& e) {
        addError("Failed to write source file: " + std::string(e.what()));
        return false;
    }
}

void ForthCCodegen::optimizeGeneratedCode() {
    // Basic optimizations could be added here
    // For now, just remove unused functions if needed
    removeUnusedFunctions();
}

void ForthCCodegen::removeUnusedFunctions() {
    // Implementation for removing unused generated functions
    // This is a placeholder for future optimization
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

ForthCCodegen::CodeGenStats ForthCCodegen::getStatistics() const {
    CodeGenStats stats = {};
    
    // Count lines in generated code
    std::string code = getCompleteCode();
    stats.linesGenerated = std::count(code.begin(), code.end(), '\n');
    
    // Count generated functions
    stats.functionsGenerated = generatedWords.size();
    
    // Other statistics would be computed here
    stats.variablesGenerated = 0; // Would count variable declarations
    stats.mathOperations = 0;     // Would count math operations
    stats.controlStructures = 0;  // Would count control structures
    stats.usesFloatingPoint = false; // Would detect float usage
    stats.usesStrings = false;       // Would detect string usage
    stats.estimatedStackDepth = esp32Config.stackSize; // Use configured stack size
    
    return stats;
}

// ESP-IDF Integration Implementation (part of c_backend.cpp)

std::string ForthCCodegen::generateESPIDFComponent() const {
    return R"(idf_component_register(
    SRCS "forth_runtime.c" "forth_generated.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos
))";
}

std::string ForthCCodegen::generateMainComponent() const {
    return R"(idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES forth_compiler nvs_flash
))";
}

bool ForthCCodegen::writeESPIDFProject(const std::string& projectPath) {
    try {
        fs::create_directories(projectPath);
        fs::create_directories(projectPath + "/main");
        fs::create_directories(projectPath + "/components/forth_compiler/include");
        fs::create_directories(projectPath + "/components/forth_compiler/src");
        
        // Write project files using ESPIDFProjectGenerator
        ESPIDFProjectGenerator::generateProject(projectPath, moduleName, *this);
        
        return true;
    } catch (const std::exception& e) {
        addError("Failed to create ESP-IDF project: " + std::string(e.what()));
        return false;
    }
}

// ESPIDFProjectGenerator implementation
bool ESPIDFProjectGenerator::generateProject(const std::string& projectPath, 
                                           const std::string& projectName,
                                           const ForthCCodegen& codegen) {
    try {
        // Create directory structure
        fs::create_directories(projectPath + "/main");
        fs::create_directories(projectPath + "/components/forth_compiler/include");
        fs::create_directories(projectPath + "/components/forth_compiler/src");
        
        // Write root CMakeLists.txt
        {
            std::ofstream file(projectPath + "/CMakeLists.txt");
            file << generateRootCMakeLists(projectName);
        }
        
        // Write main CMakeLists.txt
        {
            std::ofstream file(projectPath + "/main/CMakeLists.txt");
            file << generateMainCMakeLists();
        }
        
        // Write component CMakeLists.txt
        {
            std::ofstream file(projectPath + "/components/forth_compiler/CMakeLists.txt");
            file << generateComponentCMakeLists("forth_compiler");
        }
        
        // Write main.cpp
        {
            std::ofstream file(projectPath + "/main/main.cpp");
            file << generateMainCpp(projectName);
        }
        
        // Write FORTH runtime files
        {
            std::ofstream headerFile(projectPath + "/components/forth_compiler/include/forth_runtime.h");
            headerFile << codegen.getHeaderCode();
            
            std::ofstream sourceFile(projectPath + "/components/forth_compiler/src/forth_runtime.c");
            sourceFile << codegen.getSourceCode();
        }
        
        // Write partition table
        {
            std::ofstream file(projectPath + "/partitions.csv");
            file << generatePartitionTable();
        }
        
        // Write basic sdkconfig
        {
            std::ofstream file(projectPath + "/sdkconfig.defaults");
            file << generateSDKConfig();
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::string ESPIDFProjectGenerator::generateRootCMakeLists(const std::string& projectName) {
    return R"(# ESP-IDF Project for FORTH Compiler
cmake_minimum_required(VERSION 3.16)

# Set the project name
set(PROJECT_NAME ")" + projectName + R"(")

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project
project(${PROJECT_NAME})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compile definitions for ESP32 platform
add_compile_definitions(ESP32_PLATFORM=1)

# Optional: Set partition table
set(PARTITION_TABLE_CSV_PATH "${CMAKE_SOURCE_DIR}/partitions.csv")
)";
}

std::string ESPIDFProjectGenerator::generateMainCMakeLists() {
    return R"(idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES 
        forth_compiler 
        driver
        nvs_flash
        wifi_provisioning
        esp_timer
        freertos
)";
}

std::string ESPIDFProjectGenerator::generateComponentCMakeLists(const std::string& componentName) {
    return R"(idf_component_register(
    SRCS 
        "src/forth_runtime.c"
        "src/forth_generated.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        driver
        freertos
        esp_timer
        nvs_flash
    PRIV_REQUIRES
        esp_common
)";
}

std::string ESPIDFProjectGenerator::generateMainCpp(const std::string& forthProgram) {
    return R"(#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

// Include generated FORTH runtime
#include "forth_runtime.h"

static const char* TAG = "FORTH_MAIN";

// FORTH task configuration
#define FORTH_TASK_STACK_SIZE 8192
#define FORTH_TASK_PRIORITY   5
#define FORTH_TASK_CORE       1

static void forth_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting FORTH program...");
    
    // Initialize FORTH runtime
    forth_stack_init();
    
    // ESP32 specific initialization
    #ifdef ESP32_PLATFORM
    forth_gpio_init();
    #endif
    
    // Run the generated FORTH program
    forth_program_main();
    
    ESP_LOGI(TAG, "FORTH program completed");
    
    // Clean up
    forth_stack_cleanup();
    
    // Task cleanup
    vTaskDelete(NULL);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "ESP32 FORTH Compiler Runtime");
    ESP_LOGI(TAG, "Program: )" + forthProgram + R"(");
    
    // Initialize NVS (required for WiFi and other features)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Create FORTH execution task
    BaseType_t task_created = xTaskCreatePinnedToCore(
        forth_task,              // Task function
        "forth_task",            // Task name
        FORTH_TASK_STACK_SIZE,   // Stack size
        NULL,                    // Parameters
        FORTH_TASK_PRIORITY,     // Priority
        NULL,                    // Task handle
        FORTH_TASK_CORE          // Core to run on
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create FORTH task");
        return;
    }
    
    ESP_LOGI(TAG, "FORTH task created successfully");
}
)";
}

std::string ESPIDFProjectGenerator::generatePartitionTable() {
    return R"(# ESP-IDF Partition Table for FORTH Compiler
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
)";
}

std::string ESPIDFProjectGenerator::generateSDKConfig() {
    return R"(# ESP32 FORTH Compiler Default Configuration

# Compiler optimizations
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE=y

# FreeRTOS configuration
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_ENABLE_TASK_SNAPSHOT=y

# ESP32-specific optimizations
CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y
CONFIG_ESP32_SPIRAM_SUPPORT=n

# Memory configuration
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_ESP_MAIN_TASK_AFFINITY_CPU1=y

# FORTH-specific settings
CONFIG_FORTH_STACK_SIZE=1024
CONFIG_FORTH_ENABLE_FLOAT=y
CONFIG_FORTH_ENABLE_GPIO=y
CONFIG_FORTH_ENABLE_TIMER=y

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_LOG_MAXIMUM_EQUALS_DEFAULT=y

# Console configuration
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_UART_BAUDRATE_115200=y
)";
}

// ForthCodegenFactory implementation
std::unique_ptr<ForthCCodegen> ForthCodegenFactory::create(TargetType target) {
    auto codegen = std::make_unique<ForthCCodegen>();
    
    switch (target) {
        case TargetType::ESP32_GENERIC:
            codegen->setTarget("esp32");
            codegen->setESP32Config(getESP32Config(target));
            break;
            
        case TargetType::ESP32_C3:
            codegen->setTarget("esp32c3");
            codegen->setESP32Config(getESP32Config(target));
            break;
            
        case TargetType::ESP32_S3:
            codegen->setTarget("esp32s3");
            codegen->setESP32Config(getESP32Config(target));
            break;
            
        case TargetType::NATIVE_LINUX:
            codegen->setTarget("linux");
            break;
            
        case TargetType::NATIVE_WINDOWS:
            codegen->setTarget("windows");
            break;
    }
    
    return codegen;
}

ForthCCodegen::ESP32Config ForthCodegenFactory::getESP32Config(TargetType target) {
    ForthCCodegen::ESP32Config config;
    
    switch (target) {
        case TargetType::ESP32_GENERIC:
            config.useTasking = true;
            config.useGPIO = true;
            config.useWiFi = false;
            config.useTimer = true;
            config.stackSize = 1024;
            config.priority = 5;
            break;
            
        case TargetType::ESP32_C3:
            config.useTasking = true;
            config.useGPIO = true;
            config.useWiFi = true;  // ESP32-C3 has good WiFi support
            config.useTimer = true;
            config.stackSize = 1024;
            config.priority = 5;
            break;
            
        case TargetType::ESP32_S3:
            config.useTasking = true;
            config.useGPIO = true;
            config.useWiFi = true;
            config.useTimer = true;
            config.stackSize = 2048;  // More memory available
            config.priority = 5;
            break;
            
        default:
            // Default configuration
            break;
    }
    
    return config;
}
