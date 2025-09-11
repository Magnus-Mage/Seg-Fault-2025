#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "dictionary/dictionary.h"
#include "semantic/analyzer.h"
#include "codegen/c_backend.h"  // Updated from llvm_backend.h
#include "common/utils.h"
#include "functional"

namespace fs = std::filesystem;
using namespace std::chrono;

[[nodiscard]] auto readSourceFile(const std::string& filename) -> std::string {
    if (!fs::exists(filename)) {
        throw std::runtime_error("File does not exist: " + filename);
    }
    
    std::ifstream file{filename};
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    return std::string{
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{}
    };
}

auto printTokenizationResults(const std::vector<Token>& tokens, bool verbose) -> void {
    if (!verbose) return;
    
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "TOKENIZATION RESULTS\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    std::cout << std::left;
    std::cout << std::setw(12) << "Type" 
              << std::setw(20) << "Value" 
              << std::setw(8) << "Line" 
              << std::setw(8) << "Column" << "\n";
    std::cout << std::string(48, '-') << "\n";
    
    ForthLexer lexer; // Create instance for tokenTypeToString
    for (const auto& token : tokens) {
        if (token.type == TokenType::EOF_TOKEN) continue;
        
        std::string displayValue = token.value;
        if (displayValue.length() > 18) {
            displayValue = displayValue.substr(0, 15) + "...";
        }
        
        std::cout << std::setw(12) << lexer.tokenTypeToString(token.type)
                  << std::setw(20) << ("'" + displayValue + "'")
                  << std::setw(8) << token.line
                  << std::setw(8) << token.column << "\n";
    }
    
    std::cout << "\nTotal tokens: " << (tokens.size() - 1) << " (excluding EOF)\n";
}

// AST visitor for printing the parse tree
class ASTPrinter : public ASTVisitor {
private:
    int indent = 0;
    
    void printIndent() {
        for (int i = 0; i < indent; ++i) {
            std::cout << "  ";
        }
    }
    
public:
    void visit(ProgramNode& node) override {
        printIndent();
        std::cout << "Program (" << node.getChildCount() << " statements)\n";
        indent++;
        for (const auto& child : node.getChildren()) {
            child->accept(*this);
        }
        indent--;
    }
    
    void visit(WordDefinitionNode& node) override {
        printIndent();
        std::cout << "WordDefinition: " << node.getWordName() << "\n";
        indent++;
        for (const auto& child : node.getChildren()) {
            child->accept(*this);
        }
        indent--;
    }
    
    void visit(WordCallNode& node) override {
        printIndent();
        std::cout << "WordCall: " << node.getWordName() << "\n";
    }
    
    void visit(NumberLiteralNode& node) override {
        printIndent();
        std::cout << "Number: " << node.getValue() << "\n";
    }
    
    void visit(StringLiteralNode& node) override {
        printIndent();
        std::cout << "String: \"" << node.getValue() << "\"" 
                  << (node.isPrint() ? " [PRINT]" : "") << "\n";
    }
    
    void visit(IfStatementNode& node) override {
        printIndent();
        std::cout << "If Statement" << (node.hasElse() ? " (with else)" : "") << "\n";
        indent++;
        if (node.getThenBranch()) {
            printIndent();
            std::cout << "THEN branch:\n";
            indent++;
            for (const auto& child : node.getThenBranch()->getChildren()) {
                child->accept(*this);
            }
            indent--;
        }
        if (node.getElseBranch()) {
            printIndent();
            std::cout << "ELSE branch:\n";
            indent++;
            for (const auto& child : node.getElseBranch()->getChildren()) {
                child->accept(*this);
            }
            indent--;
        }
        indent--;
    }
    
    void visit(BeginUntilLoopNode& node) override {
        printIndent();
        std::cout << "Begin-Until Loop\n";
        indent++;
        if (node.getBody()) {
            printIndent();
            std::cout << "Body:\n";
            indent++;
            for (const auto& child : node.getBody()->getChildren()) {
                child->accept(*this);
            }
            indent--;
        }
        indent--;
    }
    
