#include "../test_framework.h"
#include "codegen/cpp_codegen.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "dictionary/dictionary.h"

// Test the C++ code generator with a simple FORTH program
auto testBasicCodeGeneration() -> bool {
    // Create a simple FORTH program
    const std::string forthCode = R"(
        : SQUARE DUP * ;
        : TEST-PROGRAM 
            5 SQUARE . CR
        ;
    )";
    
    try {
        // Tokenize
        ForthLexer lexer;
        auto tokens = lexer.tokenize(forthCode);
        
        // Parse
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) {
            std::cout << "Parser errors:\n";
            for (const auto& error : parser.getErrors()) {
                std::cout << "  " << error << "\n";
            }
            return false;
        }
        
        // Generate C++ code
        CppCodeGenerator::GenerationConfig config;
        config.includeDebugInfo = true;
        config.generateESP32Specific = true;
        
        auto dictionary = DictionaryFactory::create(DictionaryFactory::Configuration::ESP32_OPTIMIZED);
        CppCodeGenerator codegen(std::move(dictionary), config);
        
        auto generatedCode = codegen.generateCode(*ast);
        
        // Verify generated code contains expected elements
        bool hasStackClass = generatedCode.headerIncludes.find("class ForthStack") != std::string::npos;
        bool hasForthProgram = generatedCode.headerIncludes.find("class ForthProgram") != std::string::npos;
        bool hasSquareMethod = generatedCode.methodImplementations.find("SQUARE()") != std::string::npos;
        bool hasESP32Main = generatedCode.mainFunction.find("app_main") != std::string::npos;
        
        std::cout << "Generated code validation:\n";
        std::cout << "  Stack class: " << (hasStackClass ? "✓" : "✗") << "\n";
        std::cout << "  FORTH program class: " << (hasForthProgram ? "✓" : "✗") << "\n";
        std::cout << "  SQUARE method: " << (hasSquareMethod ? "✓" : "✗") << "\n";
        std::cout << "  ESP32 main: " << (hasESP32Main ? "✓" : "✗") << "\n";
        
        // Print a sample of the generated code for debugging
        std::cout << "\n--- Generated Header (first 500 chars) ---\n";
        std::string header = generatedCode.headerIncludes;
        if (header.length() > 500) {
            header = header.substr(0, 500) + "...";
        }
        std::cout << header << "\n";
        
        return hasStackClass && hasForthProgram && hasSquareMethod && hasESP32Main;
        
    } catch (const std::exception& e) {
        std::cout << "Exception during code generation: " << e.what() << "\n";
        return false;
    }
}

// Test ESP-IDF project structure generation
auto testESP32ProjectGeneration() -> bool {
    try {
        // Create minimal program
        const std::string forthCode = ": BLINK 1 GPIO-SET 500 DELAY-MS 0 GPIO-SET 500 DELAY-MS ;";
        
        ForthLexer lexer;
        auto tokens = lexer.tokenize(forthCode);
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        auto dictionary = DictionaryFactory::create(DictionaryFactory::Configuration::ESP32_OPTIMIZED);
        CppCodeGenerator codegen(std::move(dictionary));
        
        auto generatedCode = codegen.generateCode(*ast);
        
        // Test ESP-IDF project generation
        auto projectStructure = ESPIDFGenerator::generateProjectStructure(generatedCode, "blink_test");
        
        bool hasCMakeLists = !projectStructure.cmakeListsContent.empty();
        bool hasMainCpp = !projectStructure.mainCppContent.empty();
        bool hasESPComponents = generatedCode.requiredESPComponents.size() > 0;
        
        std::cout << "ESP-IDF project structure validation:\n";
        std::cout << "  CMakeLists.txt: " << (hasCMakeLists ? "✓" : "✗") << "\n";
        std::cout << "  main.cpp: " << (hasMainCpp ? "✓" : "✗") << "\n";
        std::cout << "  ESP components: " << (hasESPComponents ? "✓" : "✗") << "\n";
        std::cout << "  Components count: " << generatedCode.requiredESPComponents.size() << "\n";
        
        return hasCMakeLists && hasMainCpp && hasESPComponents;
        
    } catch (const std::exception& e) {
        std::cout << "Exception during ESP32 project generation: " << e.what() << "\n";
        return false;
    }
}

