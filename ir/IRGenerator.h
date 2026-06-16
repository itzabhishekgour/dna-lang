#pragma once

#include "../ast/ASTNode.h"
#include "IRBuilder.h"
#include "IRProgram.h"
#include <string>

// =============================================================================
//  DNA IR v0.1 — IR Generator
//  File: ir/IRGenerator.h
//
//  IRGenerator is an ASTVisitor that walks a fully type-checked ProgramNode
//  and lowers it to an IRProgram consisting of three-address-code (3AC)
//  instructions.
//
//  Expression lowering:
//    Each expression visitor sets lastResult_ to the name of the temp or
//    literal string that holds the expression's value.  Callers use the
//    helper  emitExpr(node) → std::string  to get this value.
//
//  Statement lowering:
//    Statement visitors call emitExpr() and emit instructions directly via
//    the IRBuilder.
//
//  Control-flow lowering:
//    if    → CMP + JUMP_IF_FALSE + optional JUMP (for else)
//    while → LABEL + CMP + JUMP_IF_FALSE + JUMP
//    for   → init + LABEL + CMP + JUMP_IF_FALSE + body + update + JUMP
//
//  Class lowering:
//    - Class fields are recorded in IRClass metadata.
//    - Methods are emitted as qualified functions: "ClassName::methodName".
//    - Constructors are emitted as: "ClassName::construct".
//    - Object creation emits NEW instructions.
//    - Field access emits GETFIELD / SETFIELD.
// =============================================================================

namespace DNA {

class IRGenerator : public ASTVisitor {
public:
    IRGenerator() = default;

    /// Walk the AST and return the finished IRProgram.
    IRProgram generate(ProgramNode& program);

    // No errors in v0.1 — generator assumes the program is semantically valid.

private:
    IRBuilder   builder_;
    std::string lastResult_;  // result of the most recently emitted expression
    std::string currentClass_; // non-empty when inside a class body

    // ── Helpers ───────────────────────────────────────────────────────────────

    /// Emit expr node, return the name of the temp/literal holding its value.
    std::string emitExpr(ASTNode& node);

    /// Convenience wrappers around builder_.emit()
    void emit(OpCode op, const std::string& dest,
              std::vector<std::string> ops, int line = 0);
    void emit(OpCode op, std::vector<std::string> ops, int line = 0);
    void emit(OpCode op, int line = 0);

    std::string newTemp()                           { return builder_.newTemp();       }
    std::string newLabel(const std::string& prefix) { return builder_.newLabel(prefix);}

    /// Qualify a method name with the current class: "Student::greet"
    std::string qualifiedName(const std::string& name) const;

    // ── ASTVisitor overrides ──────────────────────────────────────────────────

    void visitProgram(ProgramNode&)           override;
    void visitLoad(LoadNode&)                 override;
    void visitClass(ClassNode&)               override;
    void visitFieldDecl(FieldDeclNode&)       override;
    void visitAction(ActionNode&)             override;
    void visitConstruct(ConstructNode&)       override;
    void visitBlock(BlockNode&)               override;
    void visitVarDecl(VarDeclNode&)           override;
    void visitAssignment(AssignmentNode&)     override;
    void visitIf(IfNode&)                     override;
    void visitWhile(WhileNode&)               override;
    void visitFor(ForNode&)                   override;
    void visitReturn(ReturnNode&)             override;
    void visitBreak(BreakNode&)               override;
    void visitContinue(ContinueNode&)         override;
    void visitExprStmt(ExprStmtNode&)         override;
    void visitBinaryExpr(BinaryExprNode&)     override;
    void visitUnaryExpr(UnaryExprNode&)       override;
    void visitPostfixExpr(PostfixExprNode&)   override;
    void visitCallExpr(CallExprNode&)         override;
    void visitObjectCreation(ObjectCreationNode&) override;
    void visitMemberAccess(MemberAccessNode&) override;
    void visitLiteral(LiteralNode&)           override;
    void visitIdentifier(IdentifierNode&)     override;

    // ── Loop-break/continue support ───────────────────────────────────────────
    // Stores the labels for the enclosing loop so break/continue can jump.
    struct LoopContext {
        std::string startLabel;
        std::string endLabel;
    };
    std::vector<LoopContext> loopStack_;
};

} // namespace DNA
