# DNA IR v0.1 — Intermediate Representation Specification

> **Status:** Active  
> **Version:** 0.1.0  
> **Compiler stage:** After Semantic Analysis → Before LLVM IR

---

## Overview

DNA IR is a typed, human-readable three-address-code (3AC) intermediate representation
that sits between the Semantic Analyzer and the future LLVM backend.

Design goals:
- Human readable (can be inspected with any text editor)
- Independent of LLVM — no LLVM headers required
- Straightforward mapping to LLVM IR in Phase 5
- Extensible for future optimization passes

### Emit IR

```bash
ribosome source.dna --ir
```

---

## Program Structure

An IR file consists of:

1. **Class definitions** (`CLASS … END_CLASS`) — metadata only, no code
2. **Function definitions** (`FUNC … END_FUNC`) — three-address-code body

```
CLASS Student

  FIELD String  name
  FIELD int     age

END_CLASS

FUNC Student::construct

  PARAM n
  PARAM a

  t0 = LOAD n
  STORE name t0
  ...
  RETURN

END_FUNC
```

---

## Instruction Set Reference

### Notation

```
dest = OPCODE  operand0  operand1  ...     (instruction with output)
OPCODE  operand0  operand1  ...            (instruction without output)
```

- `dest` — a temporary (`t0`, `t1`, …) or variable name
- Temporaries are fresh per function: `t0, t1, t2, …`
- Labels are globally unique: `prefix_N`

---

### Variable Operations

| Instruction | Effect |
|---|---|
| `ALLOC var` | Declare storage for named variable `var` |
| `STORE var value` | Write `value` into `var` |
| `t = LOAD var` | Read `var` into temp `t` |
| `t = MOV value` | Copy a temp or literal into a new temp |

**Example — `int age = 20`:**
```
ALLOC age
STORE age 20
```

**Example — read back:**
```
t0 = LOAD age
```

---

### Arithmetic

| Instruction | DNA operator |
|---|---|
| `t = ADD lhs rhs` | `+` |
| `t = SUB lhs rhs` | `-` |
| `t = MUL lhs rhs` | `*` |
| `t = DIV lhs rhs` | `/` |
| `t = MOD lhs rhs` | `%` |
| `t = NEG val` | unary `-` |

**Example — `a + b`:**
```
t0 = LOAD a
t1 = LOAD b
t2 = ADD t0 t1
```

---

### Comparison

| Instruction | DNA operator |
|---|---|
| `t = CMP_EQ lhs rhs` | `==` |
| `t = CMP_NE lhs rhs` | `!=` |
| `t = CMP_GT lhs rhs` | `>` |
| `t = CMP_LT lhs rhs` | `<` |
| `t = CMP_GE lhs rhs` | `>=` |
| `t = CMP_LE lhs rhs` | `<=` |

**Example — `age >= 18`:**
```
t0 = LOAD age
t1 = CMP_GE t0 18
```

---

### Logical

| Instruction | DNA operator |
|---|---|
| `t = AND lhs rhs` | `&&` |
| `t = OR  lhs rhs` | `\|\|` |
| `t = NOT val` | `!` |

---

### Control Flow

| Instruction | Effect |
|---|---|
| `LABEL name:` | Branch target (no-op at runtime) |
| `JUMP label` | Unconditional jump |
| `JUMP_IF_TRUE  cond label` | Jump if `cond` is truthy |
| `JUMP_IF_FALSE cond label` | Jump if `cond` is falsy |

#### if / else lowering

```
t0 = LOAD age
t1 = CMP_GE t0 18
JUMP_IF_FALSE t1 if_else_0
PRINT "Adult"
JUMP if_end_1

if_else_0:
PRINT "Minor"

if_end_1:
```

#### while lowering

```
while_start_0:
  t0 = LOAD counter
  t1 = CMP_LT t0 5
  JUMP_IF_FALSE t1 while_end_1
  [body]
  JUMP while_start_0

while_end_1:
```

#### for lowering

```
ALLOC i
STORE i 0

for_start_0:
  t0 = LOAD i
  t1 = CMP_LT t0 10
  JUMP_IF_FALSE t1 for_end_1
  [body]
  t2 = LOAD i        ; i++ update
  t3 = ADD t2 1
  STORE i t3
  JUMP for_start_0

for_end_1:
```

