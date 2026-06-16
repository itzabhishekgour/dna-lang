#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "semantic/SemanticAnalyzer.h"
#include "ir/IRGenerator.h"
#include "ir/IRPrinter.h"
#include "codegen/LLVMCodeGen.h"

#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

// =============================================================================
//  DNA v0.1 — Ribosome Compiler Entry Point
//  File: main.cpp
//
//  Usage:
//    ribosome <file.dna>           Print token stream (Phase 1 default)
//    ribosome <file.dna> --tokens  Same as default
//    ribosome <file.dna> --ast     Print AST tree
//    ribosome <file.dna> --check   Run semantic analysis only
//    ribosome <file.dna> --ir      Emit DNA IR
//    ribosome <file.dna> --all     Tokens + AST + semantic check + IR
// =============================================================================

// =============================================================================
// ASTPrinter — visitor that pretty-prints the AST with indentation
// =============================================================================
class ASTPrinter : public DNA::ASTVisitor {
public:
    explicit ASTPrinter(std::ostream& out = std::cout) : out_(out), indent_(0) {}

private:
    std::ostream& out_;
    int           indent_;

    void pad() { for (int i = 0; i < indent_; ++i) out_ << "  "; }

    void visitProgram(DNA::ProgramNode& node) override {
        pad(); out_ << "Program\n";
        ++indent_;
        for (auto& d : node.declarations) if (d) d->accept(*this);
        --indent_;
    }

    void visitLoad(DNA::LoadNode& node) override {
        pad(); out_ << "Load(" << node.moduleName << ")\n";
    }

    void visitClass(DNA::ClassNode& node) override {
        pad(); out_ << "Class(" << node.name << ")\n";
        ++indent_;
        for (auto& m : node.members) if (m) m->accept(*this);
        --indent_;
    }

    void visitFieldDecl(DNA::FieldDeclNode& node) override {
        pad(); out_ << "Field(" << node.type.toString() << " " << node.name;
        if (node.initializer) { out_ << " = "; node.initializer->accept(*this); }
        out_ << ")\n";
    }

    void visitAction(DNA::ActionNode& node) override {
        pad(); out_ << "Action(" << node.returnType.toString() << " " << node.name << "(";
        for (std::size_t i = 0; i < node.params.size(); ++i) {
            if (i) out_ << ", ";
            out_ << node.params[i].type.toString() << " " << node.params[i].name;
        }
        out_ << "))\n";
        ++indent_;
        if (node.body) node.body->accept(*this);
        --indent_;
    }

    void visitConstruct(DNA::ConstructNode& node) override {
        pad(); out_ << "Construct(";
        for (std::size_t i = 0; i < node.params.size(); ++i) {
            if (i) out_ << ", ";
            out_ << node.params[i].type.toString() << " " << node.params[i].name;
        }
        out_ << ")\n";
        ++indent_;
        if (node.body) node.body->accept(*this);
        --indent_;
    }

    void visitBlock(DNA::BlockNode& node) override {
        pad(); out_ << "Block\n";
        ++indent_;
        for (auto& s : node.statements) if (s) s->accept(*this);
        --indent_;
    }

    void visitVarDecl(DNA::VarDeclNode& node) override {
        pad(); out_ << "VarDecl(" << node.type.toString() << " " << node.name;
        if (node.initializer) {
            out_ << " = ";
            ++indent_; node.initializer->accept(*this); --indent_;
        }
        out_ << ")\n";
    }

    void visitAssignment(DNA::AssignmentNode& node) override {
        pad(); out_ << "Assign\n";
        ++indent_;
        if (node.target) node.target->accept(*this);
        if (node.value)  node.value->accept(*this);
        --indent_;
    }

    void visitIf(DNA::IfNode& node) override {
        pad(); out_ << "If\n";
        ++indent_;
        pad(); out_ << "Condition\n";
        ++indent_; if (node.condition) node.condition->accept(*this); --indent_;
        pad(); out_ << "Then\n";
        ++indent_; if (node.thenBlock) node.thenBlock->accept(*this); --indent_;
        if (node.elseBlock) {
            pad(); out_ << "Else\n";
            ++indent_; node.elseBlock->accept(*this); --indent_;
        }
        --indent_;
    }

    void visitWhile(DNA::WhileNode& node) override {
        pad(); out_ << "While\n";
        ++indent_;
        pad(); out_ << "Condition\n";
        ++indent_; if (node.condition) node.condition->accept(*this); --indent_;
        if (node.body) node.body->accept(*this);
        --indent_;
    }

