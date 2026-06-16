# Changelog

All notable changes to the DNA Programming Language and the Ribosome compiler will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [v0.2.0 Alpha] - 2026-06-16

This is the first public release of the DNA Programming Language and the reference Ribosome compiler. It introduces the full end-to-end compilation pipeline, generating native Windows x64 executables from DNA source files.

### Added

- **Lexer**
  - Robust scanner implementation converting character streams into a sequence of structured tokens.
  - Complete keyword tokenization support for primitives, loops, conditionals, actions, and module loading.
- **Parser**
  - Pratt-parsing-based recursive descent compiler parser.
  - Structural statement recovery and synchronization boundaries to prevent compilation cascading failures on malformed syntax.
- **Abstract Syntax Tree (AST)**
  - Full tree definition representing statement blocks, function definitions, control structures, operations, and declarations.
  - AST Visitor structure enabling recursive verification, printing, and analysis passes.
- **Semantic Analysis**
  - Context and scope resolution for variables and function parameters.
  - Type checking constraints safeguarding operations, variable declarations, and function invocation contracts.
  - Verification of unique program entrypoint `main()` and prevention of duplicate local declarations.
- **DNA Intermediate Representation (DNA IR)**
  - Platform-independent flat three-address-code (3AC) design.
  - Custom SSA-like variables stack-allocation strategies (`ALLOC`, `LOAD`, `STORE`) and intermediate virtual temporaries.
  - Explicit control-flow blocks lowering statements to conditional and unconditional jumps.
  - Clean human-readable IR text output generation via `IRPrinter`.
- **LLVM Backend**
  - LLVM 18 IR generation lowering DNA IR instructions to LLVM SSA structure.
  - ABI mapper implementing Microsoft x64 Calling Convention constraints.
  - Target machine configuration lowering LLVM IR directly to Windows native object (`.obj`) files.
- **Native Executable Generation**
  - Dynamic discovery of host Visual Studio Build Tools linker.
  - Native linker integration stitching target object files and the C runtime with `dnaruntime.lib` to produce standard `.exe` executables.
- **Runtime Library**
  - Static library runtime layer (`dnaruntime.lib`) written in C++ with C-linkage APIs.
  - Built-in `print(int)` and runtime support routines.
- **String Runtime**
  - Struct layout `%DNAString = type { ptr, i32, i32 }` matching the Windows C++ runtime definition `DNAString`.
  - ABI compliance rules: passed and returned by reference (using `sret` temporary stack allocation).
  - String print implementation calling `dna_print_string` from the compiler.
  - Basic runtime API stub interfaces for future string operations (`dna_string_copy`, `dna_string_concat`, and `dna_string_free`).
- **Automated Testing**
  - Automated test runner script (`tests/run_tests.py`) verifying compiler execution outputs and exit statuses.
  - 157-case test suite covering Lexer, Parser, Semantic Analysis, IR, LLVM codegen, regression fixes, and program runtime executions.
- **Fuzzing**
  - Parameterized test fuzzer (`tests/fuzz_test.py`) with grammar-broken input generator.
  - Validation of compiler recovery correctness, verifying that the frontend reports compilation diagnostics and exits cleanly instead of crashing or hanging on garbage inputs.

---

*DNA Programming Language is created and developed by Abhishek Gour.*
