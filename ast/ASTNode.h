#pragma once

#include <memory>
#include <string>
#include <vector>

// =============================================================================
//  DNA v0.1 — AST Node hierarchy
//  File: ast/ASTNode.h
//
//  All AST nodes are defined here as a flat hierarchy under ASTNode.
//  Ownership uses std::unique_ptr<ASTNode> (NodePtr).
//  Traversal uses the classic Visitor pattern; every node has accept().
//
//  Node inventory (matching the spec):
//    Top-level:   ProgramNode, LoadNode
//    Classes:     ClassNode, FieldDeclNode
//    Functions:   ActionNode, ConstructNode
//    Statements:  BlockNode, VarDeclNode, AssignmentNode, IfNode, WhileNode,
//                 ForNode, ReturnNode, BreakNode, ContinueNode, ExprStmtNode
//    Expressions: BinaryExprNode, UnaryExprNode, PostfixExprNode,
//                 CallExprNode, ObjectCreationNode, MemberAccessNode,
//                 LiteralNode, IdentifierNode
// =============================================================================

namespace DNA {

// =============================================================================
// TypeInfo — represents a resolved type in the AST / semantic layer
// =============================================================================
struct TypeInfo {
    enum class Kind {
        INT, FLOAT, DOUBLE, CHAR, BOOL, STRING, VOID,
        CLASS,   // user-defined class; className holds the name
        UNKNOWN  // placeholder / error sentinel
    };

    Kind        kind      = Kind::UNKNOWN;
    std::string className;  // only meaningful when kind == CLASS

    // Constructors
    TypeInfo() = default;
    explicit TypeInfo(Kind k) : kind(k) {}
    explicit TypeInfo(const std::string& cn) : kind(Kind::CLASS), className(cn) {}

    bool isVoid()    const { return kind == Kind::VOID;    }
    bool isUnknown() const { return kind == Kind::UNKNOWN; }
    bool isPrimitive() const {
        return kind == Kind::INT   || kind == Kind::FLOAT  ||
               kind == Kind::DOUBLE|| kind == Kind::CHAR   ||
               kind == Kind::BOOL  || kind == Kind::STRING;
    }

    bool operator==(const TypeInfo& o) const {
        return kind == o.kind && className == o.className;
    }
    bool operator!=(const TypeInfo& o) const { return !(*this == o); }

    std::string toString() const {
        switch (kind) {
            case Kind::INT:     return "int";
            case Kind::FLOAT:   return "float";
            case Kind::DOUBLE:  return "double";
            case Kind::CHAR:    return "char";
            case Kind::BOOL:    return "bool";
            case Kind::STRING:  return "String";
            case Kind::VOID:    return "void";
            case Kind::CLASS:   return className;
            default:            return "unknown";
        }
    }
};

// =============================================================================
// Parameter — used in ActionNode and ConstructNode parameter lists
// =============================================================================
struct Parameter {
    TypeInfo    type;
    std::string name;
};

// =============================================================================
// Visibility — access modifier
// =============================================================================
enum class Visibility { PUBLIC, PRIVATE };

// =============================================================================
// Forward declarations (needed by the Visitor interface)
// =============================================================================

// Top-level
class ProgramNode;
class LoadNode;

// Class-related
class ClassNode;
class FieldDeclNode;
class ActionNode;
class ConstructNode;

// Statements
class BlockNode;
class VarDeclNode;
class AssignmentNode;
class IfNode;
class WhileNode;
class ForNode;
class ReturnNode;
class BreakNode;
class ContinueNode;
class ExprStmtNode;

// Expressions
class BinaryExprNode;
class UnaryExprNode;
class PostfixExprNode;
class CallExprNode;
class ObjectCreationNode;
class MemberAccessNode;
class LiteralNode;
class IdentifierNode;

// =============================================================================
// ASTVisitor — pure-virtual interface implemented by each compiler pass
// =============================================================================
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // Top-level
    virtual void visitProgram(ProgramNode&)      = 0;
    virtual void visitLoad(LoadNode&)            = 0;

