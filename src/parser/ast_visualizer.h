#ifndef FORTH_AST_VISUALIZER_H
#define FORTH_AST_VISUALIZER_H

#include "parser/ast.h"
#include <iostream>
#include <string>
#include <vector>

class ASTVisualizer : public ASTVisitor {
private:
    std::vector<bool> isLastChild;
    bool compact;
    
    void printPrefix() {
        for (size_t i = 0; i < isLastChild.size(); ++i) {
            if (i == isLastChild.size() - 1) {
                std::cout << (isLastChild[i] ? "└── " : "├── ");
            } else {
                std::cout << (isLastChild[i] ? "    " : "│   ");
            }
        }
    }
    
    void visitChildren(ASTNode& node) {
        for (size_t i = 0; i < node.getChildCount(); ++i) {
            isLastChild.push_back(i == node.getChildCount() - 1);
            node.getChild(i)->accept(*this);
            isLastChild.pop_back();
        }
    }
    
public:
    explicit ASTVisualizer(bool compactMode = false) : compact(compactMode) {}
    
    void visit(ProgramNode& node) override {
        if (isLastChild.empty()) {
            std::cout << "Program (" << node.getChildCount() << " statements)\n";
        } else {
            printPrefix();
            std::cout << "Program (" << node.getChildCount() << " statements)\n";
        }
        visitChildren(node);
    }
    
    void visit(WordDefinitionNode& node) override {
        printPrefix();
        std::cout << "Definition: " << node.getWordName();
        if (!compact) {
            std::cout << " [" << node.getChildCount() << " operations]";
        }
        std::cout << "\n";
        visitChildren(node);
    }
    
    void visit(WordCallNode& node) override {
        printPrefix();
        std::cout << "Call: " << node.getWordName() << "\n";
    }
    
    void visit(NumberLiteralNode& node) override {
        printPrefix();
        std::cout << "Number: " << node.getValue();
        if (!compact && node.isFloatingPoint()) {
            std::cout << " (float)";
        }
        std::cout << "\n";
    }
    
    void visit(StringLiteralNode& node) override {
        printPrefix();
        std::cout << "String: \"" << node.getValue() << "\"";
        if (!compact) {
            std::cout << (node.isPrint() ? " [PRINT]" : " [LITERAL]");
        }
        std::cout << "\n";
    }
    
    void visit(IfStatementNode& node) override {
        printPrefix();
        std::cout << "IF" << (node.hasElse() ? "-ELSE" : "") << "-THEN\n";
        
        if (node.getThenBranch()) {
            isLastChild.push_back(!node.hasElse());
            printPrefix();
            std::cout << "THEN branch:\n";
            for (const auto& child : node.getThenBranch()->getChildren()) {
                isLastChild.push_back(&child == &node.getThenBranch()->getChildren().back());
                child->accept(*this);
                isLastChild.pop_back();
            }
            isLastChild.pop_back();
        }
        
        if (node.getElseBranch()) {
            isLastChild.push_back(true);
            printPrefix();
            std::cout << "ELSE branch:\n";
            for (const auto& child : node.getElseBranch()->getChildren()) {
                isLastChild.push_back(&child == &node.getElseBranch()->getChildren().back());
                child->accept(*this);
                isLastChild.pop_back();
            }
            isLastChild.pop_back();
        }
    }
    
    void visit(BeginUntilLoopNode& node) override {
        printPrefix();
        std::cout << "BEGIN-UNTIL Loop\n";
        
        if (node.getBody()) {
            isLastChild.push_back(true);
            printPrefix();
            std::cout << "Body:\n";
            for (const auto& child : node.getBody()->getChildren()) {
                isLastChild.push_back(&child == &node.getBody()->getChildren().back());
                child->accept(*this);
                isLastChild.pop_back();
            }
            isLastChild.pop_back();
        }
    }
    
    void visit(MathOperationNode& node) override {
        printPrefix();
        std::cout << "Math: " << node.getOperation();
        if (!compact) {
            auto effect = node.getStackEffect();
            if (effect.isKnown) {
                std::cout << " [" << effect.consumed << "→" << effect.produced << "]";
            }
        }
        std::cout << "\n";
    }
    
    void visit(VariableDeclarationNode& node) override {
        printPrefix();
        std::cout << (node.isConst() ? "Constant: " : "Variable: ") 
                  << node.getVarName() << "\n";
    }
};

// Utility function to visualize any AST
inline void visualizeAST(ASTNode& root, bool compact = false) {
    ASTVisualizer visualizer(compact);
    root.accept(visualizer);
}

#endif // FORTH_AST_VISUALIZER_H
