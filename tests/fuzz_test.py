import os
import sys
import random
import string
import subprocess
import argparse
import time

COMPILER_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build", "Release", "ribosome.exe"))

# List of DNA tokens/keywords for grammar-aware fuzzer
TOKENS = [
    "action", "class", "construct", "void", "int", "bool", "String", "float", "double", "char",
    "main", "print", "input", "return", "if", "else", "while", "for", "break", "continue",
    "true", "false", "load", "public", "private",
    "{", "}", "(", ")", "[", "]", ";", ",", ".", "=", "+", "-", "*", "/", "%", "++", "--",
    "==", "!=", ">", "<", ">=", "<=", "&&", "||", "!",
    "x", "y", "z", "a", "b", "c", "i", "j", "k", "Student", "Point", "foo", "bar",
    "10", "20", "0", "3.14", "\"hello\"", "'c'", "true", "false"
]

TEMPLATES = [
    "action void main() { int x = 10 }",
    "action void main() { if (true) { print(1) } }",
    "action void main() { while (true) { print(1) } }",
    "class Student { public int age } action void main() { Student s = Student() }",
    "action int add(int a, int b) { return a + b } action void main() { print(add(1, 2)) }",
    "load io action void main() { print(10) }",
    "action void main() { for (int i = 0; i < 10; i++) { print(i) } }"
]

def generate_random_garbage(length=None):
    if length is None:
        length = random.randint(5, 500)
    # Printable characters + some non-printable/whitespace
    chars = string.printable + "\x00\x01\n\r\t"
    return "".join(random.choice(chars) for _ in range(length))

def generate_grammar_aware_fuzz():
    if random.random() < 0.3:
        # Template-based mutation
        template = random.choice(TEMPLATES)
        mutated = list(template)
        # Apply 1 to 5 mutations
        num_mutations = random.randint(1, 5)
        for _ in range(num_mutations):
            m_type = random.choice(["delete", "insert_char", "insert_token", "truncate"])
            if m_type == "delete" and len(mutated) > 1:
                idx = random.randint(0, len(mutated) - 1)
                del mutated[idx]
            elif m_type == "insert_char":
                idx = random.randint(0, len(mutated))
                mutated.insert(idx, random.choice(string.printable))
            elif m_type == "insert_token":
                idx = random.randint(0, len(mutated))
                tok = " " + random.choice(TOKENS) + " "
                mutated.insert(idx, tok)
            elif m_type == "truncate":
                idx = random.randint(1, len(mutated))
                mutated = mutated[:idx]
        return "".join(mutated)
    else:
        # Token-soup based fuzzing
        num_tokens = random.randint(5, 100)
        chosen = []
        for _ in range(num_tokens):
            if random.random() < 0.1:
                # Add some random garbage between tokens
                chosen.append(generate_random_garbage(random.randint(1, 5)))
            else:
                chosen.append(random.choice(TOKENS))
        # Decide how to join them (spaces, newlines, or nothing)
        output = []
        for tok in chosen:
            sep = random.choice([" ", "\n", "", " ; "])
            output.append(tok + sep)
        return "".join(output)

def is_crash_exit_code(code):
    # Mask exit code to 32-bit unsigned for Windows NTSTATUS evaluation
    unsigned_code = code & 0xFFFFFFFF
    
    # 0xC0000005: Access Violation
    # 0xC000001D: Illegal Instruction
    # 0xC00000FD: Stack Overflow
    # 0xC0000409: Security Check Failure / Abort
    # 3: abort() exit code under MSVC
    # In general, if code is in the 0xC0000000 range, it is an OS exception/crash.
    if unsigned_code in [0xC0000005, 0xC000001D, 0xC00000FD, 0xC0000409]:
        return True, f"Fatal Exception (0x{unsigned_code:08X})"
        
    if code == 3:
        return True, "MSVC abort() called"
        
    # Any OS exceptions starting with 0xC...
    if 0xC0000000 <= unsigned_code <= 0xDFFFFFFF:
        return True, f"Fatal NTSTATUS Exception (0x{unsigned_code:08X})"
        
    return False, None

