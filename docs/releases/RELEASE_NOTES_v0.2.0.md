# Release Notes: DNA v0.2.0 Alpha

We are excited to announce the first official public alpha release of the **DNA Programming Language** and the reference **Ribosome compiler** (**v0.2.0 Alpha**). 

This release marks a significant milestone, establishing a fully functional end-to-end compiler pipeline that compiles DNA source code directly into native Windows x64 executables.

---

## Summary

DNA is a native, compiled, object-oriented programming language designed for minimal boilerplate, fast compilation, and high performance without a garbage collector. Version `v0.2.0 Alpha` is the initial public release, intended for early developer evaluation, testing, and feedback. 

While the compiler pipeline is highly stable and robust—hardened by comprehensive fuzzing and regression testing—it is classified as **Alpha** because major language features (such as class code generation, arrays, and standard libraries) are not yet implemented or exist only as specifications.

---

## Major Features

- **End-to-End Compilation Pipeline:** Translates `.dna` source code into target machine-code object files (`.obj`) and invokes the linker to produce stand-alone Windows executables (`.exe`).
- **Custom Three-Address Code (DNA IR):** Lowers AST to a flat, SSA-like intermediate representation suitable for future optimizations.
- **Robust Variable & Control Flow Systems:** Fully supports local variables, binary/unary operators, nested `if-else` branches, and loop structures (`while` and `for`) with proper `break` and `continue` semantics.
- **Windows x64 Calling Convention Support:** Implements Win64 ABI struct passing and return-by-reference mechanics for the `DNAString` struct layout (`%DNAString = type { ptr, i32, i32 }`), allowing seamless integration with C/C++ libraries.
- **Failsafe Compiler Design:** Frontend recursive descent parser features token synchronization to prevent cascading parser error reports. The compiler is tested to recover cleanly and report diagnostic messages without crashing.

---

## Validation Results

Ribosome `v0.2.0 Alpha` has undergone extensive automated verification:

- **Automated Test Suite:** **157 test cases** executed with a **100% pass rate** via the `run_tests.py` harness.
  - *Lexer:* 20 tests validating tokenization boundaries.
  - *Parser:* 25 tests verifying grammar parsing and recovery.
  - *Semantic Analysis:* 35 tests validating type checking, scopes, and duplicate check rules.
  - *DNA IR:* 20 tests verifying 3AC instruction correctness.
  - *LLVM Backend:* 20 tests validating LLVM Assembly lowering.
  - *Runtime Execution:* 32 tests verifying executable exits and program output outputs.
  - *Regression:* 5 tests confirming resolution of previously reported compiler bugs.
- **Dual-Mode Fuzzing:** Verification using random mutations and grammar-aware fuzzing (`fuzz_test.py`):
  - *Quick Mode:* 1,000 runs completed (0 crashes, 0 hangs, 100% controlled exits on malformed files).
  - *Stress Mode:* 10,000 runs completed (0 crashes, 0 hangs, 100% controlled exits).

---

## Known Limitations

As an Alpha release, the compiler has several notable constraints:
- **Global Variables:** Global declarations are parsed and semantically validated but are **not** supported by the LLVM backend. Only stack-allocated local variables are lowered.
- **Unsupported Types:** Primitives `float`, `double`, and `char` are parsed but rejected during backend lowering.
- **Object-Oriented Programming (OOP) Backend:** Classes, fields, constructors, and methods are parsed and semantically checked, but code generation is **not** yet implemented in the LLVM backend.
- **Nested Conditional Branches:** `else if` is not natively implemented at the syntax level and must be written as nested `else { if ... }` blocks.
- **String Modifications:** String concatenation, duplication (`dna_string_copy`), and buffer freeing (`dna_string_free`) are defined in the runtime library API but exist only as stub implementations.
- **ABI Portability:** The backend calling conventions are hardcoded for the Windows x64 ABI. Compilation on Linux or macOS platforms will fail or produce unstable binaries.

---

## Supported Platforms

- **Operating System:** Windows 10/11 (x64)
- **Compiler Host Dependencies:** 
  - CMake 3.20+
  - MSBuild (Visual Studio Build Tools 2022)
  - LLVM 18 (installed and accessible in shell path)

---

## Next Milestone: DNA v0.3

The next development phase will focus on:
1. **Arrays:** Array declarations, indexing, slicing, and runtime memory allocation bounds checking.
2. **Memory Model:** Defining clear stack vs heap allocation rules and memory layout guidelines.

---

*DNA Programming Language is created and developed by Abhishek Gour.*