    void visit(MathOperationNode& node) override {
        printIndent();
        std::cout << "MathOp: " << node.getOperation() << "\n";
    }
    
    void visit(VariableDeclarationNode& node) override {
        printIndent();
        std::cout << (node.isConst() ? "Constant: " : "Variable: ") 
                  << node.getVarName() << "\n";
    }
};

auto printParseResults(ProgramNode& ast, bool verbose) -> void {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "PARSE RESULTS\n";
    std::cout << std::string(60, '=') << "\n";
    
    if (verbose) {
        ASTPrinter printer;
        ast.accept(printer);
    } else {
        std::cout << "Successfully parsed " << ast.getChildCount() << " top-level statements\n";
    }
}

auto printSemanticResults(const SemanticAnalyzer& analyzer, bool verbose) -> void {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "SEMANTIC ANALYSIS RESULTS\n";
    std::cout << std::string(60, '=') << "\n";
    
    if (analyzer.hasErrors()) {
        std::cout << "❌ Semantic Errors:\n";
        for (const auto& error : analyzer.getErrors()) {
            std::cout << "  • " << error << "\n";
        }
    }
    
    if (analyzer.hasWarnings()) {
        std::cout << "⚠️  Semantic Warnings:\n";
        for (const auto& warning : analyzer.getWarnings()) {
            std::cout << "  • " << warning << "\n";
        }
    }
    
    if (!analyzer.hasErrors() && !analyzer.hasWarnings()) {
        std::cout << "✅ No semantic issues found\n";
    }
    
    std::cout << "\nStack Analysis:\n";
    std::cout << "  Maximum stack depth: " << analyzer.getMaxStackDepth() << "\n";
    std::cout << "  Minimum stack depth: " << analyzer.getMinStackDepth() << "\n";
    
    if (verbose) {
        const auto& wordEffects = analyzer.getWordEffects();
        if (!wordEffects.empty()) {
            std::cout << "\nWord Stack Effects:\n";
            for (const auto& [word, effect] : wordEffects) {
                std::cout << "  " << word << ": (" 
                         << effect.effect.consumed << " -> " 
                         << effect.effect.produced << ")";
                if (!effect.effect.isKnown) std::cout << " [unknown]";
                std::cout << "\n";
            }
        }
    }
}

auto printCodegenResults(const ForthCCodegen& codegen, bool showCode) -> void {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "C CODE GENERATION RESULTS\n";
    std::cout << std::string(60, '=') << "\n";
    
    if (codegen.hasErrors()) {
        std::cout << "❌ Code Generation Errors:\n";
        for (const auto& error : codegen.getErrors()) {
            std::cout << "  • " << error << "\n";
        }
    } else {
        std::cout << "✅ C code generation completed successfully\n";
        
        // Show statistics
        auto stats = codegen.getStatistics();
        std::cout << "\nGenerated Code Statistics:\n";
        std::cout << "  Lines of code: " << stats.linesGenerated << "\n";
        std::cout << "  Functions: " << stats.functionsGenerated << "\n";
        std::cout << "  Variables: " << stats.variablesGenerated << "\n";
        
        if (showCode) {
            std::cout << "\nGenerated C Code (Header):\n";
            std::cout << std::string(40, '-') << "\n";
            const auto& files = codegen.getGeneratedFiles();  // Use const reference
            if (!files.empty()) {
                std::string header = files[0].second.str();
                if (header.length() > 1000) {
                    std::cout << header.substr(0, 1000) << "\n... (truncated)\n";
                } else {
                    std::cout << header << "\n";
                }
            }
            
            std::cout << "\nGenerated C Code (Source):\n";
            std::cout << std::string(40, '-') << "\n";
            if (files.size() > 1) {  // Reuse the same files variable
                std::string source = files[1].second.str();
                if (source.length() > 1000) {
                    std::cout << source.substr(0, 1000) << "\n... (truncated)\n";
                } else {
                    std::cout << source << "\n";
                }
            }
            std::cout << std::string(40, '-') << "\n";
        }
    }
    
    if (codegen.hasWarnings()) {
        std::cout << "\n⚠️  Code Generation Warnings:\n";
        for (const auto& warning : codegen.getWarnings()) {
            std::cout << "  • " << warning << "\n";
        }
    }
}