// Test stack effect analysis during code generation
auto testStackEffectAnalysis() -> bool {
    const std::string forthCode = R"(
        : STACK-TEST 
            1 2 +     \ Should result in stack depth 1
            DUP       \ Should result in stack depth 2
            SWAP      \ Should still be stack depth 2
            DROP      \ Should result in stack depth 1
        ;
    )";
    
    try {
        ForthLexer lexer;
        auto tokens = lexer.tokenize(forthCode);
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        CppCodeGenerator::GenerationConfig config;
        config.includeStackChecks = true;
        config.includeDebugInfo = true;
        
        auto dictionary = DictionaryFactory::create(DictionaryFactory::Configuration::STANDARD);
        CppCodeGenerator codegen(std::move(dictionary), config);
        
        auto generatedCode = codegen.generateCode(*ast);
        
        // Check if stack checks are generated
        bool hasStackChecks = generatedCode.methodImplementations.find("forth_stack.size()") != std::string::npos;
        bool hasStackUnderflowCheck = generatedCode.methodImplementations.find("Stack underflow") != std::string::npos;
        
        std::cout << "Stack effect analysis validation:\n";
        std::cout << "  Stack size checks: " << (hasStackChecks ? "✓" : "✗") << "\n";
        std::cout << "  Underflow protection: " << (hasStackUnderflowCheck ? "✓" : "✗") << "\n";
        
        return hasStackChecks && hasStackUnderflowCheck;
        
    } catch (const std::exception& e) {
        std::cout << "Exception during stack effect analysis: " << e.what() << "\n";
        return false;
    }
}

// Test control flow code generation
auto testControlFlowGeneration() -> bool {
    const std::string forthCode = R"(
        : CONDITIONAL-TEST
            5 0 > IF
                ." Positive"
            ELSE
                ." Not positive"
            THEN
        ;
        
        : LOOP-TEST
            10 BEGIN
                DUP .
                1 -
                DUP 0 =
            UNTIL
            DROP
        ;
    )";
    
    try {
        ForthLexer lexer;
        auto tokens = lexer.tokenize(forthCode);
        
        ForthParser parser;
        auto ast = parser.parseProgram(tokens);
        
        auto dictionary = DictionaryFactory::create(DictionaryFactory::Configuration::STANDARD);
        CppCodeGenerator codegen(std::move(dictionary));
        
        auto generatedCode = codegen.generateCode(*ast);
        
        bool hasIfElse = generatedCode.methodImplementations.find("if (") != std::string::npos &&
                        generatedCode.methodImplementations.find("} else {") != std::string::npos;
        bool hasDoWhile = generatedCode.methodImplementations.find("do {") != std::string::npos &&
                         generatedCode.methodImplementations.find("} while") != std::string::npos;
        
        std::cout << "Control flow generation validation:\n";
        std::cout << "  IF-ELSE structure: " << (hasIfElse ? "✓" : "✗") << "\n";
        std::cout << "  BEGIN-UNTIL loop: " << (hasDoWhile ? "✓" : "✗") << "\n";
        
        return hasIfElse && hasDoWhile;
        
    } catch (const std::exception& e) {
        std::cout << "Exception during control flow generation: " << e.what() << "\n";
        return false;
    }
}

// Register all code generation tests
auto registerCodegenTests(TestRunner& runner) -> void {
    runner.addTest("Basic C++ Code Generation", testBasicCodeGeneration);
    runner.addTest("ESP32 Project Generation", testESP32ProjectGeneration);
    runner.addTest("Stack Effect Analysis", testStackEffectAnalysis);
    runner.addTest("Control Flow Generation", testControlFlowGeneration);
}

// Namespace implementation for ESP-IDF project generation
namespace ESPIDFGenerator {
    auto generateProjectStructure(const CppCodeGenerator::GeneratedCode& code,
                                 const std::string& projectName) -> ProjectStructure {
        ProjectStructure structure;
        
        // Generate main.cpp content
        std::ostringstream mainCpp;
        mainCpp << code.headerIncludes;
        mainCpp << code.methodImplementations;  
        mainCpp << code.mainFunction;
        structure.mainCppContent = mainCpp.str();
        
        // Generate CMakeLists.txt
        structure.cmakeListsContent = code.cmakeListsContent;
        
        // Generate basic partition table
        structure.partitionsContent = R"(# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
)";
        
        // Generate sdkconfig defaults
        structure.menuConfigContent = R"(CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
)";
        
        return structure;
    }
    
    auto generateComponentCMake(const std::vector<std::string>& components) -> std::string {
        std::ostringstream cmake;
        cmake << "idf_component_register(\n";
        cmake << "    SRCS \"main.cpp\"\n";
        cmake << "    INCLUDE_DIRS \".\"\n";
        cmake << "    REQUIRES";
        
        for (const auto& component : components) {
            cmake << " " << component;
        }
        
        cmake << "\n)\n";
        return cmake.str();
    }
    
    auto generateSDKConfig(bool includeDebug) -> std::string {
        std::ostringstream config;
        
        config << "# Generated sdkconfig for FORTH-ESP32 project\n";
        config << "CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y\n";
        config << "CONFIG_FREERTOS_HZ=1000\n";
        
        if (includeDebug) {
            config << "CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y\n";
            config << "CONFIG_LOG_MAXIMUM_LEVEL=5\n";
        } else {
            config << "CONFIG_LOG_DEFAULT_LEVEL_INFO=y\n";
        }
        
        return config.str();
    }
}
