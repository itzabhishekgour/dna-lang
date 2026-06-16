#pragma once

#include "Token.h"
#include <string>
#include <vector>
#include <unordered_map>

// =============================================================================
//  DNA v0.1 — Lexer
//  File: lexer/Lexer.h
//
//  The Lexer converts raw DNA source text into a flat token stream.
//  Rules:
//    - Spaces, tabs, \r are silently skipped (inline whitespace).
//    - \n emits an explicit NEWLINE token (not discarded).
//    - // comments consume until (but not including) the newline.
//    - No semicolons as statement terminators; SEMICOLON is only valid inside
//      for-loop headers.
//    - String type is "String" (capital S) → TokenType::STRING.
//    - print / input are keyword tokens (PRINT / INPUT).
//    - construct is a keyword token (CONSTRUCT).
// =============================================================================

namespace DNA {

class Lexer {
public:
    explicit Lexer(const std::string& source);

    /// Tokenizes the entire source and returns a complete token stream.
    /// The last token is always EOF_TOKEN.
    std::vector<Token> tokenize();

    /// Accumulated lexer errors (non-fatal; tokenization continues).
    const std::vector<std::string>& errors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    std::string              source_;
    std::size_t              pos_;
    int                      line_;
    int                      col_;
    std::vector<std::string> errors_;

    // ── Source navigation ──────────────────────────────────────────────────────
    char current()  const;          // source_[pos_]  or '\0' at EOF
    char peekNext() const;          // source_[pos_+1] or '\0'
    char advance();                 // return current(), then move forward
    bool isAtEnd()  const;

    // ── Whitespace / comments ──────────────────────────────────────────────────
    void skipInlineWhitespace();    // spaces, tabs, \r  (NOT \n)
    void skipLineComment();         // // … (stops before \n)

    // ── Readers ───────────────────────────────────────────────────────────────
    Token readStringLiteral();      // "…"
    Token readCharLiteral();        // '.'
    Token readNumber();             // 42  |  3.14
    Token readIdentifierOrKeyword();// alpha / _ → IDENTIFIER or keyword

    // ── Helpers ───────────────────────────────────────────────────────────────
    Token makeToken(TokenType type, const std::string& value,
                    int line, int col) const;
    void  reportError(const std::string& msg);

    // ── Keyword table (static, initialised once) ───────────────────────────────
    static const std::unordered_map<std::string, TokenType>& keywords();
};

} // namespace DNA
