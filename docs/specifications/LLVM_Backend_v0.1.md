# DNA Compiler (Ribosome) — LLVM Backend Specification v0.1

> **Compiler Stage:** DNA IR → LLVM IR → Object File → Native Executable (Windows)

This document specifies the architecture, instruction mapping, and code generation/linking flow of the LLVM backend (Phase 1) for the DNA programming language.

---

## 1. Backend Architecture

The backend consists of five core C++ components located in the `codegen/` folder:

*   **LLVMContextManager**: Simple wrapper holding the LLVM global context (`llvm::LLVMContext`), the target module (`llvm::Module`), and the active instruction builder (`llvm::IRBuilder<>`).
*   **LLVMTypeMapper**: Translates DNA type names ("int", "bool", "void") to their corresponding `llvm::Type*` equivalents. Unsupported types in Phase 1 fail fast by throwing exceptions.
*   **LLVMValueMapper**: Maintains separate namespaces for SSA intermediate results (`temps_` mapping to `llvm::Value*`) and stack allocations (`vars_` mapping to `llvm::AllocaInst*`) to ensure SSA safety.
*   **LLVMCodeGen**: The orchestrator which lowers functions and instructions from `IRProgram` to LLVM IR, coordinates basic block generation, and drives compilation to native object files.

---

## 2. LLVM Instruction Mapping

DNA IR instructions are mapped directly to LLVM instructions via the `llvm::IRBuilder<>` API:

| DNA IR OpCode | LLVM Instruction | Builder API Call |
|---|---|---|
| `ALLOC var` | Stack variable allocation | `Builder.CreateAlloca(Type, nullptr, var)` |
| `STORE var value` | Write to stack | `Builder.CreateStore(val, alloca)` |
| `dest = LOAD var` | Read from stack | `Builder.CreateLoad(Type, alloca, dest)` |
| `dest = MOV value` | Value aliasing / copy | Direct lookup propagation in ValueMapper |
| `dest = ADD lhs rhs` | Addition | `Builder.CreateAdd(l, r, dest)` |
| `dest = SUB lhs rhs` | Subtraction | `Builder.CreateSub(l, r, dest)` |
| `dest = MUL lhs rhs` | Multiplication | `Builder.CreateMul(l, r, dest)` |
| `dest = DIV lhs rhs` | Signed Division | `Builder.CreateSDiv(l, r, dest)` |
| `dest = MOD lhs rhs` | Signed Remainder | `Builder.CreateSRem(l, r, dest)` |
| `dest = NEG val` | Negation (Unary `-`) | `Builder.CreateNeg(v, dest)` |
| `dest = CMP_EQ lhs rhs` | Comparison `==` | `Builder.CreateICmpEQ(l, r, dest)` |
| `dest = CMP_NE lhs rhs` | Comparison `!=` | `Builder.CreateICmpNE(l, r, dest)` |
| `dest = CMP_GT lhs rhs` | Comparison `>` (Signed) | `Builder.CreateICmpSGT(l, r, dest)` |
| `dest = CMP_LT lhs rhs` | Comparison `<` (Signed) | `Builder.CreateICmpSLT(l, r, dest)` |
| `dest = CMP_GE lhs rhs` | Comparison `>=` (Signed) | `Builder.CreateICmpSGE(l, r, dest)` |
| `dest = CMP_LE lhs rhs` | Comparison `<=` (Signed) | `Builder.CreateICmpSLE(l, r, dest)` |
| `dest = AND lhs rhs` | Logical AND | `Builder.CreateAnd(l, r, dest)` |
| `dest = OR lhs rhs` | Logical OR | `Builder.CreateOr(l, r, dest)` |
| `dest = NOT val` | Logical NOT | `Builder.CreateNot(v, dest)` |
| `LABEL label` | Target marker | Moves insertion point: `Builder.SetInsertPoint(bb)` |
| `JUMP label` | Unconditional jump | `Builder.CreateBr(bb)` |
| `JUMP_IF_FALSE c L` | Conditional jump | `Builder.CreateCondBr(c, trueBB, falseBB)` |
| `dest = CALL fn args` | Function call | `Builder.CreateCall(callee, args, dest)` |
| `RETURN [value]` | Return instruction | `Builder.CreateRet(val)` / `Builder.CreateRetVoid()` |
| `PRINT value` | Built-in output | Call to `printf` using `@printf(ptr, ...)` |

---

## 3. Object File Generation Flow

The object file generation flow converts the in-memory LLVM module into a binary machine-code object file (`.obj` on Windows):

1.  **Target Selection**: The target triple is retrieved from the host machine using `llvm::sys::getDefaultTargetTriple()`.
2.  **Target Machine Configuration**: We lookup the registered target using `llvm::TargetRegistry::lookupTarget()` and initialize a target machine with options.
3.  **Data Layout**: The module's data layout is aligned with the target machine's layout.
4.  **Legacy Pass Manager**: We instantiate `llvm::legacy::PassManager` and construct a pass to emit an object file (`llvm::CodeGenFileType::ObjectFile`) directly to a file stream.

---

## 4. Linking Flow on Windows

Linking on Windows requires resolving runtime helper functions and system symbols. Ribosome implements a fully dynamic linker locator and invocation flow:

1.  **Directory Discovery**:
    *   MSVC VC Tools directory (`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC`) is traversed to locate the active MSVC toolset and find `link.exe`.
    *   Windows SDK directory (`C:\Program Files (x86)\Windows Kits\10\Lib`) is searched to locate the user-mode library subfolder (`um/x64`) and Universal CRT subfolder (`ucrt/x64`).
2.  **Command Building**:
    A batch script (`link.bat`) is generated to invoke the linker with necessary paths and static runtime libraries:
    *   `libcmt.lib` — Static C runtime library
    *   `libvcruntime.lib` — Visual C++ runtime library
    *   `libucrt.lib` — Universal C Runtime library
    *   `kernel32.lib` & `uuid.lib` — Core Windows OS APIs
3.  **Execution & Cleanup**: The batch script is executed via `std::system()` and immediately deleted.

---

## 5. Known Limitations (Phase 1)

*   **OOP**: Classes, fields (`NEW`, `GETFIELD`, `SETFIELD`), constructors, and methods are not supported and throw a fast-fail error.
*   **Strings & Chars**: Strings (other than the internal formatted output for `printf`) and characters are not supported.
*   **Floats & Doubles**: Rejects any float/double operations.
*   **Input**: Standard input handling (`input()`) is disabled.
