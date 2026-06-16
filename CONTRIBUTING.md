# Contributing to DNA

Thank you for your interest in contributing to the DNA Programming Language and the Ribosome compiler! This document outlines our guidelines for code development, branch management, quality standards, and code styles.

As an open-source project, we welcome contributions that improve stability, implement planned features, or extend language documentation.

---

## Branch Strategy

DNA follows a Greek-alphabet branching model.

- alpha
  Primary development branch and default repository branch.
  All active development occurs here.

- beta
  Release validation branch.
  Features from alpha are stabilized and tested before release.

- gamma
  Experimental branch for major features, architectural changes, and research work.

- delta
  Large-scale rewrites, compiler backend experiments, or long-running development efforts.

### Contributor Branches

Contributors should create branches from alpha:

feature/<feature-name>
bugfix/<issue-name>
docs/<topic>

Examples:

feature/arrays
feature/generics
bugfix/parser-crash
docs/runtime-guide

---

## Pull Request Workflow

1. **Fork & Clone:** Fork the repository on GitHub and clone your fork locally.
2. **Create Branch:** Create a branch from the current development branch (typically alpha).
3. **Implement Changes:** Write clean, modular code. Make sure to:
   - Preserve all existing comments, design documents, and docstrings that are unrelated to your edits.
   - Guard against null dereferences and exception panics in parsing and backend stages.
4. **Run Tests Locally:** Verify all tests pass successfully before pushing:
   ```bash
   python tests/run_tests.py
   ```
5. **Push & Open PR:** Push your branch to your fork and submit a Pull Request (PR) against the alpha branch unless otherwise specified.
6. **Code Review:** All PRs must receive at least one code review approval and pass all CI checks before merge.

---

## Coding Style

### Compiler Source Code (C++)
The Ribosome compiler is written in modern C++17. We adhere to the following coding conventions:
- **Naming Conventions:**
  - Classes and Structs: PascalCase (e.g., `SemanticAnalyzer`, `IRGenerator`).
  - Functions and Methods: camelCase (e.g., `compile`, `resolveOperand`).
  - Variables: camelCase (e.g., `varName`, `llvmType`).
  - Constants and Enums: ALL_CAPS (e.g., `OP_ADD`, `MAX_INT`).
- **Formatting:** Indentation of 4 spaces (no tabs). Clean brace placement matching existing code patterns.
- **Error Handling:** Avoid raw pointer dereferences. Check for `nullptr` explicitly. For backend errors, throw descriptive `std::runtime_error` rather than failing silently.

### Runtime Library Code (C++)
- Use C-linkage (`extern "C"`) declarations for all compiler-facing APIs.
- Prefix all runtime symbols with `dna_` to prevent namespaces collisions (e.g., `dna_print_string`).

### Python Scripts (Testing & Fuzzing)
- Write PEP 8 compliant Python code.
- Keep test configuration files parameterized and clean.

---

## Test Requirements

High stability is a core design pillar of DNA. Any code contribution must meet these testing criteria:
- **No Regression:** All existing 157 automated tests must pass.
- **New Features:** PRs introducing new constructs must include matching test cases in their respective directories under `tests/` (e.g., `tests/parser/` or `tests/runtime/`).
- **Bug Fixes:** Every resolved issue must be accompanied by a regression test case added to [tests/regression/](file:///e:/Helix/dna-lang/tests/regression/) to prevent future recurrence.
- **Fuzz Verification:** Run the fuzzer program in stress mode for at least 1,000 runs to verify your changes do not introduce compiler hangs, timeouts, or unhandled access violations:
  ```bash
  python tests/fuzz_test.py --mode stress --runs 1000
  ```

---

## Documentation Requirements

- **Inline Comments:** Maintain documentation integrity by keeping inline comments accurate. When refactoring functions, update the corresponding comments.
- **Architecture Updates:** If an internal API or intermediate representation opcode changes, document it in [COMPILER_ARCHITECTURE.md](file:///e:/Helix/dna-lang/docs/architecture/COMPILER_ARCHITECTURE.md) and the relevant spec files under `docs/`.
- **Language Spec:** Any new language feature must have its grammar rules and implementation status recorded in [DNA_LANGUAGE_SPEC_v1.md](file:///e:/Helix/dna-lang/docs/specifications/DNA_LANGUAGE_SPEC_v1.md).

---

Project Maintainer

Abhishek Gour

Helix Project
DNA Programming Language
Official Compiler: Ribosome
