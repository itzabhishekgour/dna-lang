#pragma once

#include "IRInstruction.h"
#include <string>
#include <utility>
#include <vector>

// =============================================================================
//  DNA IR v0.1 — Program Representation
//  File: ir/IRProgram.h
//
//  IRProgram is the top-level container produced by the IRGenerator.
//  It has two levels:
//
//    IRProgram
//      ├── classes[]     — class metadata (fields only; methods are functions)
//      └── functions[]   — all functions in source order
//            ├── name, returnType, params
//            └── instructions[]  — 3AC body (no FUNC/END_FUNC records here;
//                                   those are emitted by the printer)
//
//  Class methods are stored as regular functions with a qualified name:
//    Student::greet   Student::construct
// =============================================================================

#include <unordered_map>

namespace DNA {

// -----------------------------------------------------------------------------
// IRClassField — one field inside a class definition
// -----------------------------------------------------------------------------
struct IRClassField {
    std::string typeName;
    std::string name;
};

// -----------------------------------------------------------------------------
// IRClass — class metadata (no IR code; used for type resolution)
// -----------------------------------------------------------------------------
struct IRClass {
    std::string              name;
    std::vector<IRClassField> fields;
};

// -----------------------------------------------------------------------------
// IRFunction — one function or method definition
// -----------------------------------------------------------------------------
struct IRFunction {
    std::string name;            // qualified if method: "Student::greet"
    std::string returnTypeName;  // "int", "void", "String", "Student", …
    bool        isVoid = false;

    // Ordered parameter list: { typeName, paramName }
    std::vector<std::pair<std::string, std::string>> params;

    // Three-address-code body (ALLOC, STORE, LOAD, …)
    // Does NOT include FUNC / END_FUNC — those are printer concerns.
    std::vector<IRInstruction> instructions;

    // Map of variable names (locals and params) to their type names (e.g. "int", "bool")
    std::unordered_map<std::string, std::string> localVars;
};

// -----------------------------------------------------------------------------
// IRProgram — root of the IR tree
// -----------------------------------------------------------------------------
struct IRProgram {
    std::string sourceFile;   // path to the .dna file that was compiled

    // Class definitions (in declaration order)
    std::vector<IRClass> classes;

    // All functions — top-level actions and class methods — in source order
    std::vector<IRFunction> functions;
};

} // namespace DNA
