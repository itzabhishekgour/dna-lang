# DNA Language Specification (v1.0-alpha)

This document contains the official language specification for DNA. It describes the syntax, grammar, types, execution model, and current implementation status of all language constructs.

---

## 1. Keywords

The following reserved words have special meaning in DNA and cannot be used as identifiers:

| Keyword | Description | Implementation Status |
| :--- | :--- | :--- |
| `action` | Declares a function block. | **Implemented** |
| `int` | Represents 32-bit signed integer primitive. | **Implemented** |
| `bool` | Represents boolean primitive (`true`/`false`). | **Implemented** |
| `void` | Represents empty return type. | **Implemented** |
| `String` | Represents a UTF-8 character string structure. | **Implemented** |
| `if` | Conditional branch execution. | **Implemented** |
| `else` | Alternative conditional branch execution. | **Implemented** |
| `while` | Basic loop structure. | **Implemented** |
| `for` | Counter-based loop structure. | **Implemented** |
| `break` | Terminate loop execution immediately. | **Implemented** |
| `continue` | Skip to the next iteration of the active loop. | **Implemented** |
| `load` | Import external file or module dependencies. | **Partial** (Parsed & checked, no filesystem resolver) |
| `class` | Declares class types. | **Partial** (Parsed & checked, no codegen) |
| `new` | Instantiate class instances. | **Partial** (Parsed & checked, no memory allocator) |
| `return` | Yield value from active function block. | **Implemented** |
| `print` | Standard console output built-in. | **Implemented** |
| `input` | Standard console input built-in. | **Planned** (Reserved keyword) |

---

## 2. Primitive Types

DNA supports the following primitive types:

### `int`
- **Description:** 32-bit signed two's complement integer.
- **LLVM Type:** `i32`
- **Status:** **Implemented**

### `bool`
- **Description:** 1-bit boolean state representing `true` or `false`.
- **LLVM Type:** `i1`
- **Status:** **Implemented**

### `void`
- **Description:** Empty type denoting the absence of a value. Used exclusively for functions that return no value.
- **LLVM Type:** `void`
- **Status:** **Implemented**

### `float` / `double` / `char`
- **Description:** Future floating point and character primitives.
- **Status:** **Planned** (Currently parsed but rejected during backend lowering)

---

## 3. Variables

Variables in DNA store data values. They must be declared with an explicit type.

- **Stack Local Variables:** Fully supported. Memory is allocated on the function stack frame.
- **Global Variables:** Parsed and semantically verified for duplicate declarations, but **unsupported** by the LLVM backend.
- **Status:** **Implemented** (Locals) / **Planned** (Globals backend support)

```dna
// Valid local variable declarations
int count = 10
bool isActive = true
String message = "Hello, DNA!"
```

---

## 4. Functions (`action`)

Functions in DNA are declared using the `action` keyword. They must define parameter types and an explicit return type.

- **Status:** **Implemented**
- **Constraints:** Return types and arguments must be primitives or Strings. Overloading and varargs are currently unsupported.

```dna
// Basic action declaration
action int add(int a, int b) {
    return a + b
}

// Action returning void
action void greet() {
    print("Welcome to DNA!")
}
```

---

## 5. Classes & Objects

Classes define structure and behaviour templates for objects.

- **Status:** **Partial**
- **Details:** The compiler parses and semantically validates fields and method definitions, preventing duplicates. However, the LLVM backend does not generate memory layout structures or allocate object properties in this release.

```dna
class Point {
    int x
    int y
}
```

---

## 6. Constructors

Constructors initialize fields during object creation.

- **Status:** **Partial**
- **Details:** Parsed and structurally verified. Code generation for memory initialization is not implemented.

```dna
class Point {
    int x
    int y
    
    // Constructor definition syntax
    action Point(int startX, int startY) {
        x = startX
        y = startY
    }
}
```

---

## 7. Methods

Methods are actions defined within a class block.

- **Status:** **Partial**
- **Details:** Parsed and verified for identifier validity. Lowering method scopes and implicit `this` pointer parameters to LLVM is unimplemented.

---

## 8. Control Flow

### `if`/`else`
- **Status:** **Implemented**
- **Constraints:** Conditional expression must resolve to `bool`. Native `else if` is unsupported; nested `else { if ... }` blocks must be used instead.

```dna
if (score > 90) {
    print("Grade A")
} else {
    if (score > 80) {
        print("Grade B")
    } else {
        print("Grade C")
    }
}
```

---

## 9. Loops

DNA supports `while` and `for` loops along with `break` and `continue` keywords.

- **Status:** **Implemented**

```dna
// While loop
int i = 0
while (i < 5) {
    if (i == 2) {
        i++
        continue
    }
    print(i)
    i++
}

// For loop
for (int j = 0; j < 10; j++) {
    if (j == 5) {
        break
    }
    print(j)
}
```

---

## 10. Operators

DNA supports the following operator sets:

| Operator Category | Operators | Status |
| :--- | :--- | :--- |
| **Arithmetic** | `+`, `-`, `*`, `/`, `%` | **Implemented** |
| **Relational** | `==`, `!=`, `<`, `>`, `<=`, `>=` | **Implemented** |
| **Logical** | `&&`, `||`, `!` | **Implemented** (Non-short-circuiting) |
| **Unary** | `-` (numeric negation), `!` | **Implemented** |
| **Postfix** | `++`, `--` | **Implemented** |
| **Assignment** | `=` | **Implemented** |

---

## 11. Strings

String variables store sequences of text characters.

- **Memory Layout:** Lowered to a structural representation:
  ```cpp
  struct DNAString {
      char* data;
      int32_t length;
      int32_t capacity;
  };
  ```
- **Windows x64 ABI:** Strings are passed and returned by reference using temporal stack slots.
- **Status:** **Implemented** (Literals, variables, declarations, and console printing via `print`). String concatenation (`dna_string_concat`), cloning (`dna_string_copy`), and destruction (`dna_string_free`) are defined as runtime APIs but exist only as stub implementations.

```dna
String banner = "=== DNA Version 0.2.0 ==="
print(banner)
```

---

## 12. Imports (`load`)

Allows importing external modules or files into the compilation unit.

- **Status:** **Partial**
- **Details:** The syntax `load <module_name>` is successfully parsed, and the semantic analyzer tracks imports to prevent duplicate loads of the same module name. However, filesystem resolution, cross-file symbol resolution, and library linking of multi-file packages are not yet implemented.

```dna
load Math
load Console
```

---

## 13. Syntax & Grammar Examples

Below is a complete, grammatically valid DNA program showing all currently supported features:

```dna
// Module loading
load StandardIO

// Function declaration with calculations
action int computeFactorial(int n) {
    int result = 1
    int i = 1
    while (i <= n) {
        result = result * i
        i++
    }
    return result
}

// Main execution entrypoint
action void main() {
    String message = "Factorial computed successfully:"
    int val = 5
    int fact = computeFactorial(val)
    
    print(message)
    print(fact)
}
```

---

*DNA Programming Language is created and developed by Abhishek Gour.*