    // Declarations
    virtual void visitClass(ClassNode&)          = 0;
    virtual void visitFieldDecl(FieldDeclNode&)  = 0;
    virtual void visitAction(ActionNode&)        = 0;
    virtual void visitConstruct(ConstructNode&)  = 0;

    // Statements
    virtual void visitBlock(BlockNode&)          = 0;
    virtual void visitVarDecl(VarDeclNode&)      = 0;
    virtual void visitAssignment(AssignmentNode&)= 0;
    virtual void visitIf(IfNode&)                = 0;
    virtual void visitWhile(WhileNode&)          = 0;
    virtual void visitFor(ForNode&)              = 0;
    virtual void visitReturn(ReturnNode&)        = 0;
    virtual void visitBreak(BreakNode&)          = 0;
    virtual void visitContinue(ContinueNode&)    = 0;
    virtual void visitExprStmt(ExprStmtNode&)    = 0;

    // Expressions
    virtual void visitBinaryExpr(BinaryExprNode&)      = 0;
    virtual void visitUnaryExpr(UnaryExprNode&)        = 0;
    virtual void visitPostfixExpr(PostfixExprNode&)    = 0;
    virtual void visitCallExpr(CallExprNode&)          = 0;
    virtual void visitObjectCreation(ObjectCreationNode&) = 0;
    virtual void visitMemberAccess(MemberAccessNode&)  = 0;
    virtual void visitLiteral(LiteralNode&)            = 0;
    virtual void visitIdentifier(IdentifierNode&)      = 0;
};