def run_fuzz_case(case_data, case_index, temp_file_path):
    with open(temp_file_path, "w", encoding="utf-8") as f:
        f.write(case_data)
        
    # Enforce 5.0s timeout per compilation
    try:
        proc = subprocess.run(
            [COMPILER_PATH, temp_file_path, "--check"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=5.0
        )
        code = proc.returncode
        is_crash, reason = is_crash_exit_code(code)
        if is_crash:
            return "CRASH", f"Exit code {code} ({reason}). Stderr: {proc.stderr}"
        return "OK", f"Exit code {code}"
    except subprocess.TimeoutExpired:
        return "TIMEOUT", "Compiler timed out (exceeded 5.0 seconds)."
    except Exception as e:
        return "ERROR", f"Failed to execute compiler: {str(e)}"

def main():
    parser = argparse.ArgumentParser(description="Ribosome Compiler Fuzzer")
    parser.add_argument("--quick", action="store_true", help="Run 1,000 cases (default)")
    parser.add_argument("--stress", action="store_true", help="Run 10,000 cases")
    args = parser.parse_args()
    
    iterations = 10000 if args.stress else 1000
    mode_str = "Stress" if args.stress else "Quick"
    
    print(f"Starting Fuzz Testing in {mode_str} Mode ({iterations} iterations)...")
    
    if not os.path.exists(COMPILER_PATH):
        print(f"Error: Compiler executable not found at {COMPILER_PATH}. Please compile first.")
        sys.exit(1)
        
    temp_file = os.path.abspath(os.path.join(os.path.dirname(__file__), "fuzz_temp.dna"))
    
    crashes = 0
    timeouts = 0
    errors = 0
    ok_count = 0
    
    start_time = time.time()
    max_duration = 0.0
    
    # We will use seed for reproducibility
    random.seed(42)
    
    for i in range(1, iterations + 1):
        # 30% pure garbage, 70% grammar-aware
        if random.random() < 0.3:
            case_data = generate_random_garbage()
        else:
            case_data = generate_grammar_aware_fuzz()
            
        case_start = time.time()
        status, detail = run_fuzz_case(case_data, i, temp_file)
        case_duration = time.time() - case_start
        max_duration = max(max_duration, case_duration)
        
        if status == "CRASH":
            crashes += 1
            print(f"\n[ CRASH ] Case {i} failed!")
            print(f"Input data:\n{case_data}\n")
            print(f"Details: {detail}\n")
        elif status == "TIMEOUT":
            timeouts += 1
            print(f"\n[ TIMEOUT ] Case {i} timed out after 5s!")
            print(f"Input data:\n{case_data}\n")
        elif status == "ERROR":
            errors += 1
            print(f"\n[ ERROR ] Case {i} executor error: {detail}")
        else:
            ok_count += 1
            
        if i % 100 == 0:
            print(f"Progress: {i}/{iterations} cases completed...")
            
    # Cleanup
    if os.path.exists(temp_file):
        try: os.remove(temp_file)
        except Exception: pass
        
    total_time = time.time() - start_time
    print("\n------------------------------------------------")
    print(f"Fuzzing Summary ({mode_str} Mode)")
    print(f"Total Cases Run:    {iterations}")
    print(f"Controlled Exits:   {ok_count}")
    print(f"Crashes Detected:   {crashes}")
    print(f"Timeouts (Hangs):   {timeouts}")
    print(f"Executor Errors:    {errors}")
    print(f"Total Time Taken:   {total_time:.2f} seconds")
    print(f"Max Case Duration:  {max_duration:.4f} seconds")
    print("------------------------------------------------")
    
    if crashes > 0 or timeouts > 0:
        print("Fuzzing failed: crashes or hangs detected.")
        sys.exit(1)
    else:
        print("Fuzzing succeeded: Zero compiler crashes or hangs detected.")
        sys.exit(0)

if __name__ == "__main__":
    main()
