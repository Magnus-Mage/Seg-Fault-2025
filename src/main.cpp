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
#include "common/utils.h"

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
    
    for (const auto& token : tokens) {
        if (token.type == TokenType::EOF_TOKEN) continue;
        
        std::string displayValue = token.value;
        if (displayValue.length() > 18) {
            displayValue = displayValue.substr(0, 15) + "...";
        }
        
        std::cout << std::setw(12) << ForthLexer{}.tokenTypeToString(token.type)
                  << std::setw(20) << ("'" + displayValue + "'")
                  << std::setw(8) << token.line
                  << std::setw(8) << token.column << "\n";
    }
    
    std::cout << "\nTotal tokens: " << (tokens.size() - 1) << " (excluding EOF)\n"; // -1 for EOF
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
                    const high_resolution_clock::duration& parseDuration) -> void {
    std::cout << "\n" << std::string(40, '=') << "\n";
    std::cout << "PERFORMANCE STATISTICS\n";
    std::cout << std::string(40, '=') << "\n";
    
    const auto lexMs = duration_cast<microseconds>(lexDuration);
    const auto parseMs = duration_cast<microseconds>(parseDuration);
    const auto totalMs = lexMs + parseMs;
    
    std::cout << "Tokens processed:   " << (tokens.size() - 1) << "\n";
    std::cout << "Lexing time:        " << lexMs.count() << " μs\n";
    std::cout << "Parsing time:       " << parseMs.count() << " μs\n";
    std::cout << "Total time:         " << totalMs.count() << " μs\n";
    
    if (tokens.size() > 1) {
        const auto tokensPerSecond = (tokens.size() - 1) * 1'000'000 / totalMs.count();
        std::cout << "Processing rate:    " << tokensPerSecond << " tokens/second\n";
    }
}

auto main(int argc, char* argv[]) -> int {
    std::cout << "FORTH-ESP32 Compiler v0.1.0\n";
    std::cout << "Phase 3: Parser & AST Generation\n\n";
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <forth_file> [options]\n";
        std::cerr << "Options:\n";
        std::cerr << "  -v, --verbose    Show detailed information\n";
        std::cerr << "  -t, --tokens     Show tokenization results\n";
        std::cerr << "  -a, --ast        Show AST structure\n";
        std::cerr << "  -d, --dict       Show dictionary contents\n";
        std::cerr << "  -s, --stats      Show performance statistics\n";
        return 1;
    }
    
    const std::string filename{argv[1]};
    bool verbose = false, showTokens = false, showAST = false, showDict = false, showStats = false;
    
    // Parse command line options
    for (int i = 2; i < argc; ++i) {
        const std::string arg{argv[i]};
        if (arg == "-v" || arg == "--verbose") {
            verbose = showTokens = showAST = showDict = showStats = true;
        } else if (arg == "-t" || arg == "--tokens") {
            showTokens = true;
        } else if (arg == "-a" || arg == "--ast") {
            showAST = true;
        } else if (arg == "-d" || arg == "--dict") {
            showDict = true;
        } else if (arg == "-s" || arg == "--stats") {
            showStats = true;
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
        
        // Program analysis
        analyzeProgram(*ast, parser.getDictionary());
        
        // Dictionary information
        if (showDict) {
            parser.getDictionary().printDictionary();
        }
        
        // Performance statistics
        if (showStats || verbose) {
            printStatistics(tokens, lexDuration, parseDuration);
        }
        
        std::cout << "\n" << std::string(50, '-') << "\n";
        std::cout << "✅ Phase 3 completed successfully!\n";
        std::cout << "✅ AST generation working\n";
        std::cout << "✅ Dictionary system functional\n";
        std::cout << "✅ Error handling operational\n";
        std::cout << "\n🚀 Ready for Phase 4: Semantic Analysis & Code Generation\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