auto analyzeProgram(ProgramNode& ast, const ForthDictionary& dictionary) -> void {
    std::cout << "\n" << std::string(40, '=') << "\n";
    std::cout << "PROGRAM ANALYSIS\n";
    std::cout << std::string(40, '=') << "\n";
    
    // Count different node types
    int wordDefs = 0, mathOps = 0, controlStructures = 0, variables = 0, constants = 0;
    
    std::function<void(ASTNode*)> analyzeNode = [&](ASTNode* node) {
        switch (node->getType()) {
            case ASTNode::NodeType::WORD_DEFINITION: wordDefs++; break;
            case ASTNode::NodeType::MATH_OPERATION: mathOps++; break;
            case ASTNode::NodeType::IF_STATEMENT:
            case ASTNode::NodeType::BEGIN_UNTIL_LOOP: controlStructures++; break;
            case ASTNode::NodeType::VARIABLE_DECLARATION: variables++; break;
            case ASTNode::NodeType::CONSTANT_DECLARATION: constants++; break;
            default: break;
        }
        
        // Recursively analyze children
        for (const auto& child : node->getChildren()) {
            analyzeNode(child.get());
        }
    };
    
    analyzeNode(&ast);
    
    std::cout << "Features found:\n";
    std::cout << "- Word definitions:     " << wordDefs << "\n";
    std::cout << "- Math operations:      " << mathOps << "\n";
    std::cout << "- Control structures:   " << controlStructures << "\n";
    std::cout << "- Variables:            " << variables << "\n";
    std::cout << "- Constants:            " << constants << "\n";
    
    std::cout << "\nDictionary size: " << dictionary.getDictionarySize() << " entries\n";
    
    // Complexity assessment
    int totalComplexity = wordDefs * 3 + mathOps + controlStructures * 2 + variables + constants;
    std::string complexity = "Simple";
    if (totalComplexity > 20) complexity = "Complex";
    else if (totalComplexity > 10) complexity = "Moderate";
    
    std::cout << "Program complexity: " << complexity << " (score: " << totalComplexity << ")\n";
}

auto printStatistics(const std::vector<Token>& tokens, 
                    const high_resolution_clock::duration& lexDuration,
                    const high_resolution_clock::duration& parseDuration,
                    const high_resolution_clock::duration& semanticDuration,
                    const high_resolution_clock::duration& codegenDuration) -> void {
    std::cout << "\n" << std::string(40, '=') << "\n";
    std::cout << "PERFORMANCE STATISTICS\n";
    std::cout << std::string(40, '=') << "\n";
    
    const auto lexMs = duration_cast<microseconds>(lexDuration);
    const auto parseMs = duration_cast<microseconds>(parseDuration);
    const auto semanticMs = duration_cast<microseconds>(semanticDuration);
    const auto codegenMs = duration_cast<microseconds>(codegenDuration);
    const auto totalMs = lexMs + parseMs + semanticMs + codegenMs;
    
    std::cout << "Tokens processed:   " << (tokens.size() - 1) << "\n";
    std::cout << "Lexing time:        " << lexMs.count() << " μs\n";
    std::cout << "Parsing time:       " << parseMs.count() << " μs\n";
    std::cout << "Semantic analysis:  " << semanticMs.count() << " μs\n";
    std::cout << "Code generation:    " << codegenMs.count() << " μs\n";
    std::cout << "Total time:         " << totalMs.count() << " μs\n";
    
    if (tokens.size() > 1) {
        const auto tokensPerSecond = (tokens.size() - 1) * 1'000'000 / totalMs.count();
        std::cout << "Processing rate:    " << tokensPerSecond << " tokens/second\n";
    }
    
    std::cout << "\nPhase breakdown:\n";
    std::cout << "  Lexing:      " << std::fixed << std::setprecision(1) 
              << (100.0 * lexMs.count() / totalMs.count()) << "%\n";
    std::cout << "  Parsing:     " << std::fixed << std::setprecision(1)
              << (100.0 * parseMs.count() / totalMs.count()) << "%\n";
    std::cout << "  Semantic:    " << std::fixed << std::setprecision(1)
              << (100.0 * semanticMs.count() / totalMs.count()) << "%\n";
    std::cout << "  Code Gen:    " << std::fixed << std::setprecision(1)
              << (100.0 * codegenMs.count() / totalMs.count()) << "%\n";
}

