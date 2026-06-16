#pragma once

#include <string>

// =============================================================================
//  DNA v0.1 — Token definitions
//  File: lexer/Token.h
//
//  Defines every token type the Ribosome lexer can produce, the Token struct
//  that carries type + value + source location, and inline display utilities
//  that match the exact output format required by the Phase-1 milestone.
// =============================================================================

namespace DNA {

// -----------------------------------------------------------------------------
// TokenType
// -----------------------------------------------------------------------------
enum class TokenType {
    // ── Keywords ──────────────────────────────────────────────────────────────
    ACTION,       // action
    CLASS,        // class
    LOAD,         // load
    CONSTRUCT,    // construct

    IF,           // if
    ELSE,         // else
    FOR,          // for
    WHILE,        // while

    RETURN,       // return
    BREAK,        // break
    CONTINUE,     // continue

    PUBLIC,       // public
    PRIVATE,      // private

    // ── Built-in type keywords ─────────────────────────────────────────────────
    INT,          // int
    FLOAT,        // float
    DOUBLE,       // double
    CHAR,         // char   (keyword, not the character type)
    BOOL,         // bool
    STRING,       // String  ← official DNA string type (capital S)
    VOID,         // void

    // ── Boolean literal keywords ───────────────────────────────────────────────
    TRUE,         // true
    FALSE,        // false

    // ── Built-in function keywords ─────────────────────────────────────────────
    PRINT,        // print
    INPUT,        // input

    // ── Literals ──────────────────────────────────────────────────────────────
    NUMBER,           // integer literal   e.g. 42
    DECIMAL,          // floating literal  e.g. 3.14
    STRING_LITERAL,   // "..."
    CHAR_LITERAL,     // '.'

    // ── Identifier ────────────────────────────────────────────────────────────
    IDENTIFIER,

    // ── Arithmetic operators ───────────────────────────────────────────────────
    PLUS,         // +
    MINUS,        // -
    STAR,         // *
    SLASH,        // /
    MOD,          // %

    // ── Increment / Decrement ──────────────────────────────────────────────────
    INCREMENT,    // ++
    DECREMENT,    // --

    // ── Assignment ────────────────────────────────────────────────────────────
    ASSIGN,       // =

    // ── Comparison operators ───────────────────────────────────────────────────
    EQUAL,          // ==
    NOT_EQUAL,      // !=
    GREATER,        // >
    LESS,           // <
    GREATER_EQUAL,  // >=
    LESS_EQUAL,     // <=

    // ── Logical operators ──────────────────────────────────────────────────────
    AND,          // &&
    OR,           // ||
    NOT,          // !

    // ── Symbols ───────────────────────────────────────────────────────────────
    LPAREN,       // (
    RPAREN,       // )
    LBRACE,       // {
    RBRACE,       // }
    LBRACKET,     // [
    RBRACKET,     // ]
    COMMA,        // ,
    DOT,          // .
    SEMICOLON,    // ;   (used in for-loop headers only — DNA has no statement semicolons)

    // ── Special ───────────────────────────────────────────────────────────────
    NEWLINE,      // \n  — emitted explicitly per the v0.1 spec
    EOF_TOKEN,    // end of file
    UNKNOWN       // unrecognised character
};

// -----------------------------------------------------------------------------
// Token
// -----------------------------------------------------------------------------
struct Token {
    TokenType   type;
    std::string value;   // raw text of the token (without surrounding quotes for literals)
    int         line;
    int         column;

