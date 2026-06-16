#include "SemanticAnalyzer.h"

#include <sstream>

// =============================================================================
//  DNA v0.1 — Semantic Analyzer implementation
//  File: semantic/SemanticAnalyzer.cpp
// =============================================================================

namespace DNA {

// =============================================================================
// Public interface
// =============================================================================
void SemanticAnalyzer::analyze(ProgramNode& program) {
    // Reset state
    scopes_.clear();
    classes_.clear();
    actions_.clear();
    loadedModules_.clear();
    errors_.clear();
    inLoop_ = false;
    currentReturnType_ = TypeInfo(); // UNKNOWN

    // First pass: register all top-level class names so forward references work
    for (auto& decl : program.declarations) {
        if (auto* cls = dynamic_cast<ClassNode*>(decl.get())) {
            classes_[cls->name] = cls;
        } else if (auto* act = dynamic_cast<ActionNode*>(decl.get())) {
            actions_[act->name] = act;
        }
    }

    // Check main entrypoint
    auto mainIt = actions_.find("main");
    if (mainIt == actions_.end()) {
        reportError("Entrypoint function 'main' not defined", 0);
    } else {
        ActionNode* mainAct = mainIt->second;
        if (!mainAct->params.empty()) {
            reportError("Entrypoint function 'main' must not accept parameters", mainAct->line);
        }
        if (mainAct->returnType.kind != TypeInfo::Kind::VOID) {
            reportError("Entrypoint function 'main' must return void", mainAct->line);
        }
    }

    // Second pass: full analysis
    pushScope();
    program.accept(*this);
    popScope();
}

// =============================================================================
// Scope management
// =============================================================================
void SemanticAnalyzer::pushScope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::popScope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

void SemanticAnalyzer::declare(const std::string& name, SymbolInfo info) {
    if (scopes_.empty()) return;
    scopes_.back()[name] = std::move(info);
}

bool SemanticAnalyzer::isDeclaredInCurrentScope(const std::string& name) const {
    if (scopes_.empty()) return false;
    return scopes_.back().count(name) > 0;
}

const SymbolInfo* SemanticAnalyzer::lookup(const std::string& name) const {
    // Search from innermost scope outward
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

// =============================================================================
// Error reporting
// =============================================================================
void SemanticAnalyzer::reportError(const std::string& msg, int line) {
    std::ostringstream oss;
    oss << "[Semantic Error]";
    if (line > 0) oss << " Line " << line;
    oss << ": " << msg;
    errors_.push_back(oss.str());
}

// =============================================================================
// Type utilities
// =============================================================================

/// Returns whether a value of type 'from' can be stored into a location of
/// type 'to'.  In v0.1, widening promotions (int→float, int→double, float→double)
/// are allowed.  UNKNOWN on either side suppresses cascading errors.
bool SemanticAnalyzer::isAssignable(const TypeInfo& to, const TypeInfo& from) const {
    if (to.isUnknown() || from.isUnknown()) return true; // suppress cascade
    if (to == from) return true;

    // Numeric widening
    using K = TypeInfo::Kind;
    if (to.kind == K::FLOAT  && from.kind == K::INT)   return true;
    if (to.kind == K::DOUBLE && from.kind == K::INT)    return true;
    if (to.kind == K::DOUBLE && from.kind == K::FLOAT)  return true;

    return false;
}

/// Infers the static type of an expression node.
/// Returns TypeInfo::UNKNOWN on error (already reported).
TypeInfo SemanticAnalyzer::inferType(ASTNode& expr) {
    // We abuse the visitor infrastructure: we need a return-value visitor.
    // Simplest approach: use a local lambda-like struct.
    // Instead, we implement type inference directly via dynamic_cast.

    if (auto* lit = dynamic_cast<LiteralNode*>(&expr)) {
        switch (lit->kind) {
            case LiteralNode::Kind::INT:        return TypeInfo(TypeInfo::Kind::INT);
            case LiteralNode::Kind::FLOAT:      return TypeInfo(TypeInfo::Kind::FLOAT);
            case LiteralNode::Kind::DOUBLE:     return TypeInfo(TypeInfo::Kind::DOUBLE);
            case LiteralNode::Kind::CHAR:       return TypeInfo(TypeInfo::Kind::CHAR);
            case LiteralNode::Kind::STRING:     return TypeInfo(TypeInfo::Kind::STRING);
            case LiteralNode::Kind::BOOL_TRUE:
            case LiteralNode::Kind::BOOL_FALSE: return TypeInfo(TypeInfo::Kind::BOOL);
            default:                            return TypeInfo();
        }
    }

    if (auto* id = dynamic_cast<IdentifierNode*>(&expr)) {
        const SymbolInfo* sym = lookup(id->name);
        if (!sym) {
            reportError("Undeclared variable '" + id->name + "'", id->line);
            return TypeInfo(); // UNKNOWN
        }
        return sym->type;
    }

    if (auto* assign = dynamic_cast<AssignmentNode*>(&expr)) {
        TypeInfo targetType;
        if (auto* id = dynamic_cast<IdentifierNode*>(assign->target.get())) {
            const SymbolInfo* sym = lookup(id->name);
            if (!sym) {
                reportError("Assignment to undeclared variable '" + id->name + "'", assign->line);
                return TypeInfo();
            }
            targetType = sym->type;
        } else if (auto* ma = dynamic_cast<MemberAccessNode*>(assign->target.get())) {
            targetType = inferType(*ma);
        }
        
        if (assign->value) {
            TypeInfo valType = inferType(*assign->value);
            if (!targetType.isUnknown() && !isAssignable(targetType, valType)) {
                reportError("Type Error: Expected " + targetType.toString() +
                            " but got " + valType.toString(), assign->line);
            }
        }
        return targetType;
    }

    if (auto* call = dynamic_cast<CallExprNode*>(&expr)) {
        // Built-ins
        if (call->callee == "print")  return TypeInfo(TypeInfo::Kind::VOID);
        if (call->callee == "input")  return TypeInfo(TypeInfo::Kind::STRING);

        // Method call: callee starts with '.'  (e.g. ".greet", ".getAge")
        // The first arg is the receiver encoded as a MemberAccessNode.
        if (!call->callee.empty() && call->callee[0] == '.') {
            std::string methodName = call->callee.substr(1);
            // Resolve receiver type from first arg (MemberAccessNode or IdentifierNode)
            if (!call->args.empty()) {
                TypeInfo receiverType;
                if (auto* ma = dynamic_cast<MemberAccessNode*>(call->args[0].get())) {
                    receiverType = inferType(*ma->object);
                }
                if (receiverType.kind == TypeInfo::Kind::CLASS) {
                    auto clsIt = classes_.find(receiverType.className);
                    if (clsIt != classes_.end()) {
                        // Find action with matching name
                        for (auto& member : clsIt->second->members) {
                            if (auto* action = dynamic_cast<ActionNode*>(member.get())) {
                                if (action->name == methodName) {
                                    if (call->args.size() - 1 != action->params.size()) {
                                        reportError("Parameter count mismatch", call->line);
                                    } else {
                                        for (std::size_t i = 0; i < action->params.size(); ++i) {
                                            TypeInfo argType = inferType(*call->args[i + 1]);
                                            TypeInfo paramType = action->params[i].type;
                                            if (!isAssignable(paramType, argType)) {
                                                reportError("Parameter type mismatch", call->line);
                                            }
                                        }
                                    }
                                    return action->returnType;
                                }
                            }
                        }
                    }
                } else if (!receiverType.isUnknown()) {
                    reportError("Method call can only be performed on object instances", call->line);
                }
            }
            // Method not found in class — return UNKNOWN to suppress cascade
            return TypeInfo();
        }

        // User-defined: look up function name
        const SymbolInfo* sym = lookup(call->callee);
        if (!sym) {
            reportError("Call to undeclared function '" + call->callee + "'", call->line);
            return TypeInfo();
        }

        auto actIt = actions_.find(call->callee);
        if (actIt != actions_.end()) {
            ActionNode* act = actIt->second;
            if (call->args.size() != act->params.size()) {
                reportError("Parameter count mismatch", call->line);
            } else {
                for (std::size_t i = 0; i < call->args.size(); ++i) {
                    TypeInfo argType = inferType(*call->args[i]);
                    TypeInfo paramType = act->params[i].type;
                    if (!isAssignable(paramType, argType)) {
                        reportError("Parameter type mismatch", call->line);
                    }
                }
            }
        }
        return sym->type;
    }

    if (auto* obj = dynamic_cast<ObjectCreationNode*>(&expr)) {
        if (classes_.find(obj->className) == classes_.end()) {
            reportError("Unknown class '" + obj->className + "'", obj->line);
        }
        // Object creation always returns the class type
        return TypeInfo(obj->className);
    }

    if (auto* bin = dynamic_cast<BinaryExprNode*>(&expr)) {
        TypeInfo lt = inferType(*bin->left);
        TypeInfo rt = inferType(*bin->right);

        // Logical operators
        if (bin->op == "&&" || bin->op == "||") {
            if ((!lt.isUnknown() && lt.kind != TypeInfo::Kind::BOOL) ||
                (!rt.isUnknown() && rt.kind != TypeInfo::Kind::BOOL)) {
                reportError("Logical expression operands must be of type bool", bin->line);
            }
            return TypeInfo(TypeInfo::Kind::BOOL);
        }

        // Relational operators
        if (bin->op == "<" || bin->op == ">" || bin->op == "<=" || bin->op == ">=") {
            if ((!lt.isUnknown() && lt.kind != TypeInfo::Kind::INT && lt.kind != TypeInfo::Kind::FLOAT && lt.kind != TypeInfo::Kind::DOUBLE) ||
                (!rt.isUnknown() && rt.kind != TypeInfo::Kind::INT && rt.kind != TypeInfo::Kind::FLOAT && rt.kind != TypeInfo::Kind::DOUBLE)) {
                reportError("Type mismatch in binary expression", bin->line);
            }
            return TypeInfo(TypeInfo::Kind::BOOL);
        }

        // Equality operators
        if (bin->op == "==" || bin->op == "!=") {
            if (!lt.isUnknown() && !rt.isUnknown() && lt.kind != rt.kind) {
                reportError("Type mismatch in binary expression", bin->line);
            }
            return TypeInfo(TypeInfo::Kind::BOOL);
        }

        // Arithmetic operators
        if (bin->op == "+" || bin->op == "-" || bin->op == "*" || bin->op == "/" || bin->op == "%") {
            if (bin->op == "+" && (lt.kind == TypeInfo::Kind::STRING || rt.kind == TypeInfo::Kind::STRING)) {
                return TypeInfo(TypeInfo::Kind::STRING);
            }
            if ((!lt.isUnknown() && lt.kind != TypeInfo::Kind::INT && lt.kind != TypeInfo::Kind::FLOAT && lt.kind != TypeInfo::Kind::DOUBLE) ||
                (!rt.isUnknown() && rt.kind != TypeInfo::Kind::INT && rt.kind != TypeInfo::Kind::FLOAT && rt.kind != TypeInfo::Kind::DOUBLE)) {
                reportError("Type mismatch in binary expression", bin->line);
            }
        }

        // Arithmetic: widening rules
        if (!lt.isUnknown() && !rt.isUnknown()) {
            if (lt.kind == TypeInfo::Kind::DOUBLE || rt.kind == TypeInfo::Kind::DOUBLE)
                return TypeInfo(TypeInfo::Kind::DOUBLE);
            if (lt.kind == TypeInfo::Kind::FLOAT  || rt.kind == TypeInfo::Kind::FLOAT)
                return TypeInfo(TypeInfo::Kind::FLOAT);
            if (lt.kind == TypeInfo::Kind::STRING && bin->op == "+")
                return TypeInfo(TypeInfo::Kind::STRING); // string concatenation
        }
        return lt.isUnknown() ? rt : lt;
    }

    if (auto* unary = dynamic_cast<UnaryExprNode*>(&expr)) {
        if (unary->op == "!") {
            TypeInfo t = inferType(*unary->operand);
            if (!t.isUnknown() && t.kind != TypeInfo::Kind::BOOL) {
                reportError("Logical NOT requires bool type", unary->line);
            }
            return TypeInfo(TypeInfo::Kind::BOOL);
        }
        if (unary->op == "-") {
            TypeInfo t = inferType(*unary->operand);
            if (!t.isUnknown() && t.kind != TypeInfo::Kind::INT &&
                t.kind != TypeInfo::Kind::FLOAT && t.kind != TypeInfo::Kind::DOUBLE) {
                reportError("Unary minus requires numeric type", unary->line);
            }
            return t;
        }
        return inferType(*unary->operand);
    }

    if (auto* postfix = dynamic_cast<PostfixExprNode*>(&expr)) {
        if (postfix->operand) {
            TypeInfo t = inferType(*postfix->operand);
            if (!t.isUnknown() &&
                t.kind != TypeInfo::Kind::INT   &&
                t.kind != TypeInfo::Kind::FLOAT &&
                t.kind != TypeInfo::Kind::DOUBLE) {
                reportError("Postfix increment/decrement requires numeric type", postfix->line);
            }
            return t;
        }
        return TypeInfo();
    }

    if (auto* ma = dynamic_cast<MemberAccessNode*>(&expr)) {
        // Resolve field type from class definition
        TypeInfo objType = inferType(*ma->object);
        if (objType.kind == TypeInfo::Kind::CLASS) {
            auto clsIt = classes_.find(objType.className);
            if (clsIt != classes_.end()) {
                for (auto& member : clsIt->second->members) {
                    if (auto* field = dynamic_cast<FieldDeclNode*>(member.get())) {
                        if (field->name == ma->member) return field->type;
                    }
                }
            }
        } else if (!objType.isUnknown()) {
            reportError("Field access or assignment can only be performed on object instances", ma->line);
        }
        return TypeInfo(); // field not found or non-class
    }

    return TypeInfo(); // UNKNOWN
}

// =============================================================================
// Visitor implementations
// =============================================================================

void SemanticAnalyzer::visitProgram(ProgramNode& node) {
    for (auto& decl : node.declarations) {
        if (decl) decl->accept(*this);
    }
}

void SemanticAnalyzer::visitLoad(LoadNode& node) {
    if (loadedModules_.count(node.moduleName) > 0) {
        reportError("Duplicate module import '" + node.moduleName + "'", node.line);
    } else {
        loadedModules_.insert(node.moduleName);
    }
}

void SemanticAnalyzer::visitClass(ClassNode& node) {
    pushScope();
    // Declare class-level fields and methods in this scope
    for (auto& member : node.members) {
        if (member) member->accept(*this);
    }
    popScope();
}

void SemanticAnalyzer::visitFieldDecl(FieldDeclNode& node) {
    if (isDeclaredInCurrentScope(node.name)) {
        reportError("Duplicate field declaration '" + node.name + "'", node.line);
        return;
    }

    if (node.initializer) {
        TypeInfo initType = inferType(*node.initializer);
        if (!isAssignable(node.type, initType)) {
            reportError("Type Error: Field '" + node.name +
                        "' declared as " + node.type.toString() +
                        " but initialised with " + initType.toString(), node.line);
        }
    }

    declare(node.name, { node.type, node.line });
}

void SemanticAnalyzer::visitAction(ActionNode& node) {
    if (isDeclaredInCurrentScope(node.name)) {
        reportError("Duplicate action declaration '" + node.name + "'", node.line);
    } else {
        declare(node.name, { node.returnType, node.line, false, true });
    }

    pushScope();
    // Add parameters to inner scope
    for (auto& p : node.params) {
        if (isDeclaredInCurrentScope(p.name)) {
            reportError("Duplicate parameter '" + p.name + "' in action '"
                        + node.name + "'", node.line);
        } else {
            declare(p.name, { p.type, node.line });
        }
    }

    TypeInfo savedReturn = currentReturnType_;
    currentReturnType_ = node.returnType;

    if (node.body) node.body->accept(*this);

    currentReturnType_ = savedReturn;
    popScope();
}

void SemanticAnalyzer::visitConstruct(ConstructNode& node) {
    pushScope();
    for (auto& p : node.params) {
        if (isDeclaredInCurrentScope(p.name)) {
            reportError("Duplicate parameter '" + p.name + "' in construct", node.line);
        } else {
            declare(p.name, { p.type, node.line });
        }
    }

    TypeInfo savedReturn = currentReturnType_;
    currentReturnType_ = TypeInfo(TypeInfo::Kind::VOID);

    if (node.body) node.body->accept(*this);

    currentReturnType_ = savedReturn;
    popScope();
}

void SemanticAnalyzer::visitBlock(BlockNode& node) {
    // Note: callers push/pop scope around blocks that need it.
    // The block itself does not push a scope — that is done by the enclosing
    // action/while/for/if to allow parameter visibility inside the block.
    for (auto& stmt : node.statements) {
        if (stmt) stmt->accept(*this);
    }
}

void SemanticAnalyzer::visitVarDecl(VarDeclNode& node) {
    if (isDeclaredInCurrentScope(node.name)) {
        reportError("Duplicate variable declaration '" + node.name + "'", node.line);
        return;
    }

    if (node.type.kind == TypeInfo::Kind::CLASS) {
        if (classes_.find(node.type.className) == classes_.end()) {
            reportError("Unknown class '" + node.type.className + "'", node.line);
        }
    }

    if (node.initializer) {
        TypeInfo initType = inferType(*node.initializer);

        // input() is assignment-compatible with any primitive type in v0.1
        bool fromInput = false;
        if (auto* call = dynamic_cast<CallExprNode*>(node.initializer.get())) {
            fromInput = (call->callee == "input");
        }

        if (!fromInput && !isAssignable(node.type, initType)) {
            reportError("Type Error: Expected " + node.type.toString() +
                        " but got " + initType.toString(), node.line);
        }
    }

    declare(node.name, { node.type, node.line });
}

void SemanticAnalyzer::visitAssignment(AssignmentNode& node) {
    // Resolve target type
    TypeInfo targetType;
    if (auto* id = dynamic_cast<IdentifierNode*>(node.target.get())) {
        const SymbolInfo* sym = lookup(id->name);
        if (!sym) {
            reportError("Assignment to undeclared variable '" + id->name + "'", node.line);
            return;
        }
        targetType = sym->type;
    } else if (auto* ma = dynamic_cast<MemberAccessNode*>(node.target.get())) {
        targetType = inferType(*ma);
    }

    if (node.value) {
        TypeInfo valType = inferType(*node.value);
        if (!targetType.isUnknown() && !isAssignable(targetType, valType)) {
            reportError("Type Error: Cannot assign " + valType.toString() +
                        " to " + targetType.toString(), node.line);
        }
    }
}

void SemanticAnalyzer::visitIf(IfNode& node) {
    if (node.condition) {
        TypeInfo condType = inferType(*node.condition);
        if (!condType.isUnknown() && condType.kind != TypeInfo::Kind::BOOL) {
            reportError("Condition must be of type bool", node.line);
        }
    }

    if (node.thenBlock) {
        pushScope();
        node.thenBlock->accept(*this);
        popScope();
    }
    if (node.elseBlock) {
        pushScope();
        node.elseBlock->accept(*this);
        popScope();
    }
}

void SemanticAnalyzer::visitWhile(WhileNode& node) {
    if (node.condition) {
        TypeInfo condType = inferType(*node.condition);
        if (!condType.isUnknown() && condType.kind != TypeInfo::Kind::BOOL) {
            reportError("Condition must be of type bool", node.line);
        }
    }

    bool savedLoop = inLoop_;
    inLoop_ = true;
    if (node.body) {
        pushScope();
        node.body->accept(*this);
        popScope();
    }
    inLoop_ = savedLoop;
}

void SemanticAnalyzer::visitFor(ForNode& node) {
    pushScope(); // for-init variable scoped to the loop

    if (node.init)      node.init->accept(*this);
    if (node.condition) {
        TypeInfo condType = inferType(*node.condition);
        if (!condType.isUnknown() && condType.kind != TypeInfo::Kind::BOOL) {
            reportError("Condition must be of type bool", node.line);
        }
    }
    if (node.update)    inferType(*node.update);

    bool savedLoop = inLoop_;
    inLoop_ = true;
    if (node.body) node.body->accept(*this);
    inLoop_ = savedLoop;

    popScope();
}

void SemanticAnalyzer::visitReturn(ReturnNode& node) {
    TypeInfo retType;
    if (node.value) {
        retType = inferType(*node.value);
    } else {
        retType = TypeInfo(TypeInfo::Kind::VOID);
    }

    if (!currentReturnType_.isUnknown() &&
        !isAssignable(currentReturnType_, retType)) {
        reportError("Type Error: Return type mismatch — expected "
                    + currentReturnType_.toString()
                    + " but got " + retType.toString(), node.line);
    }
}

void SemanticAnalyzer::visitBreak(BreakNode& node) {
    if (!inLoop_) {
        reportError("'break' used outside of a loop", node.line);
    }
}

void SemanticAnalyzer::visitContinue(ContinueNode& node) {
    if (!inLoop_) {
        reportError("'continue' used outside of a loop", node.line);
    }
}

void SemanticAnalyzer::visitExprStmt(ExprStmtNode& node) {
    if (node.expr) inferType(*node.expr);
}

void SemanticAnalyzer::visitBinaryExpr(BinaryExprNode& node) {
    if (node.left)  node.left->accept(*this);
    if (node.right) node.right->accept(*this);
}

void SemanticAnalyzer::visitUnaryExpr(UnaryExprNode& node) {
    if (node.operand) node.operand->accept(*this);
}

void SemanticAnalyzer::visitPostfixExpr(PostfixExprNode& node) {
    inferType(node);
}

void SemanticAnalyzer::visitCallExpr(CallExprNode& node) {
    // Validate argument expressions
    for (auto& arg : node.args) {
        if (arg) arg->accept(*this);
    }
    // Function existence checked in inferType when used as a value
}

void SemanticAnalyzer::visitObjectCreation(ObjectCreationNode& node) {
    if (classes_.find(node.className) == classes_.end()) {
        reportError("Unknown class '" + node.className + "'", node.line);
    }
    for (auto& arg : node.args) {
        if (arg) arg->accept(*this);
    }
}

void SemanticAnalyzer::visitMemberAccess(MemberAccessNode& node) {
    if (node.object) node.object->accept(*this);
}

void SemanticAnalyzer::visitLiteral(LiteralNode& /*node*/) {
    // Nothing to check
}

void SemanticAnalyzer::visitIdentifier(IdentifierNode& node) {
    if (!lookup(node.name)) {
        reportError("Undeclared variable '" + node.name + "'", node.line);
    }
}

} // namespace DNA
