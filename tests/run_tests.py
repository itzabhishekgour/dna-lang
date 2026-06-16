import os
import sys
import subprocess
import glob
import re

# Path to the ribosome compiler executable
COMPILER_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build", "Release", "ribosome.exe"))

# Subdirectories to search for tests
TEST_DIRS = [
    "tests/lexer",
    "tests/parser",
    "tests/semantic",
    "tests/ir",
    "tests/llvm",
    "tests/runtime",
    "tests/regression"
]

def parse_metadata(file_path):
    metadata = {
        "flags": [],
        "compiler_exit_code": 0,
        "compiler_error": None,
        "compiler_stdout": [],
        "execution_stdout": []
    }
    
    with open(file_path, "r", encoding="utf-8") as f:
        lines = f.readlines()
        
    mode = None
    for line in lines:
        line_strip = line.strip()
        if not line_strip.startswith("//"):
            continue
        
        # Remove '//' and leading space
        content = line_strip[2:].strip()
        
        # Check metadata headers
        if content.startswith("FLAGS:"):
            metadata["flags"] = content[6:].strip().split()
            continue
        elif content.startswith("EXPECT_COMPILER_EXIT_CODE:"):
            try:
                metadata["compiler_exit_code"] = int(content[26:].strip())
            except ValueError:
                pass
            continue
        elif content.startswith("EXPECT_COMPILER_ERROR:"):
            metadata["compiler_error"] = ""
            mode = "compiler_error"
            continue
        elif content.startswith("EXPECT_COMPILER_STDOUT:"):
            mode = "compiler_stdout"
            continue
        elif content.startswith("EXPECT_EXECUTION_STDOUT:"):
            mode = "execution_stdout"
            continue
        
        # Append lines based on active mode
        if mode == "compiler_error":
            if metadata["compiler_error"] == "":
                metadata["compiler_error"] = content
            else:
                metadata["compiler_error"] += "\n" + content
        elif mode == "compiler_stdout":
            metadata["compiler_stdout"].append(content)
        elif mode == "execution_stdout":
            metadata["execution_stdout"].append(content)
            
    return metadata

def clean_up_build_artifacts(base_name):
    # ribosome generates baseName.obj and baseName.exe in the Cwd
    obj_file = f"{base_name}.obj"
    exe_file = f"{base_name}.exe"
    if os.path.exists(obj_file):
        try: os.remove(obj_file)
        except Exception: pass
    if os.path.exists(exe_file):
        try: os.remove(exe_file)
        except Exception: pass

def run_test(file_path):
    metadata = parse_metadata(file_path)
    flags = metadata["flags"]
    
    # Run compiler
    cmd = [COMPILER_PATH, os.path.abspath(file_path)] + flags
    
    try:
        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=5.0
        )
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT", "Compiler timed out (exceeded 5.0 seconds)."
    except Exception as e:
        return False, "ERROR", f"Failed to execute compiler: {str(e)}"
    
    # 1. Verify compiler exit code
    if proc.returncode != metadata["compiler_exit_code"]:
        return False, "EXIT_CODE_MISMATCH", f"Expected compiler exit code {metadata['compiler_exit_code']}, got {proc.returncode}.\nStdout: {proc.stdout}\nStderr: {proc.stderr}"
    
    # 2. Verify compiler error message if expected
    if metadata["compiler_error"]:
        expected_err = metadata["compiler_error"].strip().replace("\r\n", "\n")
        actual_err = proc.stderr.strip().replace("\r\n", "\n")
        if expected_err not in actual_err:
            return False, "COMPILER_ERROR_MISMATCH", f"Expected compiler error substring:\n'{expected_err}'\nbut got:\n'{actual_err}'"
            
    # 3. Verify compiler stdout if expected
    if metadata["compiler_stdout"]:
        actual_lines = [l.strip() for l in proc.stdout.splitlines() if l.strip()]
        expected_lines = [l.strip() for l in metadata["compiler_stdout"] if l.strip()]
        for el in expected_lines:
            match_found = any(el in al for al in actual_lines)
            if not match_found:
                return False, "COMPILER_STDOUT_MISMATCH", f"Expected line:\n'{el}'\nnot found in compiler stdout:\n'{proc.stdout}'"
                
    # 4. Verify native execution if --build is in flags and compiler was expected to succeed
    if "--build" in flags and metadata["compiler_exit_code"] == 0:
        base_name = os.path.splitext(os.path.basename(file_path))[0]
        exe_path = os.path.abspath(os.path.join(".", f"{base_name}.exe"))
        
        if not os.path.exists(exe_path):
            return False, "EXE_MISSING", "Executable was not generated."
            
        try:
            exe_proc = subprocess.run(
                [exe_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=5.0
            )
        except subprocess.TimeoutExpired:
            clean_up_build_artifacts(base_name)
            return False, "TIMEOUT", "Compiled executable timed out (exceeded 5.0 seconds)."
        except Exception as e:
            clean_up_build_artifacts(base_name)
            return False, "ERROR", f"Failed to execute compiled binary: {str(e)}"
            
        # Verify execution stdout
        if metadata["execution_stdout"]:
            actual_exec_lines = [l.strip() for l in exe_proc.stdout.splitlines() if l.strip()]
            expected_exec_lines = [l.strip() for l in metadata["execution_stdout"] if l.strip()]
            for el in expected_exec_lines:
                match_found = any(el in al for al in actual_exec_lines)
                if not match_found:
                    clean_up_build_artifacts(base_name)
                    return False, "EXECUTION_STDOUT_MISMATCH", f"Expected execution line:\n'{el}'\nnot found in stdout:\n'{exe_proc.stdout}'"
                    
        # Verify execution exit code (must be 0 on success)
        if exe_proc.returncode != 0:
            clean_up_build_artifacts(base_name)
            return False, "EXECUTION_FAILED", f"Compiled binary exited with code {exe_proc.returncode}. Stderr: {exe_proc.stderr}"
            
        clean_up_build_artifacts(base_name)
        
    return True, "SUCCESS", ""

def main():
    if not os.path.exists(COMPILER_PATH):
        print(f"Error: Compiler executable not found at {COMPILER_PATH}. Please compile first.")
        sys.exit(1)
        
    test_files = []
    workspace_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    
    for td in TEST_DIRS:
        full_dir = os.path.join(workspace_root, td)
        if os.path.exists(full_dir):
            test_files.extend(glob.glob(os.path.join(full_dir, "*.dna")))
            
    print(f"Found {len(test_files)} tests.")
    passed = 0
    failed = 0
    
    for tf in sorted(test_files):
        rel_path = os.path.relpath(tf, workspace_root)
        success, code, msg = run_test(tf)
        if success:
            print(f"[ PASS ] {rel_path}")
            passed += 1
        else:
            print(f"[ FAIL ] {rel_path} - Reason: {code}")
            if msg:
                print(f"         {msg}")
            failed += 1
            
    print("\n------------------------------------------------")
    print(f"Result: {passed} passed, {failed} failed out of {len(test_files)} total.")
    print("------------------------------------------------")
    
    if failed > 0:
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == "__main__":
    main()