    Token(TokenType t, std::string v, int l, int c)
        : type(t), value(std::move(v)), line(l), column(c) {}
};

// -----------------------------------------------------------------------------
// Display utilities
// -----------------------------------------------------------------------------

/// Returns the canonical name string for a TokenType.
/// e.g.  tokenTypeName(TokenType::ACTION) == "ACTION"
///       tokenTypeName(TokenType::EOF_TOKEN) == "EOF"
inline std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::ACTION:         return "ACTION";
        case TokenType::CLASS:          return "CLASS";
        case TokenType::LOAD:           return "LOAD";
        case TokenType::CONSTRUCT:      return "CONSTRUCT";
        case TokenType::IF:             return "IF";
        case TokenType::ELSE:           return "ELSE";
        case TokenType::FOR:            return "FOR";
        case TokenType::WHILE:          return "WHILE";
        case TokenType::RETURN:         return "RETURN";
        case TokenType::BREAK:          return "BREAK";
        case TokenType::CONTINUE:       return "CONTINUE";
        case TokenType::PUBLIC:         return "PUBLIC";
        case TokenType::PRIVATE:        return "PRIVATE";
        case TokenType::INT:            return "INT";
        case TokenType::FLOAT:          return "FLOAT";
        case TokenType::DOUBLE:         return "DOUBLE";
        case TokenType::CHAR:           return "CHAR";
        case TokenType::BOOL:           return "BOOL";
        case TokenType::STRING:         return "STRING";
        case TokenType::VOID:           return "VOID";
        case TokenType::TRUE:           return "TRUE";
        case TokenType::FALSE:          return "FALSE";
        case TokenType::PRINT:          return "PRINT";
        case TokenType::INPUT:          return "INPUT";
        case TokenType::NUMBER:         return "NUMBER";
        case TokenType::DECIMAL:        return "DECIMAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::CHAR_LITERAL:   return "CHAR_LITERAL";
        case TokenType::IDENTIFIER:     return "IDENTIFIER";
        case TokenType::PLUS:           return "PLUS";
        case TokenType::MINUS:          return "MINUS";
        case TokenType::STAR:           return "STAR";
        case TokenType::SLASH:          return "SLASH";
        case TokenType::MOD:            return "MOD";
        case TokenType::INCREMENT:      return "INCREMENT";
        case TokenType::DECREMENT:      return "DECREMENT";
        case TokenType::ASSIGN:         return "ASSIGN";
        case TokenType::EQUAL:          return "EQUAL";
        case TokenType::NOT_EQUAL:      return "NOT_EQUAL";
        case TokenType::GREATER:        return "GREATER";
        case TokenType::LESS:           return "LESS";
        case TokenType::GREATER_EQUAL:  return "GREATER_EQUAL";
        case TokenType::LESS_EQUAL:     return "LESS_EQUAL";
        case TokenType::AND:            return "AND";
        case TokenType::OR:             return "OR";
        case TokenType::NOT:            return "NOT";
        case TokenType::LPAREN:         return "LPAREN";
        case TokenType::RPAREN:         return "RPAREN";
        case TokenType::LBRACE:         return "LBRACE";
        case TokenType::RBRACE:         return "RBRACE";
        case TokenType::LBRACKET:       return "LBRACKET";
        case TokenType::RBRACKET:       return "RBRACKET";
        case TokenType::COMMA:          return "COMMA";
        case TokenType::DOT:            return "DOT";
        case TokenType::SEMICOLON:      return "SEMICOLON";
        case TokenType::NEWLINE:        return "NEWLINE";
        case TokenType::EOF_TOKEN:      return "EOF";
        case TokenType::UNKNOWN:        return "UNKNOWN";
        default:                        return "UNKNOWN";
    }
}

/// Returns the formatted display string for a token in the Ribosome token
/// stream output format.
///
///   IDENTIFIER(main)
///   STRING_LITERAL("Hello DNA")
///   NUMBER(42)
///   DECIMAL(3.14)
///   CHAR_LITERAL('x')
///   ACTION                    ← keywords/operators show no value
///   NEWLINE
///   EOF
///
inline std::string tokenToDisplayString(const Token& tok) {
    switch (tok.type) {
        case TokenType::IDENTIFIER:
            return "IDENTIFIER(" + tok.value + ")";
        case TokenType::NUMBER:
            return "NUMBER(" + tok.value + ")";
        case TokenType::DECIMAL:
            return "DECIMAL(" + tok.value + ")";
        case TokenType::STRING_LITERAL:
            return "STRING_LITERAL(\"" + tok.value + "\")";
        case TokenType::CHAR_LITERAL:
            return "CHAR_LITERAL('" + tok.value + "')";
        case TokenType::UNKNOWN:
            return "UNKNOWN('" + tok.value + "')";
        default:
            return tokenTypeName(tok.type);
    }
}

} // namespace DNA