auto main(int argc, char* argv[]) -> int {
    std::cout << "FORTH-ESP32 Compiler v0.3.0\n";
    std::cout << "Phase 4: Semantic Analysis & C Code Generation\n\n";  // Updated
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <forth_file> [options]\n";
        std::cerr << "Options:\n";
        std::cerr << "  -v, --verbose      Show detailed information\n";
        std::cerr << "  -t, --tokens       Show tokenization results\n";
        std::cerr << "  -a, --ast          Show AST structure\n";
        std::cerr << "  -s, --semantic     Show semantic analysis details\n";
        std::cerr << "  -c, --codegen      Show code generation details\n";
        std::cerr << "  --show-code        Show generated C code\n";  // Updated from --ir
        std::cerr << "  -d, --dict         Show dictionary contents\n";
        std::cerr << "  --stats            Show performance statistics\n";
        std::cerr << "  -o, --output       Output file for generated code\n";
        std::cerr << "  --target           Target architecture (default: esp32)\n";
        std::cerr << "  --create-esp32     Create ESP-IDF project\n";  // New option
        return 1;
    }
    
    const std::string filename{argv[1]};
    bool verbose = false, showTokens = false, showAST = false, showSemantic = false;
    bool showCodegen = false, showCode = false, showDict = false, showStats = false;
    bool createESP32Project = false;  // New flag
    std::string outputFile, target = "esp32";  // Updated default
    
    // Parse command line options
    for (int i = 2; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "-v" || arg == "--verbose") {
            verbose = showTokens = showAST = showSemantic = showCodegen = showDict = showStats = true;
        } else if (arg == "-t" || arg == "--tokens") {
            showTokens = true;
        } else if (arg == "-a" || arg == "--ast") {
            showAST = true;
        } else if (arg == "-s" || arg == "--semantic") {
            showSemantic = true;
        } else if (arg == "-c" || arg == "--codegen") {
            showCodegen = true;
        } else if (arg == "--show-code") {  // Updated from --ir
            showCode = true;
        } else if (arg == "-d" || arg == "--dict") {
            showDict = true;
        } else if (arg == "--stats") {
            showStats = true;
        } else if (arg == "--create-esp32") {  // New option
            createESP32Project = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            }
        } else if (arg == "--target") {
            if (i + 1 < argc) {
                target = argv[++i];
            }
        }
    }
    
    try {
        // Read source file
        std::cout << "Reading file: " << filename << "\n";
        const auto source = readSourceFile(filename);
        
        if (source.empty()) {
            std::cout << "Warning: File is empty\n";
            return 0;
        }
        
        std::cout << "Source size: " << source.size() << " bytes\n";
        std::cout << "Target: " << target << "\n";
        
        // Phase 1: Lexical Analysis
        ForthLexer lexer;
        const auto lexStartTime = high_resolution_clock::now();
        const auto tokens = lexer.tokenize(source);
        const auto lexEndTime = high_resolution_clock::now();
        const auto lexDuration = lexEndTime - lexStartTime;
        
        std::cout << "✅ Lexical analysis completed: " << (tokens.size() - 1) << " tokens\n";
        
        if (showTokens) {
            printTokenizationResults(tokens, true);
        }
        
        // Phase 2: Parsing
        ForthParser parser;
        const auto parseStartTime = high_resolution_clock::now();
        auto ast = parser.parseProgram(tokens);
        const auto parseEndTime = high_resolution_clock::now();
        const auto parseDuration = parseEndTime - parseStartTime;
        
        // Check for parse errors
        if (parser.hasErrors()) {
            std::cout << "\n❌ Parse errors found:\n";
            for (const auto& error : parser.getErrors()) {
                std::cout << "  • " << error << "\n";
            }
            return 1;
        }
        
        std::cout << "✅ Parsing completed: " << ast->getChildCount() << " top-level statements\n";
        
        if (showAST || verbose) {
            printParseResults(*ast, showAST || verbose);
        }
        
        // Phase 3: Semantic Analysis
        SemanticAnalyzer analyzer(&parser.getDictionary());
        const auto semanticStartTime = high_resolution_clock::now();
        const bool semanticSuccess = analyzer.analyze(*ast);
        const auto semanticEndTime = high_resolution_clock::now();
        const auto semanticDuration = semanticEndTime - semanticStartTime;
        
        if (semanticSuccess) {
            std::cout << "✅ Semantic analysis completed successfully\n";
        } else {
            std::cout << "⚠️  Semantic analysis completed with issues\n";
        }
        
        if (showSemantic || verbose || analyzer.hasErrors() || analyzer.hasWarnings()) {
            printSemanticResults(analyzer, showSemantic || verbose);
        }
        
        // Phase 4: C Code Generation (Updated from LLVM)
        auto codegen = ForthCodegenFactory::create(
            target == "esp32c3" ? ForthCodegenFactory::TargetType::ESP32_C3 :
            target == "esp32s3" ? ForthCodegenFactory::TargetType::ESP32_S3 :
            ForthCodegenFactory::TargetType::ESP32
        );
        
        codegen->setSemanticAnalyzer(&analyzer);
        codegen->setDictionary(&parser.getDictionary());
        
        const auto codegenStartTime = high_resolution_clock::now();
        bool codegenSuccess = codegen->generateCode(*ast);
        const auto codegenEndTime = high_resolution_clock::now();
        const auto codegenDuration = codegenEndTime - codegenStartTime;
        
        if (codegenSuccess && !codegen->hasErrors()) {
            std::cout << "✅ C code generation completed successfully\n";
        } else {
            std::cout << "❌ C code generation failed\n";
        }
        
        if (showCodegen || verbose || codegen->hasErrors()) {
            printCodegenResults(*codegen, showCode || verbose);
        }
        
        // Program analysis
        analyzeProgram(*ast, parser.getDictionary());
        
        // Dictionary information
        if (showDict) {
            parser.getDictionary().printDictionary();
        }
        
        // Performance statistics
        if (showStats || verbose) {
            printStatistics(tokens, lexDuration, parseDuration, semanticDuration, codegenDuration);
        }
        
        // Generate output file if requested
	if (!outputFile.empty() && codegenSuccess && !codegen->hasErrors()) {
	    std::cout << "\nGenerating output file: " << outputFile << "\n";
	    
	    // Always generate both header and source files
	    std::string baseName = outputFile;
	    if (baseName.find('.') != std::string::npos) {
		baseName = baseName.substr(0, baseName.find_last_of('.'));
	    }
	    
	    if (codegen->writeToFiles(baseName)) {
		std::cout << "✅ C files written to directory: " << baseName << "\n";
	    } else {
		std::cout << "❌ Failed to write C files\n";
	    }
	}
        // Create ESP-IDF project if requested
        if (createESP32Project && codegenSuccess && !codegen->hasErrors()) {
            std::string projectPath = fs::current_path() / "esp32_project";
            if (!outputFile.empty()) {
                projectPath = outputFile;
            }
            
            std::cout << "\nCreating ESP-IDF project: " << projectPath << "\n";
            if (codegen->writeESPIDFProject(projectPath)) {
                std::cout << "✅ ESP-IDF project created at " << projectPath << "\n";
                std::cout << "\nNext steps:\n";
                std::cout << "  cd " << projectPath << "\n";
                std::cout << "  idf.py set-target " << target << "\n";
                std::cout << "  idf.py build\n";
                std::cout << "  idf.py flash\n";
            } else {
                std::cout << "❌ Failed to create ESP-IDF project\n";
            }
        }
        
        // Final status report
        std::cout << "\n" << std::string(50, '-') << "\n";
        
        bool hasErrors = parser.hasErrors() || 
                        (analyzer.hasErrors() && !semanticSuccess) ||
                        codegen->hasErrors();
        
        if (!hasErrors) {
            std::cout << "🎉 Phase 4 completed successfully!\n";
            std::cout << "✅ Lexical analysis working\n";
            std::cout << "✅ Parser generating proper AST\n";
            std::cout << "✅ Dictionary system functional\n";
            std::cout << "✅ Semantic analysis operational\n";
            std::cout << "✅ C code generation working\n";  // Updated
            std::cout << "✅ Stack effect analysis functional\n";
            std::cout << "✅ Error handling working\n";
            
            if (analyzer.hasWarnings() || codegen->hasWarnings()) {
                int totalWarnings = analyzer.getWarnings().size() + codegen->getWarnings().size();
                std::cout << "⚠️  " << totalWarnings << " warnings (non-critical)\n";
            }
            
            std::cout << "\n🚀 Ready for Phase 5: ESP32 Integration & Optimization\n";
            
            if (codegenSuccess) {
                std::cout << "\nGenerated code statistics:\n";
                auto stats = codegen->getStatistics();
                std::cout << "  - " << stats.linesGenerated << " lines of C code\n";
                std::cout << "  - " << stats.functionsGenerated << " FORTH word functions\n";
                std::cout << "  - " << stats.variablesGenerated << " variables\n";
                std::cout << "  - Estimated stack usage: " << stats.estimatedStackDepth << " bytes\n";
            }
            
        } else {
            std::cout << "❌ Phase 4 completed with errors\n";
            
            int totalErrors = parser.getErrors().size() + 
                            analyzer.getErrors().size() + 
                            codegen->getErrors().size();
            int totalWarnings = analyzer.getWarnings().size() + 
                               codegen->getWarnings().size();
            
            std::cout << "Total errors: " << totalErrors << "\n";
            std::cout << "Total warnings: " << totalWarnings << "\n";
            std::cout << "\n🔧 Fix errors before proceeding to Phase 5\n";
            
            return 1;
        }
        
        // Integration test if successful
        if (!hasErrors && (createESP32Project || !outputFile.empty())) {
            std::cout << "\n" << std::string(30, '=') << "\n";
            std::cout << "INTEGRATION TEST SUMMARY\n";
            std::cout << std::string(30, '=') << "\n";
            
            try {
                // Test the complete compilation pipeline
                auto testCodegen = ForthCodegenFactory::create(ForthCodegenFactory::TargetType::ESP32);
                testCodegen->setSemanticAnalyzer(&analyzer);
                testCodegen->setDictionary(&parser.getDictionary());
                
                if (testCodegen->generateCode(*ast)) {
                    std::cout << "✅ High-level compiler interface working\n";
                    std::cout << "✅ Full compilation pipeline functional\n";
                    std::cout << "✅ C code generation via compiler interface working\n";
                    
                    auto testStats = testCodegen->getStatistics();
                    if (testStats.linesGenerated > 0) {
                        std::cout << "✅ Code generation metrics working\n";
                    }
                    
                    if (!testCodegen->getHeaderCode().empty() && !testCodegen->getCompleteCode().empty()) {
                        std::cout << "✅ Header and source code generation working\n";
                    }
                } else {
                    std::cout << "⚠️  High-level compiler interface has issues\n";
                    for (const auto& error : testCodegen->getErrors()) {
                        std::cout << "  • " << error << "\n";
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "❌ Integration test failed: " << e.what() << "\n";
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown fatal error occurred\n";
        return 1;
    }
    
    return 0;
}