// =============================================================================
// ASTNode — base class for every node
// =============================================================================
class ASTNode {
public:
    int line   = 0;
    int column = 0;

    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

// Convenience aliases
using NodePtr  = std::unique_ptr<ASTNode>;
using NodeList = std::vector<NodePtr>;

// =============================================================================
// Top-level nodes
// =============================================================================

/// Root of the AST. Holds top-level declarations in source order.
class ProgramNode : public ASTNode {
public:
    NodeList declarations; // LoadNode | ClassNode | ActionNode
    void accept(ASTVisitor& v) override { v.visitProgram(*this); }
};

/// load <module>
class LoadNode : public ASTNode {
public:
    std::string moduleName;
    void accept(ASTVisitor& v) override { v.visitLoad(*this); }
};

// =============================================================================
// Class-related nodes
// =============================================================================

/// A field declaration inside a class body.
///   [public|private] Type name [= initializer]
class FieldDeclNode : public ASTNode {
public:
    Visibility visibility  = Visibility::PRIVATE;
    TypeInfo   type;
    std::string name;
    NodePtr    initializer; // optional default value
    void accept(ASTVisitor& v) override { v.visitFieldDecl(*this); }
};

/// A block of statements enclosed in { }.
class BlockNode : public ASTNode {
public:
    NodeList statements;
    void accept(ASTVisitor& v) override { v.visitBlock(*this); }
};

/// construct([params]) { body }
class ConstructNode : public ASTNode {
public:
    Visibility              visibility = Visibility::PUBLIC;
    std::vector<Parameter>  params;
    std::unique_ptr<BlockNode> body;
    void accept(ASTVisitor& v) override { v.visitConstruct(*this); }
};

/// action [returnType] name([params]) { body }
class ActionNode : public ASTNode {
public:
    Visibility              visibility = Visibility::PRIVATE;
    TypeInfo                returnType;
    std::string             name;
    std::vector<Parameter>  params;
    std::unique_ptr<BlockNode> body;
    void accept(ASTVisitor& v) override { v.visitAction(*this); }
};

/// class Name { members* }
class ClassNode : public ASTNode {
public:
    std::string name;
    NodeList    members; // FieldDeclNode | ActionNode | ConstructNode
    void accept(ASTVisitor& v) override { v.visitClass(*this); }
};

// =============================================================================
// Statement nodes
// =============================================================================

/// Type name [= initializer]
class VarDeclNode : public ASTNode {
public:
    TypeInfo    type;
    std::string name;
    NodePtr     initializer; // optional
    void accept(ASTVisitor& v) override { v.visitVarDecl(*this); }
};

/// target = value
class AssignmentNode : public ASTNode {
public:
    NodePtr target; // IdentifierNode or MemberAccessNode
    NodePtr value;
    void accept(ASTVisitor& v) override { v.visitAssignment(*this); }
};

/// if(condition) thenBlock [else elseBlock]
class IfNode : public ASTNode {
public:
    NodePtr                    condition;
    std::unique_ptr<BlockNode> thenBlock;
    std::unique_ptr<BlockNode> elseBlock; // nullptr if no else
    void accept(ASTVisitor& v) override { v.visitIf(*this); }
};

/// while(condition) body
class WhileNode : public ASTNode {
public:
    NodePtr                    condition;
    std::unique_ptr<BlockNode> body;
    void accept(ASTVisitor& v) override { v.visitWhile(*this); }
};

/// for(init; condition; update) body
class ForNode : public ASTNode {
public:
    NodePtr                    init;       // VarDeclNode
    NodePtr                    condition;  // boolean expression
    NodePtr                    update;     // e.g. i++ (PostfixExprNode)
    std::unique_ptr<BlockNode> body;
    void accept(ASTVisitor& v) override { v.visitFor(*this); }
};

/// return [value]
class ReturnNode : public ASTNode {
public:
    NodePtr value; // nullptr for void returns
    void accept(ASTVisitor& v) override { v.visitReturn(*this); }
};

/// break
class BreakNode : public ASTNode {
public:
    void accept(ASTVisitor& v) override { v.visitBreak(*this); }
};

/// continue
class ContinueNode : public ASTNode {
public:
    void accept(ASTVisitor& v) override { v.visitContinue(*this); }
};

/// A statement that consists of a bare expression (e.g. a function call).
class ExprStmtNode : public ASTNode {
public:
    NodePtr expr;
    void accept(ASTVisitor& v) override { v.visitExprStmt(*this); }
};

// =============================================================================
// Expression nodes
// =============================================================================

/// left op right
///   op ∈ { + - * / % == != > < >= <= && || }
class BinaryExprNode : public ASTNode {
public:
    std::string op;
    NodePtr     left;
    NodePtr     right;
    void accept(ASTVisitor& v) override { v.visitBinaryExpr(*this); }
};

/// op operand   (prefix)
///   op ∈ { ! - }
class UnaryExprNode : public ASTNode {
public:
    std::string op;
    NodePtr     operand;
    void accept(ASTVisitor& v) override { v.visitUnaryExpr(*this); }
};

/// operand op   (postfix)
///   op ∈ { ++ -- }
class PostfixExprNode : public ASTNode {
public:
    std::string op;
    NodePtr     operand;
    void accept(ASTVisitor& v) override { v.visitPostfixExpr(*this); }
};

/// callee(args…)
/// callee is the raw name (e.g. "print", "add", "myFunc").
class CallExprNode : public ASTNode {
public:
    std::string callee;
    NodeList    args;
    void accept(ASTVisitor& v) override { v.visitCallExpr(*this); }
};

/// ClassName(args…)  — object construction, no 'new' keyword
class ObjectCreationNode : public ASTNode {
public:
    std::string className;
    NodeList    args;
    void accept(ASTVisitor& v) override { v.visitObjectCreation(*this); }
};

/// object.member   — field / property access
class MemberAccessNode : public ASTNode {
public:
    NodePtr     object;
    std::string member;
    void accept(ASTVisitor& v) override { v.visitMemberAccess(*this); }
};

/// A compile-time literal value.
class LiteralNode : public ASTNode {
public:
    enum class Kind { INT, FLOAT, DOUBLE, CHAR, STRING, BOOL_TRUE, BOOL_FALSE };
    Kind        kind;
    std::string value; // raw text representation

    LiteralNode(Kind k, std::string v) : kind(k), value(std::move(v)) {}
    void accept(ASTVisitor& v) override { v.visitLiteral(*this); }
};

/// A bare identifier reference.
class IdentifierNode : public ASTNode {
public:
    std::string name;
    explicit IdentifierNode(std::string n) : name(std::move(n)) {}
    void accept(ASTVisitor& v) override { v.visitIdentifier(*this); }
};

} // namespace DNA
