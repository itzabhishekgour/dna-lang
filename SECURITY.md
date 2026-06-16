# Security Policy

This document outlines the security policies, supported versions, vulnerability reporting procedures, and current security-related architecture limitations for the DNA Programming Language and the reference Ribosome compiler.

---

## 1. Supported Versions

We currently only provide security fixes and maintenance updates for the active release branch:

| Version | Supported | Notes |
| :--- | :---: | :--- |
| **v0.2.x Alpha** | **Yes** | Active development branch. Security fixes are backported here. |
| **< v0.2.0** | **No** | Legacy/prototype versions are not supported. |

---

## 2. Reporting a Vulnerability

We take compiler security and output executable stability seriously. If you discover a security vulnerability (such as a compiler crash leading to arbitrary code execution during compilation, or a runtime library memory corruption flaw), please follow these steps:

1. **Do not open a public issue.** Security issues should be reported privately to prevent premature disclosure.
2. **Email the project author:** Please send a detailed report to the creator, **Abhishek Gour**, describing:
   - The nature of the vulnerability.
   - A minimal reproducible example DNA file.
   - The compiler flags used during execution.
   - The expected vs. actual behavior.
3. **Response Timeline:** We aim to acknowledge receipt of reports within 48 hours and provide a resolution plan within 7 days.

---

## 3. Security-Related Architectural Limitations

As the project is currently in the **Alpha** phase, developers should be aware of the following security-related limitations:

### String Memory Leaks
- **Issue:** The runtime library string deallocation routine `dna_string_free` is currently defined as a stub that performs no operation.
- **Risk:** Any future dynamic allocations (such as string concatenation or copies) will result in persistent memory leaks within long-running processes. Applications must avoid continuous string allocations in loop bodies in production-like scenarios.

### Parser AST Recovery & Null Nodes
- **Issue:** When syntax errors occur, the parser synchronizes and skips invalid tokens. If error recovery discards vital declarations, some AST nodes may contain `nullptr` child references.
- **Risk:** Although the semantic analyzer is designed to safeguard against dereferencing null nodes, complex invalid syntax combinations could bypass compiler guardrails and cause a null-pointer crash (denial of service of compilation).

### Backend Exception Panic
- **Issue:** If invalid or manual IR instructions are fed directly to the codegen stage bypassing the semantic analyzer, the backend (`LLVMCodeGen::resolveOperand`) throws a `std::runtime_error`.
- **Risk:** This results in an unhandled C++ exception, terminating the compilation process immediately without a graceful cleanup of temporary target files.

---

*DNA Programming Language is created and developed by Abhishek Gour.*
