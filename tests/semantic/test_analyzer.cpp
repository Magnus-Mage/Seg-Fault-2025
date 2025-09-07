#include "../test_framework.h"
#include "semantic/analyzer.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include <memory>

class SemanticTestFixture {
public:
    ForthLexer lexer;
    ForthParser parser;
    SemanticAnalyzer analyzer;
    
    auto analyzeCode(const std::string& code) -> std::unique_ptr<ProgramNode> {
        auto tokens = lexer.tokenize(code);
        auto ast = parser.parseProgram(tokens);
        
        if (parser.hasErrors()) {
            throw std::runtime_error("Parser errors: " + 
                (parser.getErrors().empty() ? "unknown" : parser.getErrors()[0]));
        }
        
        analyzer.analyze(*ast);
        return ast;
    }
    
    auto getStackEffect(const std::string& wordName) -> ASTNode::StackEffect {
        return analyzer.getStackEffect(wordName);
    }
    
    auto hasError() -> bool {
        return analyzer.hasErrors();
    }
    
    auto getErrors() -> const std::vector<std::string>& {
        return analyzer.getErrors();
    }
};

auto registerSemanticTests(TestRunner& runner) -> void {
    
    // Basic stack effect analysis
    runner.addTest("semantic_number_literal_effect", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode("42");
        
        // Number literals should push one value
        auto numberNode = dynamic_cast<NumberLiteralNode*>(ast->getChild(0));
        if (!numberNode) return false;
        
        auto effect = numberNode->getStackEffect();
        return effect.consumed == 0 && effect.produced == 1 && effect.isKnown;
    });
    
    runner.addTest("semantic_basic_math_effects", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode("10 20 +");
        
        if (!ast || ast->getChildCount() != 3) return false;
        
        // Check math operation stack effect
        auto mathNode = dynamic_cast<MathOperationNode*>(ast->getChild(2));
        if (!mathNode) return false;
        
        auto effect = mathNode->getStackEffect();
        return effect.consumed == 2 && effect.produced == 1 && effect.isKnown;
    });
    
    runner.addTest("semantic_word_definition_analysis", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode(": DOUBLE DUP + ;");
        
        if (fixture.hasError()) return false;
        
        auto effect = fixture.getStackEffect("DOUBLE");
        // DOUBLE should consume 1 and produce 1 (duplicate then add)
        return effect.consumed == 1 && effect.produced == 1 && effect.isKnown;
    });
    
    runner.addTest("semantic_stack_underflow_detection", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode("+"); // Add without operands
        
        // Should detect stack underflow
        return fixture.hasError();
    });
    
    runner.addTest("semantic_complex_word_analysis", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode(": SQUARE DUP * ;");
        
        if (fixture.hasError()) return false;
        
        auto effect = fixture.getStackEffect("SQUARE");
        // SQUARE should consume 1 and produce 1
        return effect.consumed == 1 && effect.produced == 1 && effect.isKnown;
    });
    
    runner.addTest("semantic_control_flow_analysis", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode(": ABS DUP 0< IF NEGATE THEN ;");
        
        if (fixture.hasError()) return false;
        
        auto effect = fixture.getStackEffect("ABS");
        // ABS should consume 1 and produce 1
        return effect.consumed == 1 && effect.produced == 1 && effect.isKnown;
    });
    
    runner.addTest("semantic_variable_declaration", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode("VARIABLE COUNTER");
        
        if (fixture.hasError()) return false;
        
        // Variable declarations don't affect stack during compilation
        auto varNode = dynamic_cast<VariableDeclarationNode*>(ast->getChild(0));
        if (!varNode) return false;
        
        auto effect = varNode->getStackEffect();
        return effect.consumed == 0 && effect.produced == 0;
    });
    
    runner.addTest("semantic_constant_declaration", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode("42 CONSTANT ANSWER");
        
        if (fixture.hasError()) return false;
        
        // Constant declaration consumes value from stack
        auto constNode = dynamic_cast<VariableDeclarationNode*>(ast->getChild(1));
        if (!constNode) return false;
        
        auto effect = constNode->getStackEffect();
        return effect.consumed == 1 && effect.produced == 0;
    });
    
    runner.addTest("semantic_string_literal_effects", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode("\" Hello World\"");
        
        auto stringNode = dynamic_cast<StringLiteralNode*>(ast->getChild(0));
        if (!stringNode) return false;
        
        auto effect = stringNode->getStackEffect();
        // Regular strings push address and length
        return effect.consumed == 0 && effect.produced == 2 && effect.isKnown;
    });
    
    runner.addTest("semantic_print_string_effects", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode(".\" Hello World\"");
        
        auto stringNode = dynamic_cast<StringLiteralNode*>(ast->getChild(0));
        if (!stringNode || !stringNode->isPrint()) return false;
        
        auto effect = stringNode->getStackEffect();
        // Print strings don't affect stack
        return effect.consumed == 0 && effect.produced == 0 && effect.isKnown;
    });
    
    runner.addTest("semantic_nested_word_calls", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode(": DOUBLE DUP + ; : QUADRUPLE DOUBLE DOUBLE ;");
        
        if (fixture.hasError()) return false;
        
        auto effect = fixture.getStackEffect("QUADRUPLE");
        // QUADRUPLE should consume 1 and produce 1 (like DOUBLE twice)
        return effect.consumed == 1 && effect.produced == 1 && effect.isKnown;
    });
    
    runner.addTest("semantic_loop_stack_balance", []() {
        SemanticTestFixture fixture;
        auto ast = fixture.analyzeCode(": COUNTDOWN BEGIN DUP . 1- DUP 0= UNTIL DROP ;");
        
        if (fixture.hasError()) return false;
        
        auto effect = fixture.getStackEffect("COUNTDOWN");
        // COUNTDOWN should consume initial value and produce nothing
        return effect.consumed == 1 && effect.produced == 0;
    });
}
