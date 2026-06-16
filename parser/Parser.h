#pragma once

#include "../ast/ASTNode.h"
#include "../lexer/Token.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
//  DNA v0.1 — Parser
//  File: parser/Parser.h
//
//  Recursive-descent parser.  Consumes a flat token stream produced by the
//  Lexer and builds a typed AST rooted at ProgramNode.
//
//  NEWLINE tokens:
//    - Emitted by the lexer but mostly treated as optional statement
//      separators by the parser.  skipNewlines() discards them wherever
//      whitespace is insignificant.
//    - NEWLINE tokens inside for-loop headers do not occur in practice
//      (the header is on one line) and are skipped automatically.
//
//  Operator precedence (lowest → highest):
//    1. Assignment           =            (right-associative)
//    2. Logical OR           ||
//    3. Logical AND          &&
//    4. Equality             == !=
//    5. Comparison           > < >= <=
//    6. Addition/Subtraction + -
//    7. Multiplication/…     * / %
//    8. Unary prefix         ! -
//    9. Postfix              ++ --
//   10. Primary              literals, identifiers, calls, (expr)
// =============================================================================

namespace DNA {

// -----------------------------------------------------------------------------
// ParseError — thrown on unrecoverable syntax errors
// -----------------------------------------------------------------------------
class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

// -----------------------------------------------------------------------------
// Parser
// -----------------------------------------------------------------------------
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    /// Parse the token stream into a complete program AST.
    std::unique_ptr<ProgramNode> parse();

    /// All accumulated parse errors (even when some were recovered).
    const std::vector<std::string>& errors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    std::vector<Token>       tokens_;
    std::size_t              pos_;
    std::vector<std::string> errors_;

    // ── Token stream navigation ────────────────────────────────────────────────
    const Token& cur()  const;                              // current token
    const Token& peekAt(std::size_t offset) const;         // pos_ + offset
    const Token& advance();                                 // consume + return
    bool  check(TokenType t) const;
    bool  match(TokenType t);                               // consume if matches
    const Token& expect(TokenType t, const std::string& msg);
    bool  isAtEnd() const;

    void  skipNewlines();  // skip any number of consecutive NEWLINE tokens

    // ── Type keyword check ─────────────────────────────────────────────────────
    bool isTypeKeyword(TokenType t) const;

    /// Returns true when the upcoming tokens look like a variable declaration
    /// (built-in type keyword OR  ClassName + IDENTIFIER).
    bool isVarDeclStart() const;

    // ── Error handling ─────────────────────────────────────────────────────────
    void reportError(const std::string& msg, int line = -1);
    void synchronize();   // skip to next NEWLINE / RBRACE on error

    // ── Type parsing ────────────────────────────────────────────────────────────
    TypeInfo parseType();  // consumes one type keyword or class-name identifier

    // ── Parameter list ─────────────────────────────────────────────────────────
    std::vector<Parameter> parseParamList();   // ( param, param, … )  — parens consumed
    NodeList               parseArgList();     // expr, expr, …        — parens NOT consumed

    // ── Top-level ─────────────────────────────────────────────────────────────
    NodePtr parseTopLevelDecl();
    std::unique_ptr<LoadNode>   parseLoad();
    std::unique_ptr<ClassNode>  parseClass();
    std::unique_ptr<ActionNode> parseAction(Visibility vis);

    // ── Class members ──────────────────────────────────────────────────────────
    NodePtr                        parseClassMember();
    std::unique_ptr<FieldDeclNode> parseFieldDecl(Visibility vis);
    std::unique_ptr<ConstructNode> parseConstruct(Visibility vis);

    // ── Block and statements ───────────────────────────────────────────────────
    std::unique_ptr<BlockNode> parseBlock();
    NodePtr                    parseStatement();
    std::unique_ptr<VarDeclNode> parseVarDecl(bool forLoopInit = false);
    std::unique_ptr<IfNode>      parseIf();
    std::unique_ptr<WhileNode>   parseWhile();
    std::unique_ptr<ForNode>     parseFor();
    std::unique_ptr<ReturnNode>  parseReturn();
    NodePtr                      parseExprStatement();

    // ── Expressions (Pratt-style precedence climbing) ─────────────────────────
    NodePtr parseExpression();
    NodePtr parseAssignment();
    NodePtr parseLogicalOr();
    NodePtr parseLogicalAnd();
    NodePtr parseEquality();
    NodePtr parseComparison();
    NodePtr parseAddition();
    NodePtr parseMultiplication();
    NodePtr parseUnary();
    NodePtr parsePostfix();
    NodePtr parsePrimary();
};

} // namespace DNA
