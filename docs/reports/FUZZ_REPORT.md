# Fuzzing Report: DNA Compiler Stability

This document details the fuzz testing results for the DNA Programming Language and the reference **Ribosome compiler**, version **v0.2.0 Alpha**.

---

## 1. Executive Summary

Stability is a primary design pillar of the Ribosome compiler. To guarantee that the compiler frontend degrades gracefully when presented with arbitrary malformed text, the codebase is validated against an automated fuzz testing harness (`tests/fuzz_test.py`).

| Metric | Quick Mode | Stress Mode |
| :--- | :---: | :---: |
| **Total Fuzz Invocations** | 1,000 | 10,000 |
| **Controlled Diagnostic Exits** | 1,000 | 10,000 |
| **Crashes / Segfaults** | 0 | 0 |
| **Hangs / Timeouts** | 0 | 0 |
| **Stability Assessment** | **100% Robust** | **100% Robust** |

---

## 2. Fuzzing Strategy & Methodology

The fuzz testing harness `tests/fuzz_test.py` generates test inputs using two complementary strategies:

1. **Random Character Generation (Garbage Inputs):** Emits completely unstructured text streams containing arbitrary ASCII sequences, control characters, and special symbols (e.g. `@#$%`, unbalanced braces, null bytes).
2. **Grammar-Aware Mutations (Semantic Mutations):** Mutates structurally valid DNA programs by inserting syntax errors, duplicate variable declarations, type mismatches, and calling invalid function references to challenge the compiler's diagnostic engine.

### Run Configuration
The compiler is executed via the check command:
```bash
ribosome.exe <fuzzed_input_file> --check
```
This forces the compiler to run lexical analysis, syntax parsing, AST construction, and semantic analysis without executing target linking or object file creation.

---

## 3. Crash Detection Criteria

The fuzzer monitors the execution of `ribosome.exe` and flags any of the following unhandled behaviors as failures:

- **Segmentation Faults / Access Violations:** Null-pointer dereferences or boundary errors in AST visitor traversals.
- **Aborts / Critical Signals:** Process abort signals or unhandled C++ exceptions.
- **Hangs / Timeouts:** Infinite loops inside lexical analyzers or parsers.
- **Unhandled Exceptions:** Uncaught exceptions traversing past compiler entry boundaries.

*Note: Controlled exit codes (where the compiler successfully identifies syntactic/semantic issues, displays diagnostic text, and exits with a non-zero exit status) are categorized as expected, valid executions.*

---

## 4. Assessment & Findings

- **Lexer Resilience:** The scanner correctly handles invalid UTF-8 sequences and control sequences, converting them to diagnostic error tokens instead of crashing.
- **Parser Synchronization:** When encountering malformed structures, the recursive descent parser successfully synchronizes on declaration or block brackets, preventing infinite recursion or cascading null node allocations in the AST.
- **Semantic Guardrails:** The semantic pass successfully catches invalid variable uses and mismatched types, preventing corrupt instructions from feeding down to the LLVM backend.

---

*DNA Programming Language is created and developed by Abhishek Gour.*
