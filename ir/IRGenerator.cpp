#include "IRGenerator.h"

#include <cassert>
#include <cctype>
#include <sstream>

// =============================================================================
//  DNA IR v0.1 — IR Generator implementation
//  File: ir/IRGenerator.cpp
// =============================================================================

namespace DNA {

// =============================================================================
// Public entry point
// =============================================================================
IRProgram IRGenerator::generate(ProgramNode& program) {
    program.accept(*this);
    return builder_.takeProgram();
}

// =============================================================================
// Helpers
// =============================================================================
std::string IRGenerator::emitExpr(ASTNode& node) {
    node.accept(*this);
    return lastResult_;
}

void IRGenerator::emit(OpCode op, const std::string& dest,
                       std::vector<std::string> ops, int line) {
    builder_.emit(op, dest, std::move(ops), line);
}

void IRGenerator::emit(OpCode op, std::vector<std::string> ops, int line) {
    builder_.emit(op, std::move(ops), line);
}

void IRGenerator::emit(OpCode op, int line) {
    builder_.emit(op, line);
}

std::string IRGenerator::qualifiedName(const std::string& name) const {
    if (currentClass_.empty()) return name;
    return currentClass_ + "::" + name;
}

// =============================================================================
// Top-level
// =============================================================================
void IRGenerator::visitProgram(ProgramNode& node) {
    for (auto& decl : node.declarations) {
        if (decl) decl->accept(*this);
    }
}

void IRGenerator::visitLoad(LoadNode& node) {
    // load statements have no IR equivalent in v0.1 — emit a comment.
    // We need a function context for emit(); skip if none exists.
    // (load is a top-level statement, no function is active yet.)
    // Store as a comment in a special preamble function if needed.
    // For now, simply ignore — the module system is not implemented.
    (void)node;
}

// =============================================================================
// Class lowering
// =============================================================================
void IRGenerator::visitClass(ClassNode& node) {
    // 1. Record class metadata
    IRClass cls;
    cls.name = node.name;
    for (auto& member : node.members) {
        if (auto* field = dynamic_cast<FieldDeclNode*>(member.get())) {
            cls.fields.push_back({ field->type.toString(), field->name });
        }
    }
    builder_.registerClass(std::move(cls));

    // 2. Generate code for methods and constructors inside this class scope
    currentClass_ = node.name;
    for (auto& member : node.members) {
        if (member) member->accept(*this);
    }
    currentClass_.clear();
}

void IRGenerator::visitFieldDecl(FieldDeclNode& /*node*/) {
    // Field declarations are pure metadata; no IR instructions are emitted.
    // (Handled in visitClass above.)
}

// =============================================================================
// Function lowering:  action ReturnType name(params) { body }
// =============================================================================
void IRGenerator::visitAction(ActionNode& node) {
    bool isVoid = node.returnType.isVoid();
    std::string fname = qualifiedName(node.name);

    builder_.beginFunction(fname, node.returnType.toString(), isVoid);

    // Add parameters to the function record only (the IRPrinter emits them)
    for (auto& p : node.params) {
        builder_.currentFunc().params.emplace_back(p.type.toString(), p.name);
        builder_.currentFunc().localVars[p.name] = p.type.toString();
        // NOTE: Do NOT emit(PARAM) here — the printer reads from IRFunction.params
    }

    // Body
    if (node.body) node.body->accept(*this);

    // Implicit void return if the last instruction is not RETURN
    if (isVoid) {
        auto& instrs = builder_.currentFunc().instructions;
        bool hasReturn = !instrs.empty() &&
                         instrs.back().op == OpCode::RETURN;
        if (!hasReturn) {
            emit(OpCode::RETURN, {}, node.line);
        }
    }
}

// =============================================================================
// Constructor lowering:  construct(params) { body }
// =============================================================================
void IRGenerator::visitConstruct(ConstructNode& node) {
    std::string fname = qualifiedName("construct");
    builder_.beginFunction(fname, currentClass_, false);

    for (auto& p : node.params) {
        builder_.currentFunc().params.emplace_back(p.type.toString(), p.name);
        builder_.currentFunc().localVars[p.name] = p.type.toString();
    }

    if (node.body) node.body->accept(*this);

    // Constructor has an implicit "return this" — for v0.1 emit RETURN
    emit(OpCode::RETURN, {}, node.line);
}

// =============================================================================
// Block and statement lowering
// =============================================================================
void IRGenerator::visitBlock(BlockNode& node) {
    for (auto& stmt : node.statements) {
        if (stmt) stmt->accept(*this);
    }
}

// ── VarDecl:  Type name [= initializer] ───────────────────────────────────────
void IRGenerator::visitVarDecl(VarDeclNode& node) {
    // ALLOC declares the stack slot
    builder_.currentFunc().localVars[node.name] = node.type.toString();
    emit(OpCode::ALLOC, { node.name }, node.line);

    // STORE writes the initial value (if present)
    if (node.initializer) {
        std::string val = emitExpr(*node.initializer);
        emit(OpCode::STORE, { node.name, val }, node.line);
    }
}

// ── Assignment:  target = value ───────────────────────────────────────────────
void IRGenerator::visitAssignment(AssignmentNode& node) {
    std::string val = emitExpr(*node.value);

    if (auto* id = dynamic_cast<IdentifierNode*>(node.target.get())) {
        // Simple variable assignment
        emit(OpCode::STORE, { id->name, val }, node.line);
    } else if (auto* ma = dynamic_cast<MemberAccessNode*>(node.target.get())) {
        // Field assignment: SETFIELD object field value
        std::string obj = emitExpr(*ma->object);
        emit(OpCode::SETFIELD, { obj, ma->member, val }, node.line);
    }
}

// ── if / else ─────────────────────────────────────────────────────────────────
void IRGenerator::visitIf(IfNode& node) {
    std::string cond = emitExpr(*node.condition);

    if (node.elseBlock) {
        // if-else
        std::string elseLabel = newLabel("if_else");
        std::string endLabel  = newLabel("if_end");

        emit(OpCode::JUMP_IF_FALSE, { cond, elseLabel }, node.line);
        if (node.thenBlock) node.thenBlock->accept(*this);
        emit(OpCode::JUMP, { endLabel }, node.line);
        emit(OpCode::LABEL, { elseLabel }, node.line);
        node.elseBlock->accept(*this);
        emit(OpCode::LABEL, { endLabel }, node.line);
    } else {
        // if only
        std::string endLabel = newLabel("if_end");

        emit(OpCode::JUMP_IF_FALSE, { cond, endLabel }, node.line);
        if (node.thenBlock) node.thenBlock->accept(*this);
        emit(OpCode::LABEL, { endLabel }, node.line);
    }
}

// ── while ─────────────────────────────────────────────────────────────────────
//   LABEL while_start
//   [condition]
//   JUMP_IF_FALSE cond while_end
//   [body]
//   JUMP while_start
//   LABEL while_end
void IRGenerator::visitWhile(WhileNode& node) {
    std::string startLabel = newLabel("while_start");
    std::string endLabel   = newLabel("while_end");

    loopStack_.push_back({ startLabel, endLabel });

    emit(OpCode::LABEL, { startLabel }, node.line);
    std::string cond = emitExpr(*node.condition);
    emit(OpCode::JUMP_IF_FALSE, { cond, endLabel }, node.line);

    if (node.body) node.body->accept(*this);

    emit(OpCode::JUMP, { startLabel }, node.line);
    emit(OpCode::LABEL, { endLabel }, node.line);

    loopStack_.pop_back();
}

// ── for ───────────────────────────────────────────────────────────────────────
//   [init]
//   LABEL for_start
//   [condition]
//   JUMP_IF_FALSE cond for_end
//   [body]
//   [update]
//   JUMP for_start
//   LABEL for_end
void IRGenerator::visitFor(ForNode& node) {
    // Initialiser (e.g. int i = 0)
    if (node.init) node.init->accept(*this);

    std::string startLabel = newLabel("for_start");
    std::string endLabel   = newLabel("for_end");

    loopStack_.push_back({ startLabel, endLabel });

    emit(OpCode::LABEL, { startLabel }, node.line);
    std::string cond = emitExpr(*node.condition);
    emit(OpCode::JUMP_IF_FALSE, { cond, endLabel }, node.line);

    if (node.body) node.body->accept(*this);

    // Update expression (e.g. i++) — result is discarded
    if (node.update) emitExpr(*node.update);

    emit(OpCode::JUMP, { startLabel }, node.line);
    emit(OpCode::LABEL, { endLabel }, node.line);

    loopStack_.pop_back();
}

// ── return ────────────────────────────────────────────────────────────────────
void IRGenerator::visitReturn(ReturnNode& node) {
    if (node.value) {
        std::string val = emitExpr(*node.value);
        emit(OpCode::RETURN, { val }, node.line);
    } else {
        emit(OpCode::RETURN, {}, node.line);
    }
}

// ── break ─────────────────────────────────────────────────────────────────────
void IRGenerator::visitBreak(BreakNode& node) {
    if (!loopStack_.empty()) {
        emit(OpCode::JUMP, { loopStack_.back().endLabel }, node.line);
    }
}

// ── continue ──────────────────────────────────────────────────────────────────
void IRGenerator::visitContinue(ContinueNode& node) {
    if (!loopStack_.empty()) {
        emit(OpCode::JUMP, { loopStack_.back().startLabel }, node.line);
    }
}

// ── expression statement ──────────────────────────────────────────────────────
void IRGenerator::visitExprStmt(ExprStmtNode& node) {
    if (node.expr) {
        node.expr->accept(*this);
        // lastResult_ is the produced value, but we discard it (statement context)
    }
}

// =============================================================================
// Expression lowering — each sets lastResult_ to the value name
// =============================================================================

// ── Binary expression ─────────────────────────────────────────────────────────
//   t0 = LOAD a
//   t1 = LOAD b     (identifiers are always loaded; literals are inline)
//   t2 = OP t0 t1
void IRGenerator::visitBinaryExpr(BinaryExprNode& node) {
    std::string lhs = emitExpr(*node.left);
    std::string rhs = emitExpr(*node.right);

    OpCode op;
    const std::string& s = node.op;
    if      (s == "+")  op = OpCode::ADD;
    else if (s == "-")  op = OpCode::SUB;
    else if (s == "*")  op = OpCode::MUL;
    else if (s == "/")  op = OpCode::DIV;
    else if (s == "%")  op = OpCode::MOD;
    else if (s == "==") op = OpCode::CMP_EQ;
    else if (s == "!=") op = OpCode::CMP_NE;
    else if (s == ">")  op = OpCode::CMP_GT;
    else if (s == "<")  op = OpCode::CMP_LT;
    else if (s == ">=") op = OpCode::CMP_GE;
    else if (s == "<=") op = OpCode::CMP_LE;
    else if (s == "&&") op = OpCode::LOG_AND;
    else if (s == "||") op = OpCode::LOG_OR;
    else                op = OpCode::ADD; // fallback

    std::string dest = newTemp();
    emit(op, dest, { lhs, rhs }, node.line);
    lastResult_ = dest;
}

// ── Unary expression  (! -) ───────────────────────────────────────────────────
void IRGenerator::visitUnaryExpr(UnaryExprNode& node) {
    std::string val = emitExpr(*node.operand);
    std::string dest = newTemp();

    if (node.op == "!") {
        emit(OpCode::LOG_NOT, dest, { val }, node.line);
    } else if (node.op == "-") {
        emit(OpCode::NEG, dest, { val }, node.line);
    }
    lastResult_ = dest;
}

// ── Postfix  (i++  i--) ───────────────────────────────────────────────────────
//   t_old = LOAD i
//   t_new = ADD t_old 1   (or SUB for --)
//   STORE i t_new
//   lastResult_ = t_old   (postfix returns the *old* value)
void IRGenerator::visitPostfixExpr(PostfixExprNode& node) {
    // Get the name of the variable being incremented
    std::string varName;
    if (auto* id = dynamic_cast<IdentifierNode*>(node.operand.get())) {
        varName = id->name;
    }

    std::string tOld = newTemp();
    emit(OpCode::LOAD, tOld, { varName }, node.line);

    std::string tNew  = newTemp();
    OpCode      arith = (node.op == "++") ? OpCode::ADD : OpCode::SUB;
    emit(arith, tNew, { tOld, "1" }, node.line);

    if (!varName.empty()) {
        emit(OpCode::STORE, { varName, tNew }, node.line);
    }

    lastResult_ = tOld; // postfix semantics: old value is the expression result
}

// ── Function / method call ─────────────────────────────────────────────────────
void IRGenerator::visitCallExpr(CallExprNode& node) {
    // ── Built-in: print(value) ────────────────────────────────────────────────
    if (node.callee == "print") {
        if (!node.args.empty()) {
            std::string val = emitExpr(*node.args[0]);
            emit(OpCode::PRINT, { val }, node.line);
        }
        lastResult_ = "";
        return;
    }

    // ── Built-in: input([prompt]) ─────────────────────────────────────────────
    if (node.callee == "input") {
        std::string dest = newTemp();
        if (!node.args.empty()) {
            std::string prompt = emitExpr(*node.args[0]);
            emit(OpCode::INPUT, dest, { prompt }, node.line);
        } else {
            emit(OpCode::INPUT, dest, {}, node.line);
        }
        lastResult_ = dest;
        return;
    }

    // ── Method call:  callee starts with '.'  (e.g. ".greet") ────────────────
    // The encoded arg list is: [MemberAccessNode(receiver), arg1, arg2, ...]
    if (!node.callee.empty() && node.callee[0] == '.') {
        std::string methodName = node.callee.substr(1);

        // Emit the receiver object (first arg is a MemberAccessNode containing it)
        std::string receiver;
        std::size_t firstRealArg = 0;
        if (!node.args.empty()) {
            if (auto* ma = dynamic_cast<MemberAccessNode*>(node.args[0].get())) {
                receiver = emitExpr(*ma->object);
                firstRealArg = 1;
            }
        }

        // Evaluate remaining real arguments
        std::vector<std::string> ops = {"." + methodName, receiver};
        for (std::size_t i = firstRealArg; i < node.args.size(); ++i) {
            ops.push_back(emitExpr(*node.args[i]));
        }

        std::string dest = newTemp();
        emit(OpCode::CALL, dest, std::move(ops), node.line);
        lastResult_ = dest;
        return;
    }

    // ── Regular function call ─────────────────────────────────────────────────
    std::vector<std::string> ops = { node.callee };
    for (auto& arg : node.args) {
        ops.push_back(emitExpr(*arg));
    }

    std::string dest = newTemp();
    emit(OpCode::CALL, dest, std::move(ops), node.line);
    lastResult_ = dest;
}

// ── Object creation:  Student("Alice", 21) ────────────────────────────────────
//   t0 = NEW Student "Alice" 21
void IRGenerator::visitObjectCreation(ObjectCreationNode& node) {
    std::vector<std::string> ops = { node.className };
    for (auto& arg : node.args) {
        ops.push_back(emitExpr(*arg));
    }

    std::string dest = newTemp();
    emit(OpCode::NEW, dest, std::move(ops), node.line);
    lastResult_ = dest;
}

// ── Member access:  s.name ────────────────────────────────────────────────────
//   t0 = LOAD s
//   t1 = GETFIELD t0 name
void IRGenerator::visitMemberAccess(MemberAccessNode& node) {
    std::string obj  = emitExpr(*node.object);
    std::string dest = newTemp();
    emit(OpCode::GETFIELD, dest, { obj, node.member }, node.line);
    lastResult_ = dest;
}

// ── Literal ───────────────────────────────────────────────────────────────────
// Literals are used inline as operands — no instruction is emitted.
// String literals are wrapped in quotes in the IR text.
void IRGenerator::visitLiteral(LiteralNode& node) {
    switch (node.kind) {
        case LiteralNode::Kind::STRING:
            // Preserve the quoted form in the IR
            lastResult_ = "\"" + node.value + "\"";
            break;
        case LiteralNode::Kind::CHAR:
            lastResult_ = "'" + node.value + "'";
            break;
        case LiteralNode::Kind::BOOL_TRUE:
            lastResult_ = "true";
            break;
        case LiteralNode::Kind::BOOL_FALSE:
            lastResult_ = "false";
            break;
        default:
            // INT, FLOAT, DOUBLE — raw numeric string
            lastResult_ = node.value;
            break;
    }
}

// ── Identifier ────────────────────────────────────────────────────────────────
//   t0 = LOAD varName
void IRGenerator::visitIdentifier(IdentifierNode& node) {
    std::string dest = newTemp();
    emit(OpCode::LOAD, dest, { node.name }, node.line);
    lastResult_ = dest;
}

} // namespace DNA
