#pragma once

#include "../ast/ASTNode.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <stack>

// =============================================================================
//  DNA v0.1 — Semantic Analyzer
//  File: semantic/SemanticAnalyzer.h
//
//  Visitor-based pass over the AST.  Performs:
//    1. Undeclared variable detection
//    2. Duplicate declaration detection
//    3. Type-mismatch checking  (assignment, return statements)
//    4. Invalid return type validation
//    5. Break / continue outside loop detection
//
//  Built-in function types (v0.1 approximations):
//    print(any)  → void
//    input(any)  → String  (assignments from input() are allowed for any type)
//
//  Design notes:
//    - Scoped symbol table: std::vector of maps, push on block entry, pop on exit.
//    - Class definitions collected in a first pass before body analysis.
//    - Type inference returns TypeInfo::UNKNOWN on error to avoid cascading msgs.
// =============================================================================

#include <unordered_set>

namespace DNA {

// -----------------------------------------------------------------------------
// SymbolInfo — what we store per declared name
// -----------------------------------------------------------------------------
struct SymbolInfo {
    TypeInfo type;
    int      declLine = 0;
    bool     isClass  = false;
    bool     isAction = false;
};

// -----------------------------------------------------------------------------
// SemanticAnalyzer
// -----------------------------------------------------------------------------
class SemanticAnalyzer : public ASTVisitor {
public:
    /// Run the analysis pass.  Call errors() afterwards.
    void analyze(ProgramNode& program);

    const std::vector<std::string>& errors() const { return errors_; }
    bool hasErrors()  const { return !errors_.empty(); }

private:
    // ── Symbol table ───────────────────────────────────────────────────────────
    using Scope = std::unordered_map<std::string, SymbolInfo>;
    std::vector<Scope> scopes_;

    // ── Class registry (first-pass) ────────────────────────────────────────────
    std::unordered_map<std::string, ClassNode*> classes_;
    std::unordered_map<std::string, ActionNode*> actions_;
    std::unordered_set<std::string> loadedModules_;

    // ── Context tracking ──────────────────────────────────────────────────────
    TypeInfo currentReturnType_;  // expected return type of the enclosing action
    bool     inLoop_ = false;     // true when inside a while/for body

    // ── Error accumulator ──────────────────────────────────────────────────────
    std::vector<std::string> errors_;

    // ── Scope management ──────────────────────────────────────────────────────
    void   pushScope();
    void   popScope();
    void   declare(const std::string& name, SymbolInfo info);
    bool   isDeclaredInCurrentScope(const std::string& name) const;
    const SymbolInfo* lookup(const std::string& name) const; // nullptr if not found

    // ── Type checking helpers ─────────────────────────────────────────────────
    TypeInfo inferType(ASTNode& expr);
    bool     isAssignable(const TypeInfo& to, const TypeInfo& from) const;

    // ── Error reporting ────────────────────────────────────────────────────────
    void reportError(const std::string& msg, int line = 0);

    // ── Visitor overrides ─────────────────────────────────────────────────────
    void visitProgram(ProgramNode&)       override;
    void visitLoad(LoadNode&)             override;
    void visitClass(ClassNode&)           override;
    void visitFieldDecl(FieldDeclNode&)   override;
    void visitAction(ActionNode&)         override;
    void visitConstruct(ConstructNode&)   override;
    void visitBlock(BlockNode&)           override;
    void visitVarDecl(VarDeclNode&)       override;
    void visitAssignment(AssignmentNode&) override;
    void visitIf(IfNode&)                 override;
    void visitWhile(WhileNode&)           override;
    void visitFor(ForNode&)               override;
    void visitReturn(ReturnNode&)         override;
    void visitBreak(BreakNode&)           override;
    void visitContinue(ContinueNode&)     override;
    void visitExprStmt(ExprStmtNode&)     override;
    void visitBinaryExpr(BinaryExprNode&) override;
    void visitUnaryExpr(UnaryExprNode&)   override;
    void visitPostfixExpr(PostfixExprNode&) override;
    void visitCallExpr(CallExprNode&)       override;
    void visitObjectCreation(ObjectCreationNode&) override;
    void visitMemberAccess(MemberAccessNode&)     override;
    void visitLiteral(LiteralNode&)         override;
    void visitIdentifier(IdentifierNode&)   override;
};

} // namespace DNA