---

### Functions

| Instruction | Effect |
|---|---|
| `FUNC name` | Start function definition |
| `PARAM name` | Declare incoming parameter |
| `t = CALL func arg0 arg1 …` | Call function, store result |
| `CALL func arg0 arg1 …` | Call function, discard result (void) |
| `RETURN value` | Return with value |
| `RETURN` | Void return |
| `END_FUNC` | End function definition |

**Example — `action int add(int a, int b)`:**
```
FUNC add

  PARAM a
  PARAM b

  t0 = LOAD a
  t1 = LOAD b
  t2 = ADD t0 t1
  RETURN t2

END_FUNC
```

**Example — calling `add(10, 20)`:**
```
t0 = CALL add 10 20
STORE result t0
```

---

### Built-in Functions

| Instruction | DNA source |
|---|---|
| `PRINT value` | `print(expr)` |
| `t = INPUT prompt` | `input("...")` |
| `t = INPUT` | `input()` (no prompt) |

**Example — `print(age)`:**
```
t0 = LOAD age
PRINT t0
```

**Example — `String name = input("Enter: ")`:**
```
ALLOC name
t0 = INPUT "Enter: "
STORE name t0
```

---

### OOP

| Instruction | Effect |
|---|---|
| `t = NEW ClassName arg0 arg1 …` | Create an object |
| `t = GETFIELD obj field` | Read a field |
| `SETFIELD obj field value` | Write a field |
| `t = CALL .method receiver arg0 …` | Method call |

**Example — `Student s = Student("Alice", 21)`:**
```
ALLOC s
t0 = NEW Student "Alice" 21
STORE s t0
```

**Example — `s.greet()`:**
```
t0 = LOAD s
t1 = CALL .greet t0
```

**Example — `s.name`:**
```
t0 = LOAD s
t1 = GETFIELD t0 name
```

---

### Class Definitions

Class definitions appear before functions, as metadata only.

```
CLASS Student

  FIELD String  name
  FIELD int     age

END_CLASS
```

Methods are emitted as qualified functions: `ClassName::methodName`.  
Constructors are: `ClassName::construct`.

---

### Postfix Increment/Decrement

`i++` and `i--` are lowered to explicit load + arithmetic + store:

```
t0 = LOAD i      ; save old value (postfix semantics)
t1 = ADD t0 1    ; or SUB for --
STORE i t1
```

The old value `t0` is the result if the expression value is used.

---

## Label Naming Convention

Labels follow the pattern `purpose_N` where `N` is a globally unique integer:

| Prefix | Usage |
|---|---|
| `if_else_N` | else branch target |
| `if_end_N` | end of if statement |
| `while_start_N` | loop condition re-check |
| `while_end_N` | loop exit |
| `for_start_N` | loop condition re-check |
| `for_end_N` | loop exit |

---

## Temporary Naming Convention

Temporaries are named `t0`, `t1`, `t2`, … and reset at the beginning of each
function. They are single-assignment (SSA-like) in v0.1.

---

## Complete Example

DNA source:
```dna
action void main()
{
    int age = 20
    print(age)
}
```

DNA IR output:
```
FUNC main

  ALLOC age
  STORE age 20
  t0 = LOAD age
  PRINT t0
  RETURN

END_FUNC
```

---

DNA source:
```dna
action int add(int a, int b)
{
    return a + b
}
```

DNA IR output:
```
FUNC add

  PARAM a
  PARAM b

  t0 = LOAD a
  t1 = LOAD b
  t2 = ADD t0 t1
  RETURN t2

END_FUNC
```

---

## Future IR Extensions (v0.2+)

| Feature | Planned instruction |
|---|---|
| Array access | `GETINDEX arr idx` / `SETINDEX arr idx val` |
| Null check | `CHECK_NULL val` |
| Type cast | `CAST type val` |
| Exception | `THROW val` / `TRY label` / `CATCH label` |
| Closures | `CLOSURE funcname` |
| Optimization metadata | `; [phi] [loop_invariant]` |
