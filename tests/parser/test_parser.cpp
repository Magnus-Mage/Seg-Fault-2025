#include "parser/parser.h"
#include "lexer/lexer.h"
#include "../test_framework.h"
#include <cassert>
#include <iostream>

// Helper function to parse FORTH code
auto parseCode(const std::string& code) -> std::pair<std::unique_ptr<ProgramNode>, std::vector<std::string>> {
    ForthLexer lexer;
    auto tokens = lexer.tokenize(code);
    
    ForthParser parser;
    auto ast = parser.parseProgram(tokens);
    auto errors = parser.getErrors();
    
    return {std::move(ast), errors};
}

// Helper function to check if AST has specific node types
auto hasNodeType(ProgramNode* program, ASTNode::NodeType type) -> bool {
    for (const auto& child : program->getChildren()) {
        if (child->getType() == type) {
            return true;
        }
        // Could add recursive search here if needed
    }
    return false;
}

auto registerParserTests(TestRunner& runner) -> void {
    
    runner.addTest("Basic Number Parsing", []() {
        auto [ast, errors] = parseCode("42 3.14 -17");
        
        assert(errors.empty());
        assert(ast != nullptr);
        assert(ast->getChildCount() == 3);
        
        // Check each number node
        for (size_t i = 0; i < ast->getChildCount(); ++i) {
            assert(ast->getChild(i)->getType() == ASTNode::NodeType::NUMBER_LITERAL);
        }
        
        return true;
    });
    
    runner.addTest("Basic Math Operations", []() {
        auto [ast, errors] = parseCode("5 3 + 2 *");
        
        assert(errors.empty());
        assert(ast != nullptr);
        assert(ast->getChildCount() == 4); // 5, 3, +, 2, *
        
        // Check we have numbers and math operations
        assert(ast->getChild(0)->getType() == ASTNode::NodeType::NUMBER_LITERAL);
        assert(ast->getChild(1)->getType() == ASTNode::NodeType::NUMBER_LITERAL);
        assert(ast->getChild(2)->getType() == ASTNode::NodeType::MATH_OPERATION);
        assert(ast->getChild(3)->getType() == ASTNode::NodeType::NUMBER_LITERAL);
        
        return true;
    });
    
    runner.addTest("Word Definition", []() {
        auto [ast, errors] = parseCode(": SQUARE DUP * ;");
        
        assert(errors.empty());
        assert(ast != nullptr);
        assert(ast->getChildCount() == 1);
        assert(ast->getChild(0)->getType() == ASTNode::NodeType::WORD_DEFINITION);
        
        auto wordDef = dynamic_cast<WordDefinitionNode*>(ast->getChild(0));
        assert(wordDef != nullptr);
        assert(wordDef->getWordName() == "SQUARE");
        assert(wordDef->getChildCount() == 2); // DUP and *
        
        return true;
    });
    
    runner.addTest("IF-THEN Statement", []() {
        auto [ast, errors] = parseCode("5 0 > IF 42 THEN");
        
        assert(errors.empty());
        assert(ast != nullptr);
        
        // Should have: 5, 0, >, IF statement
        bool foundIf = false;
        for (size_t i = 0; i < ast->getChildCount(); ++i) {
            if (ast->getChild(i)->getType() == ASTNode::NodeType::IF_STATEMENT) {
                foundIf = true;
                auto ifNode = dynamic_cast<IfStatementNode*>(ast->getChild(i));
                assert(ifNode != nullptr);
                assert(ifNode->getThenBranch() != nullptr);
                assert(!ifNode->hasElse());
                break;
            }
        }
        assert(foundIf);
        
        return true;
    });
    
    runner.addTest("IF-ELSE-THEN Statement", []() {
        auto [ast, errors] = parseCode("5 0 > IF 42 ELSE 24 THEN");
        
        assert(errors.empty());
        assert(ast != nullptr);
        
        bool foundIf = false;
        for (size_t i = 0; i < ast->getChildCount(); ++i) {
            if (ast->getChild(i)->getType() == ASTNode::NodeType::IF_STATEMENT) {
                foundIf = true;
                auto ifNode = dynamic_cast<IfStatementNode*>(ast->getChild(i));
                assert(ifNode != nullptr);
                assert(ifNode->getThenBranch() != nullptr);
                assert(ifNode->hasElse());
                assert(ifNode->getElseBranch() != nullptr);
                break;
            }
        }
        assert(foundIf);
        
        return true;
    });
    
    runner.addTest("BEGIN-UNTIL Loop", []() {
        auto [ast, errors] = parseCode("5 BEGIN DUP . 1 - DUP 0 <= UNTIL DROP");
        
        assert(errors.empty());
        assert(ast != nullptr);
        
        bool foundLoop = false;
        for (size_t i = 0; i < ast->getChildCount(); ++i) {
            if (ast->getChild(i)->getType() == ASTNode::NodeType::BEGIN_UNTIL_LOOP) {
                foundLoop = true;
                auto loopNode = dynamic_cast<BeginUntilLoopNode*>(ast->getChild(i));
                assert(loopNode != nullptr);
                assert(loopNode->getBody() != nullptr);
                break;
            }
        }
        assert(foundLoop);
        
        return true;
    });
    
    runner.addTest("Variable Declaration", []() {
        auto [ast, errors] = parseCode("VARIABLE COUNTER");
        
        assert(errors.empty());
        assert(ast != nullptr);
        assert(ast->getChildCount() == 1);
        assert(ast->getChild(0)->getType() == ASTNode::NodeType::VARIABLE_DECLARATION);
        
        auto varNode = dynamic_cast<VariableDeclarationNode*>(ast->getChild(0));
        assert(varNode != nullptr);
        assert(varNode->getVarName() == "COUNTER");
        assert(!varNode->isConst());
        
        return true;
    });
    
    runner.addTest("Constant Declaration", []() {
        auto [ast, errors] = parseCode("3.14159 CONSTANT PI");
        
        assert(errors.empty());
        assert(ast != nullptr);
        assert(ast->getChildCount() == 2); // Number literal + constant declaration
        
        // Find the constant declaration
        bool foundConst = false;
        for (size_t i = 0; i < ast->getChildCount(); ++i) {
            if (ast->getChild(i)->getType() == ASTNode::NodeType::CONSTANT_DECLARATION) {
                foundConst = true;
                auto constNode = dynamic_cast<VariableDeclarationNode*>(ast->getChild(i));
                assert(constNode != nullptr);
                assert(constNode->getVarName() == "PI");
                assert(constNode->isConst());
                break;
            }
        }
        assert(foundConst);
        
        return true;
    });
    
    runner.addTest("String Literals", []() {
        auto [ast, errors] = parseCode(".\" Hello World\" \" test string\"");
        
        assert(errors.empty());
        assert(ast != nullptr);
        assert(ast->getChildCount() == 2);
        
        for (size_t i = 0; i < ast->getChildCount(); ++i) {
            assert(ast->getChild(i)->getType() == ASTNode::NodeType::STRING_LITERAL);
        }
        
        // Check print string vs regular string
        auto printStr = dynamic_cast<StringLiteralNode*>(ast->getChild(0));
        auto regularStr = dynamic_cast<StringLiteralNode*>(ast->getChild(1));
        
        assert(printStr != nullptr && printStr->isPrint());
        assert(regularStr != nullptr && !regularStr->isPrint());
        
        return true;
    });
    
    runner.addTest("Complex Word Definition", []() {
        auto [ast, errors] = parseCode(": DISTANCE SWAP DUP * SWAP DUP * + SQRT ;");
        
        assert(errors.empty());
        assert(ast != nullptr);
        assert(ast->getChildCount() == 1);
        assert(ast->getChild(0)->getType() == ASTNode::NodeType::WORD_DEFINITION);
        
        auto wordDef = dynamic_cast<WordDefinitionNode*>(ast->getChild(0));
        assert(wordDef != nullptr);
        assert(wordDef->getWordName() == "DISTANCE");
        assert(wordDef->getChildCount() > 5); // Should have multiple operations
        
        return true;
    });
    
    runner.addTest("Nested Control Structures", []() {
        auto [ast, errors] = parseCode(R"(
            : TEST-NESTED
                5 BEGIN
                    DUP 0 > IF
                        DUP .
                    THEN
                    1 -
                    DUP 0 <=
                UNTIL
                DROP
            ;
        )");
        
        assert(errors.empty());
        assert(ast != nullptr);
        
        // Should parse without errors
        return true;
    });
    
    runner.addTest("Error Handling - Unmatched IF", []() {
        auto [ast, errors] = parseCode("IF 42");
        
        assert(!errors.empty());
        // Should contain error about unmatched control flow
        bool foundControlFlowError = false;
        for (const auto& error : errors) {
            if (error.find("control flow") != std::string::npos ||
                error.find("THEN") != std::string::npos) {
                foundControlFlowError = true;
                break;
            }
        }
        assert(foundControlFlowError);
        
        return true;
    });
    
    runner.addTest("Error Handling - Invalid Word Definition", []() {
        auto [ast, errors] = parseCode(": ; : INVALID");
        
        assert(!errors.empty());
        // Should have error about missing word name
        bool foundNameError = false;
        for (const auto& error : errors) {
            if (error.find("word name") != std::string::npos) {
                foundNameError = true;
                break;
            }
        }
        assert(foundNameError);
        
        return true;
    });
    
    runner.addTest("Error Handling - Undefined Word", []() {
        auto [ast, errors] = parseCode("UNDEFINED-WORD 42 +");
        
        assert(!errors.empty());
        // Should have error about undefined word
        bool foundUndefinedError = false;
        for (const auto& error : errors) {
            if (error.find("Undefined word") != std::string::npos) {
                foundUndefinedError = true;
                break;
            }
        }
        assert(foundUndefinedError);
        
        return true;
    });
    
    runner.addTest("Dictionary Integration", []() {
        ForthParser parser;
        auto& dict = parser.getDictionary();
        
        // Dictionary should have built-in words
        assert(dict.isWordDefined("+"));
        assert(dict.isWordDefined("DUP"));
        assert(dict.isWordDefined("SQRT"));
        
        // Test custom word definition
        auto [ast, errors] = parseCode(": DOUBLE DUP + ;");
        assert(errors.empty());
        
        // Word should now be in dictionary
        assert(dict.isWordDefined("DOUBLE"));
        
        return true;
    });
    
    runner.addTest("Stack Effect Analysis", []() {
        ForthParser parser;
        auto& dict = parser.getDictionary();
        
        // Test stack effects for built-in words
        auto dupEffect = dict.getStackEffect("DUP");
        assert(dupEffect.consumed == 1 && dupEffect.produced == 2 && dupEffect.isKnown);
        
        auto addEffect = dict.getStackEffect("+");
        assert(addEffect.consumed == 2 && addEffect.produced == 1 && addEffect.isKnown);
        
        auto dropEffect = dict.getStackEffect("DROP");
        assert(dropEffect.consumed == 1 && dropEffect.produced == 0 && dropEffect.isKnown);
        
        return true;
    });
    
    runner.addTest("Complex Program from Test File", []() {
        // Parse a portion of the test FORTH file
        std::string complexCode = R"(
            : SQUARE DUP * ;
            : CUBE DUP SQUARE * ;
            : ABS-VALUE DUP 0 < IF NEGATE THEN ;
            VARIABLE COUNTER
            CONSTANT PI 3.14159
            42 SQUARE .
        )";
        
        auto [ast, errors] = parseCode(complexCode);
        
        // Should parse successfully
        assert(errors.empty());
        assert(ast != nullptr);
        assert(ast->getChildCount() > 5);
        
        // Should have word definitions, variable, constant, and expressions
        bool hasWordDef = false, hasVariable = false, hasConstant = false;
        for (size_t i = 0; i < ast->getChildCount(); ++i) {
            auto nodeType = ast->getChild(i)->getType();
            if (nodeType == ASTNode::NodeType::WORD_DEFINITION) hasWordDef = true;
            if (nodeType == ASTNode::NodeType::VARIABLE_DECLARATION) hasVariable = true;
            if (nodeType == ASTNode::NodeType::CONSTANT_DECLARATION) hasConstant = true;
        }
        
        assert(hasWordDef && hasVariable && hasConstant);
        
        return true;
    });
}