    void visitFor(DNA::ForNode& node) override {
        pad(); out_ << "For\n";
        ++indent_;
        pad(); out_ << "Init\n";
        ++indent_; if (node.init) node.init->accept(*this); --indent_;
        pad(); out_ << "Condition\n";
        ++indent_; if (node.condition) node.condition->accept(*this); --indent_;
        pad(); out_ << "Update\n";
        ++indent_; if (node.update) node.update->accept(*this); --indent_;
        if (node.body) node.body->accept(*this);
        --indent_;
    }

    void visitReturn(DNA::ReturnNode& node) override {
        pad(); out_ << "Return\n";
        if (node.value) { ++indent_; node.value->accept(*this); --indent_; }
    }

    void visitBreak(DNA::BreakNode&) override {
        pad(); out_ << "Break\n";
    }

    void visitContinue(DNA::ContinueNode&) override {
        pad(); out_ << "Continue\n";
    }

    void visitExprStmt(DNA::ExprStmtNode& node) override {
        pad(); out_ << "ExprStmt\n";
        ++indent_;
        if (node.expr) node.expr->accept(*this);
        --indent_;
    }

    void visitBinaryExpr(DNA::BinaryExprNode& node) override {
        pad(); out_ << "BinaryExpr(" << node.op << ")\n";
        ++indent_;
        if (node.left)  node.left->accept(*this);
        if (node.right) node.right->accept(*this);
        --indent_;
    }

    void visitUnaryExpr(DNA::UnaryExprNode& node) override {
        pad(); out_ << "UnaryExpr(" << node.op << ")\n";
        ++indent_;
        if (node.operand) node.operand->accept(*this);
        --indent_;
    }

    void visitPostfixExpr(DNA::PostfixExprNode& node) override {
        pad(); out_ << "PostfixExpr(" << node.op << ")\n";
        ++indent_;
        if (node.operand) node.operand->accept(*this);
        --indent_;
    }

    void visitCallExpr(DNA::CallExprNode& node) override {
        pad(); out_ << "Call(" << node.callee << ")\n";
        ++indent_;
        for (auto& a : node.args) if (a) a->accept(*this);
        --indent_;
    }

    void visitObjectCreation(DNA::ObjectCreationNode& node) override {
        pad(); out_ << "New(" << node.className << ")\n";
        ++indent_;
        for (auto& a : node.args) if (a) a->accept(*this);
        --indent_;
    }

    void visitMemberAccess(DNA::MemberAccessNode& node) override {
        pad(); out_ << "MemberAccess(." << node.member << ")\n";
        ++indent_;
        if (node.object) node.object->accept(*this);
        --indent_;
    }

    void visitLiteral(DNA::LiteralNode& node) override {
        pad(); out_ << "Literal(" << node.value << ")\n";
    }

    void visitIdentifier(DNA::IdentifierNode& node) override {
        pad(); out_ << "Identifier(" << node.name << ")\n";
    }
};

// =============================================================================
// Helpers
// =============================================================================
static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static bool hasFlag(int argc, char* argv[], const std::string& flag) {
    for (int i = 2; i < argc; ++i) {
        if (argv[i] == flag) return true;
    }
    return false;
}

