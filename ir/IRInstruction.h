#pragma once

#include <string>
#include <vector>

// =============================================================================
//  DNA IR v0.1 — Instruction Definitions
//  File: ir/IRInstruction.h
//
//  An IRInstruction is a three-address-code (3AC) style record:
//
//      dest = OPCODE  operand0  operand1  ...
//
//  If dest is empty, the instruction has no output (e.g. STORE, JUMP, PRINT).
//  Operands are strings — either named variables, temporaries ("t0"), labels
//  ("L_0"), or literal values ("42", "3.14", "\"hello\"").
//
//  Temporaries are fresh SSA-like names generated per function: t0, t1, ...
//  Labels are globally unique:  label-prefix_N
// =============================================================================

namespace DNA {

// -----------------------------------------------------------------------------
// OpCode — complete instruction set for DNA IR v0.1
// -----------------------------------------------------------------------------
enum class OpCode {
    // ── Variable operations ───────────────────────────────────────────────────
    ALLOC,          // ALLOC var              — declare stack slot for variable
    STORE,          // STORE var  value       — write value into variable
    LOAD,           // dest = LOAD var        — read variable into temp
    MOV,            // dest = MOV  value      — copy temp/literal (no variable)

    // ── Arithmetic ────────────────────────────────────────────────────────────
    ADD,            // dest = ADD  lhs  rhs
    SUB,            // dest = SUB  lhs  rhs
    MUL,            // dest = MUL  lhs  rhs
    DIV,            // dest = DIV  lhs  rhs
    MOD,            // dest = MOD  lhs  rhs
    NEG,            // dest = NEG  val         — unary minus

    // ── Comparison ────────────────────────────────────────────────────────────
    CMP_EQ,         // dest = CMP_EQ  lhs  rhs
    CMP_NE,         // dest = CMP_NE  lhs  rhs
    CMP_GT,         // dest = CMP_GT  lhs  rhs
    CMP_LT,         // dest = CMP_LT  lhs  rhs
    CMP_GE,         // dest = CMP_GE  lhs  rhs
    CMP_LE,         // dest = CMP_LE  lhs  rhs

    // ── Logical ───────────────────────────────────────────────────────────────
    LOG_AND,        // dest = AND  lhs  rhs
    LOG_OR,         // dest = OR   lhs  rhs
    LOG_NOT,        // dest = NOT  val

    // ── Control flow ──────────────────────────────────────────────────────────
    LABEL,          // LABEL  name            — branch target
    JUMP,           // JUMP   label
    JUMP_IF_TRUE,   // JUMP_IF_TRUE   cond  label
    JUMP_IF_FALSE,  // JUMP_IF_FALSE  cond  label

    // ── Functions ─────────────────────────────────────────────────────────────
    PARAM,          // PARAM  name            — declare incoming parameter
    CALL,           // dest = CALL  func  arg0  arg1  ...   (dest optional for void)
    RETURN,         // RETURN [value]         — empty operands == void return

    // ── Built-ins ─────────────────────────────────────────────────────────────
    PRINT,          // PRINT  value
    INPUT,          // dest = INPUT [prompt]

    // ── OOP ───────────────────────────────────────────────────────────────────
    NEW,            // dest = NEW  ClassName  arg0  arg1  ...
    GETFIELD,       // dest = GETFIELD  object  fieldName
    SETFIELD,       // SETFIELD  object  fieldName  value

    // ── IR-level annotations (human readability only) ─────────────────────────
    COMMENT,        // ; text
};

// -----------------------------------------------------------------------------
// IRInstruction
// -----------------------------------------------------------------------------
struct IRInstruction {
    OpCode                   op      = OpCode::COMMENT;
    std::string              dest;        // output register/variable (may be empty)
    std::vector<std::string> operands;   // source operands
    int                      srcLine = 0; // originating DNA source line number

    IRInstruction() = default;

    IRInstruction(OpCode o,
                  std::string d,
                  std::vector<std::string> ops,
                  int line = 0)
        : op(o), dest(std::move(d)), operands(std::move(ops)), srcLine(line) {}
};

// -----------------------------------------------------------------------------
// opcodeName() — canonical mnemonic string for an opcode
// -----------------------------------------------------------------------------
inline std::string opcodeName(OpCode op) {
    switch (op) {
        case OpCode::ALLOC:         return "ALLOC";
        case OpCode::STORE:         return "STORE";
        case OpCode::LOAD:          return "LOAD";
        case OpCode::MOV:           return "MOV";
        case OpCode::ADD:           return "ADD";
        case OpCode::SUB:           return "SUB";
        case OpCode::MUL:           return "MUL";
        case OpCode::DIV:           return "DIV";
        case OpCode::MOD:           return "MOD";
        case OpCode::NEG:           return "NEG";
        case OpCode::CMP_EQ:        return "CMP_EQ";
        case OpCode::CMP_NE:        return "CMP_NE";
        case OpCode::CMP_GT:        return "CMP_GT";
        case OpCode::CMP_LT:        return "CMP_LT";
        case OpCode::CMP_GE:        return "CMP_GE";
        case OpCode::CMP_LE:        return "CMP_LE";
        case OpCode::LOG_AND:       return "AND";
        case OpCode::LOG_OR:        return "OR";
        case OpCode::LOG_NOT:       return "NOT";
        case OpCode::LABEL:         return "LABEL";
        case OpCode::JUMP:          return "JUMP";
        case OpCode::JUMP_IF_TRUE:  return "JUMP_IF_TRUE";
        case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OpCode::PARAM:         return "PARAM";
        case OpCode::CALL:          return "CALL";
        case OpCode::RETURN:        return "RETURN";
        case OpCode::PRINT:         return "PRINT";
        case OpCode::INPUT:         return "INPUT";
        case OpCode::NEW:           return "NEW";
        case OpCode::GETFIELD:      return "GETFIELD";
        case OpCode::SETFIELD:      return "SETFIELD";
        case OpCode::COMMENT:       return ";";
        default:                    return "UNKNOWN";
    }
}

} // namespace DNA
