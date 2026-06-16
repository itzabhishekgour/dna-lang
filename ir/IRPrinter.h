#pragma once

#include "IRInstruction.h"
#include "IRProgram.h"
#include <iostream>
#include <ostream>
#include <string>

// =============================================================================
//  DNA IR v0.1 — IR Printer
//  File: ir/IRPrinter.h
//
//  Formats an IRProgram as human-readable DNA IR text.
//
//  Output format:
//
//    CLASS Student
//      FIELD String name
//      FIELD int    age
//    END_CLASS
//
//    FUNC add
//
//      PARAM a
//      PARAM b
//
//      t0 = LOAD a
//      t1 = LOAD b
//      t2 = ADD  t0 t1
//
//      RETURN t2
//
//    END_FUNC
//
//  Indentation rules:
//    - FUNC, END_FUNC, CLASS, END_CLASS, LABEL: column 0
//    - All body instructions:                   two-space indent
//    - COMMENT (;):                             no indent
//
//  Blank line rules:
//    - One blank line after FUNC / CLASS header
//    - One blank line after the last PARAM (if any params)
//    - One blank line before every LABEL
//    - One blank line after every RETURN
//    - One blank line before END_FUNC / END_CLASS
//    - One blank line after END_FUNC to separate functions
// =============================================================================

namespace DNA {

class IRPrinter {
public:
    explicit IRPrinter(std::ostream& out = std::cout) : out_(out) {}

    // ── Public API ────────────────────────────────────────────────────────────

    void print(const IRProgram& program) {
        // Class definitions first
        for (const auto& cls : program.classes) {
            printClass(cls);
            out_ << "\n";
        }

        // Then all functions
        for (const auto& fn : program.functions) {
            printFunction(fn);
            out_ << "\n";
        }
    }

private:
    std::ostream& out_;

    // ── Internal helpers ──────────────────────────────────────────────────────

    void printClass(const IRClass& cls) {
        out_ << "CLASS " << cls.name << "\n\n";
        for (const auto& field : cls.fields) {
            // Align field type names to 8 characters for readability
            out_ << "  FIELD " << field.typeName;
            // Pad with spaces so field names align (up to 8 chars)
            int pad = 8 - static_cast<int>(field.typeName.size());
            if (pad > 0) out_ << std::string(pad, ' ');
            out_ << field.name << "\n";
        }
        out_ << "\nEND_CLASS\n";
    }

    void printFunction(const IRFunction& fn) {
        // FUNC header
        out_ << "FUNC " << fn.name << "\n\n";

        // Parameters
        for (const auto& [typeName, paramName] : fn.params) {
            out_ << "  PARAM " << paramName << "\n";
        }
        if (!fn.params.empty()) out_ << "\n";

        // Body instructions
        const auto& instrs = fn.instructions;
        for (std::size_t i = 0; i < instrs.size(); ++i) {
            const IRInstruction& instr = instrs[i];

            // Blank line before LABEL
            if (instr.op == OpCode::LABEL && i > 0) out_ << "\n";

            printInstruction(instr);

            // Blank line after RETURN
            if (instr.op == OpCode::RETURN) out_ << "\n";
        }

        // Blank line before END_FUNC (unless we already have one from RETURN)
        if (!instrs.empty() && instrs.back().op != OpCode::RETURN) {
            out_ << "\n";
        }

        out_ << "END_FUNC\n";
    }

    void printInstruction(const IRInstruction& instr) {
        // COMMENT is unindented
        if (instr.op == OpCode::COMMENT) {
            if (!instr.operands.empty()) {
                out_ << "; " << instr.operands[0] << "\n";
            } else {
                out_ << ";\n";
            }
            return;
        }

        // LABEL is unindented, printed with trailing colon
        if (instr.op == OpCode::LABEL) {
            out_ << (instr.operands.empty() ? "LABEL" : instr.operands[0]) << ":\n";
            return;
        }

        // All other instructions are indented by two spaces
        out_ << "  ";

        // "dest = OPCODE operands..."   or   "OPCODE operands..."
        if (!instr.dest.empty()) {
            out_ << instr.dest << " = ";
        }

        out_ << opcodeName(instr.op);

        // Operands
        for (const auto& op : instr.operands) {
            out_ << " " << op;
        }

        out_ << "\n";
    }
};

} // namespace DNA
