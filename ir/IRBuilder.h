#pragma once

#include "IRInstruction.h"
#include "IRProgram.h"
#include <string>
#include <vector>

// =============================================================================
//  DNA IR v0.1 — IR Builder
//  File: ir/IRBuilder.h
//
//  IRBuilder is a thin stateful helper used by IRGenerator.
//  It manages:
//    - Temporary register names  (t0, t1, t2, …)  — reset per function
//    - Label names               (prefix_N)        — monotonically increasing
//    - The "current function" being populated
//    - The growing IRProgram
//
//  All emit*() helpers append to the current function's instruction list.
// =============================================================================

namespace DNA {

class IRBuilder {
public:
    IRBuilder() = default;

    // ── Naming ────────────────────────────────────────────────────────────────

    /// Allocate a fresh temporary: t0, t1, …
    /// Counter resets at the start of every function (via beginFunction).
    std::string newTemp() {
        return "t" + std::to_string(tempCount_++);
    }

    /// Allocate a globally unique label: prefix_N
    std::string newLabel(const std::string& prefix = "L") {
        return prefix + "_" + std::to_string(labelCount_++);
    }

    // ── Function lifecycle ────────────────────────────────────────────────────

    void beginFunction(const std::string& name,
                       const std::string& retType,
                       bool isVoid) {
        program_.functions.emplace_back();
        IRFunction& fn      = program_.functions.back();
        fn.name             = name;
        fn.returnTypeName   = retType;
        fn.isVoid           = isVoid;
        currentFunc_        = &fn;
        tempCount_          = 0; // reset temporaries for each new function
    }

    IRFunction& currentFunc() {
        return *currentFunc_;
    }

    // ── Class registration ────────────────────────────────────────────────────

    void registerClass(IRClass cls) {
        program_.classes.push_back(std::move(cls));
    }

    // ── Instruction emission ──────────────────────────────────────────────────

    /// Emit an instruction into the current function.
    void emit(OpCode op,
              const std::string& dest,
              std::vector<std::string> operands,
              int srcLine = 0) {
        currentFunc_->instructions.emplace_back(op, dest, std::move(operands), srcLine);
    }

    /// Convenience: emit with no destination.
    void emit(OpCode op,
              std::vector<std::string> operands,
              int srcLine = 0) {
        emit(op, "", std::move(operands), srcLine);
    }

    /// Convenience: emit with no destination and no operands.
    void emit(OpCode op, int srcLine = 0) {
        emit(op, "", {}, srcLine);
    }

    // ── Program access ────────────────────────────────────────────────────────

    IRProgram& program() { return program_; }
    IRProgram  takeProgram() { return std::move(program_); }

private:
    IRProgram   program_;
    IRFunction* currentFunc_ = nullptr;
    int         tempCount_   = 0;
    int         labelCount_  = 0;
};

} // namespace DNA