static void printUsage(const char* prog) {
    std::cerr
        << "\n"
        << "  Ribosome — DNA v0.1 Compiler\n"
        << "\n"
        << "  Usage:\n"
        << "    " << prog << " <file.dna>           Print token stream\n"
        << "    " << prog << " <file.dna> --tokens  Print token stream (explicit)\n"
        << "    " << prog << " <file.dna> --ast     Print AST\n"
        << "    " << prog << " <file.dna> --check   Run semantic analysis\n"
        << "    " << prog << " <file.dna> --ir      Emit DNA IR\n"
        << "    " << prog << " <file.dna> --llvm    Emit LLVM IR\n"
        << "    " << prog << " <file.dna> --build   Compile to native executable\n"
        << "    " << prog << " <file.dna> --all     Tokens + AST + semantic check + IR + LLVM IR\n"
        << "\n";
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string filePath = argv[1];

    // Validate .dna extension
    if (filePath.size() < 4 ||
        filePath.substr(filePath.size() - 4) != ".dna") {
        std::cerr << "Warning: file '" << filePath
                  << "' does not have a .dna extension.\n";
    }

    const std::string source = readFile(filePath);

    // Determine mode
    bool doAll    = hasFlag(argc, argv, "--all");
    bool doTokens = hasFlag(argc, argv, "--tokens") || doAll;
    bool doAst    = hasFlag(argc, argv, "--ast")    || doAll;
    bool doCheck  = hasFlag(argc, argv, "--check")  || doAll;
    bool doIR     = hasFlag(argc, argv, "--ir")     || doAll;
    bool doLLVM   = hasFlag(argc, argv, "--llvm")   || doAll;
    bool doBuild  = hasFlag(argc, argv, "--build")  || doAll;

    // Default: print tokens
    if (!doTokens && !doAst && !doCheck && !doIR && !doLLVM && !doBuild) doTokens = true;

    // ── Phase 1: Lex ──────────────────────────────────────────────────────────
    DNA::Lexer lexer(source);
    std::vector<DNA::Token> tokens = lexer.tokenize();

    if (lexer.hasErrors()) {
        for (const auto& err : lexer.errors()) std::cerr << err << "\n";
        if (!doAst && !doCheck && !doIR && !doLLVM && !doBuild) return 1;
    }

    // ── Print token stream ─────────────────────────────────────────────────────
    if (doTokens) {
        for (const auto& tok : tokens) {
            std::cout << DNA::tokenToDisplayString(tok) << "\n";
        }
        if (doAst || doCheck || doIR || doLLVM || doBuild) std::cout << "\n";
    }

    // ── Phases 2–5 all require a parsed AST ──────────────────────────────────
    if (!doAst && !doCheck && !doIR && !doLLVM && !doBuild) return 0;

    DNA::Parser parser(tokens);
    auto program = parser.parse();

    if (parser.hasErrors()) {
        for (const auto& err : parser.errors()) std::cerr << err << "\n";
        if (!doIR && !doCheck && !doLLVM && !doBuild) return 1;
    }

    // ── Phase 2: Print AST ────────────────────────────────────────────────────
    if (doAst && program) {
        std::cout << "─── AST ───────────────────────────────────────────\n";
        ASTPrinter astPrinter(std::cout);
        program->accept(astPrinter);
        std::cout << "───────────────────────────────────────────────────\n";
        if (doCheck || doIR || doLLVM || doBuild) std::cout << "\n";
    }

    // ── Phase 3: Semantic analysis ────────────────────────────────────────────
    if ((doCheck || doIR || doLLVM || doBuild) && program) {
        DNA::SemanticAnalyzer analyzer;
        analyzer.analyze(*program);

        if (analyzer.hasErrors()) {
            for (const auto& err : analyzer.errors()) std::cerr << err << "\n";
            return 1;
        } else if (doCheck) {
            std::cout << "Semantic analysis: OK — no errors found.\n";
        }
        if (doIR || doLLVM || doBuild) std::cout << "\n";
    }

    // ── Phase 4: DNA IR generation ────────────────────────────────────────────
    if (doIR && program) {
        DNA::IRGenerator irGen;
        DNA::IRProgram   irProg = irGen.generate(*program);
        irProg.sourceFile = filePath;

        std::cout << "─── DNA IR ────────────────────────────────────────\n\n";
        DNA::IRPrinter irPrinter(std::cout);
        irPrinter.print(irProg);
        std::cout << "───────────────────────────────────────────────────\n";
        if (doLLVM || doBuild) std::cout << "\n";
    }

    // ── Phase 5: LLVM IR & Native Build ───────────────────────────────────────
    if ((doLLVM || doBuild) && program) {
        DNA::IRGenerator irGen;
        DNA::IRProgram   irProg = irGen.generate(*program);
        irProg.sourceFile = filePath;

        try {
            DNA::LLVMCodeGen llvmCodeGen(filePath);
            llvmCodeGen.generate(irProg);

            if (doLLVM) {
                if (doBuild || doIR) {
                    std::cout << "─── LLVM IR ───────────────────────────────────────\n\n";
                }
                llvmCodeGen.printIR(std::cout);
                if (doBuild || doIR) {
                    std::cout << "───────────────────────────────────────────────────\n";
                }
            }

            if (doBuild) {
                std::filesystem::path p(filePath);
                std::string baseName = p.stem().string();
                std::string objFile = baseName + ".obj";
                std::string exeFile = baseName + ".exe";

                std::cout << "Emitting object file: " << objFile << "\n";
                if (!llvmCodeGen.compile(objFile)) {
                    std::cerr << "Error: LLVM compilation failed.\n";
                    return 1;
                }

                std::cout << "Linking executable: " << exeFile << "\n";

                // Locate MSVC and Windows SDK folders dynamically
                namespace fs = std::filesystem;
                std::string msvcLibPath = "";
                std::string winSdkUmPath = "";
                std::string winSdkUcrtPath = "";
                std::string linkerPath = "";

                // Search roots for Visual Studio installations
                std::string vsRoots[] = {
                    "C:\\Program Files\\Microsoft Visual Studio",
                    "C:\\Program Files (x86)\\Microsoft Visual Studio"
                };

                for (const auto& vsRoot : vsRoots) {
                    if (!fs::exists(vsRoot)) continue;
                    for (const auto& versionEntry : fs::directory_iterator(vsRoot)) {
                        if (!versionEntry.is_directory()) continue;
                        for (const auto& editionEntry : fs::directory_iterator(versionEntry.path())) {
                            if (!editionEntry.is_directory()) continue;
                            std::filesystem::path msvcRoot = editionEntry.path() / "VC" / "Tools" / "MSVC";
                            if (fs::exists(msvcRoot)) {
                                for (const auto& entry : fs::directory_iterator(msvcRoot)) {
                                    if (entry.is_directory()) {
                                        std::string linkExe = (entry.path() / "bin" / "Hostx64" / "x64" / "link.exe").string();
                                        if (fs::exists(linkExe)) {
                                            linkerPath = linkExe;
                                            msvcLibPath = (entry.path() / "lib" / "x64").string();
                                            break;
                                        }
                                    }
                                }
                            }
                            if (!linkerPath.empty()) break;
                        }
                        if (!linkerPath.empty()) break;
                    }
                    if (!linkerPath.empty()) break;
                }

                // Search roots for Windows Kits
                std::string winKitsRoots[] = {
                    "C:\\Program Files (x86)\\Windows Kits\\10\\Lib",
                    "C:\\Program Files\\Windows Kits\\10\\Lib"
                };

                for (const auto& winKitsRoot : winKitsRoots) {
                    if (!fs::exists(winKitsRoot)) continue;
                    for (const auto& entry : fs::directory_iterator(winKitsRoot)) {
                        if (entry.is_directory()) {
                            std::string version = entry.path().filename().string();
                            if (version.rfind("10.", 0) == 0) {
                                winSdkUmPath = (entry.path() / "um" / "x64").string();
                                winSdkUcrtPath = (entry.path() / "ucrt" / "x64").string();
                                break;
                            }
                        }
                    }
                    if (!winSdkUmPath.empty()) break;
                }

                if (linkerPath.empty() || msvcLibPath.empty() || winSdkUmPath.empty() || winSdkUcrtPath.empty()) {
                    std::cerr << "Error: Could not locate MSVC build tools or Windows SDK for linking.\n";
                    std::cerr << "  linkerPath: " << (linkerPath.empty() ? "MISSING" : linkerPath) << "\n";
                    std::cerr << "  msvcLibPath: " << (msvcLibPath.empty() ? "MISSING" : msvcLibPath) << "\n";
                    std::cerr << "  winSdkUmPath: " << (winSdkUmPath.empty() ? "MISSING" : winSdkUmPath) << "\n";
                    std::cerr << "  winSdkUcrtPath: " << (winSdkUcrtPath.empty() ? "MISSING" : winSdkUcrtPath) << "\n";
                    return 1;
                }

                // Locate dnaruntime.lib relative to the compiler executable (argv[0])
                std::filesystem::path execPath = std::filesystem::absolute(argv[0]);
                std::filesystem::path execDir = execPath.parent_path();
                std::filesystem::path runtimeLib = execDir / "dnaruntime.lib";
                if (!std::filesystem::exists(runtimeLib)) {
                    // Fallback to checking build output folder or sibling directories
                    runtimeLib = "dnaruntime.lib";
                }

                // Create link.bat to avoid quote parsing issues in std::system
                std::ofstream bat("link.bat");
                if (!bat.is_open()) {
                    std::cerr << "Error: Could not create temporary link.bat script.\n";
                    return 1;
                }
                bat << "@echo off\n";
                bat << "\"" << linkerPath << "\" " << objFile << " \"" << runtimeLib.string() << "\" /OUT:" << exeFile;
                bat << " /LIBPATH:\"" << msvcLibPath << "\"";
                bat << " /LIBPATH:\"" << winSdkUmPath << "\"";
                bat << " /LIBPATH:\"" << winSdkUcrtPath << "\"";
                bat << " libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib uuid.lib /subsystem:console /nologo\n";
                bat.close();

                int linkRes = std::system("link.bat");
                fs::remove("link.bat");

                if (linkRes != 0) {
                    std::cerr << "Error: Linking failed with exit code " << linkRes << ".\n";
                    return linkRes;
                }
                std::cout << "Successfully generated " << exeFile << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
    }

    return 0;
}
