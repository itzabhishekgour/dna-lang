cmake_minimum_required(VERSION 3.20)

project(ribosome
    VERSION 0.1.0
    DESCRIPTION "Ribosome — Compiler for the DNA programming language"
    LANGUAGES CXX
)

# ── Compiler settings ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Find LLVM ──────────────────────────────────────────────────────────────────
set(LLVM_DIR "C:/Users/mrasg/.gemini/antigravity-ide/brain/dceb7154-d1e4-40cf-a962-1838ba38f9be/scratch/llvm-18.1.8-windows-amd64-msvc17-msvcrt/lib/cmake/llvm" CACHE PATH "LLVM config path")
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# ── Source files ───────────────────────────────────────────────────────────────
set(RIBOSOME_SOURCES
    main.cpp
    lexer/Lexer.cpp
    parser/Parser.cpp
    semantic/SemanticAnalyzer.cpp
    ir/IRGenerator.cpp
    codegen/LLVMCodeGen.cpp
)

# ── Executable ─────────────────────────────────────────────────────────────────
add_executable(ribosome ${RIBOSOME_SOURCES})

# ── Runtime Library ────────────────────────────────────────────────────────────
add_library(dnaruntime STATIC runtime/DNAString.cpp)

# Place dnaruntime.lib next to ribosome.exe so the compiler can find it at link time
set_target_properties(dnaruntime PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/Debug"
)

target_include_directories(ribosome PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LLVM_INCLUDE_DIRS}
)

# Set compile flags and definitions for LLVM
target_compile_definitions(ribosome PRIVATE ${LLVM_DEFINITIONS})

# Link LLVM libraries
llvm_map_components_to_libnames(llvm_libs support core x86codegen x86desc x86info target mc native)
target_link_libraries(ribosome PRIVATE ${llvm_libs})

# ── Compiler warnings ──────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(ribosome PRIVATE /W4) # Remove /WX (treat warnings as errors) for LLVM-dependent builds
else()
    target_compile_options(ribosome PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Install ────────────────────────────────────────────────────────────────────
install(TARGETS ribosome RUNTIME DESTINATION bin)

message(STATUS "")
message(STATUS "  ██████╗ ███╗   ██╗ █████╗ ")
message(STATUS "  ██╔══██╗████╗  ██║██╔══██╗")
message(STATUS "  ██║  ██║██╔██╗ ██║███████║")
message(STATUS "  ██║  ██║██║╚██╗██║██╔══██║")
message(STATUS "  ██████╔╝██║ ╚████║██║  ██║")
message(STATUS "  ╚═════╝ ╚═╝  ╚═══╝╚═╝  ╚═╝  v${PROJECT_VERSION}")
message(STATUS "")
message(STATUS "  Compiler: Ribosome")
message(STATUS "  Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build:    ${CMAKE_BUILD_TYPE}")
message(STATUS "")
