#include "Lexer.h"

#include <cctype>
#include <sstream>
#include <stdexcept>

// =============================================================================
//  DNA v0.1 — Lexer implementation
//  File: lexer/Lexer.cpp
// =============================================================================

namespace DNA {

// -----------------------------------------------------------------------------
// Keyword map
// Every string that maps to a non-IDENTIFIER token is listed here.
// Order does not matter (unordered_map).
// -----------------------------------------------------------------------------
const std::unordered_map<std::string, TokenType>& Lexer::keywords() {
    static const std::unordered_map<std::string, TokenType> kMap = {
        // Control flow
        { "if",        TokenType::IF        },
        { "else",      TokenType::ELSE      },
        { "for",       TokenType::FOR       },
        { "while",     TokenType::WHILE     },
        // Declarations
        { "action",    TokenType::ACTION    },
        { "class",     TokenType::CLASS     },
        { "load",      TokenType::LOAD      },
        { "construct", TokenType::CONSTRUCT },
        // Jump statements
        { "return",    TokenType::RETURN    },
        { "break",     TokenType::BREAK     },
        { "continue",  TokenType::CONTINUE  },
        // Access modifiers
        { "public",    TokenType::PUBLIC    },
        { "private",   TokenType::PRIVATE   },
        // Primitive types
        { "int",       TokenType::INT       },
        { "float",     TokenType::FLOAT     },
        { "double",    TokenType::DOUBLE    },
        { "char",      TokenType::CHAR      },
        { "bool",      TokenType::BOOL      },
        { "String",    TokenType::STRING    },  // Capital S — official DNA String type
        { "void",      TokenType::VOID      },
        // Boolean literals
        { "true",      TokenType::TRUE      },
        { "false",     TokenType::FALSE     },
        // Built-in functions (treated as keywords so they receive named tokens)
        { "print",     TokenType::PRINT     },
        { "input",     TokenType::INPUT     },
    };
    return kMap;
}

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
Lexer::Lexer(const std::string& source)
    : source_(source), pos_(0), line_(1), col_(1) {}

// -----------------------------------------------------------------------------
// Source navigation
// -----------------------------------------------------------------------------
char Lexer::current() const {
    if (pos_ >= source_.size()) return '\0';
    return source_[pos_];
}

char Lexer::peekNext() const {
    if (pos_ + 1 >= source_.size()) return '\0';
    return source_[pos_ + 1];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        ++line_;
        col_ = 1;
    } else {
        ++col_;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
Token Lexer::makeToken(TokenType type, const std::string& value,
                       int line, int col) const {
    return Token(type, value, line, col);
}

void Lexer::reportError(const std::string& msg) {
    std::ostringstream oss;
    oss << "[Lexer] Line " << line_ << ", Col " << col_ << ": " << msg;
    errors_.push_back(oss.str());
}

// -----------------------------------------------------------------------------
// Whitespace / comments
// -----------------------------------------------------------------------------
void Lexer::skipInlineWhitespace() {
    while (!isAtEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else {
            break;
        }
    }
}

// Called when current() == '/' && peekNext() == '/'.
// Consumes everything up to (but NOT including) the \n.
void Lexer::skipLineComment() {
    while (!isAtEnd() && current() != '\n') {
        advance();
    }
    // The \n itself is left for the main loop to emit as NEWLINE.
}

// -----------------------------------------------------------------------------
// String literal   "…"
// Supports escape sequences: \n \t \r \" \' \\
// The stored value is the decoded content (without surrounding quotes).
// -----------------------------------------------------------------------------
Token Lexer::readStringLiteral() {
    int startLine = line_;
    int startCol  = col_;
    advance(); // consume opening "

    std::string value;
    while (!isAtEnd() && current() != '"') {
        if (current() == '\n') {
            reportError("Unterminated string literal");
            break;
        }
        if (current() == '\\') {
            advance(); // consume backslash
            switch (current()) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '"':  value += '"';  break;
                case '\'': value += '\''; break;
                case '\\': value += '\\'; break;
                default:
                    reportError(std::string("Unknown escape sequence: \\")
                                + current());
                    value += current();
                    break;
            }
            if (!isAtEnd()) advance();
        } else {
            value += current();
            advance();
        }
    }
    if (!isAtEnd()) advance(); // consume closing "
    return makeToken(TokenType::STRING_LITERAL, value, startLine, startCol);
}

// -----------------------------------------------------------------------------
// Char literal   '.'
// Supports same escape sequences as string literals.
// -----------------------------------------------------------------------------
Token Lexer::readCharLiteral() {
    int startLine = line_;
    int startCol  = col_;
    advance(); // consume opening '

    std::string value;
    if (!isAtEnd() && current() != '\'') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case '\'': value += '\''; break;
                case '\\': value += '\\'; break;
                default:   value += current(); break;
            }
            if (!isAtEnd()) advance();
        } else {
            value += current();
            advance();
        }
    }

    if (!isAtEnd() && current() == '\'') {
        advance(); // consume closing '
    } else {
        reportError("Unterminated char literal");
    }
    return makeToken(TokenType::CHAR_LITERAL, value, startLine, startCol);
}

// -----------------------------------------------------------------------------
// Number literal
// Produces NUMBER for integers, DECIMAL for floating-point values.
// Accepts at most one '.' — a second dot ends the token.
// -----------------------------------------------------------------------------
Token Lexer::readNumber() {
    int startLine = line_;
    int startCol  = col_;
    std::string value;
    bool isDecimal = false;

    while (!isAtEnd()) {
        char c = current();
        if (std::isdigit(c)) {
            value += c;
            advance();
        } else if (c == '.' && !isDecimal && std::isdigit(peekNext())) {
            // Accept a decimal point only if followed by at least one digit
            isDecimal = true;
            value += c;
            advance();
        } else {
            break;
        }
    }

    TokenType t = isDecimal ? TokenType::DECIMAL : TokenType::NUMBER;
    return makeToken(t, value, startLine, startCol);
}

// -----------------------------------------------------------------------------
// Identifier / keyword
// Pattern: [a-zA-Z_][a-zA-Z0-9_]*
// Checks the keyword map; emits IDENTIFIER if not found.
// -----------------------------------------------------------------------------
Token Lexer::readIdentifierOrKeyword() {
    int startLine = line_;
    int startCol  = col_;
    std::string value;

    while (!isAtEnd() && (std::isalnum(current()) || current() == '_')) {
        value += current();
        advance();
    }

    const auto& kw = keywords();
    auto it = kw.find(value);
    TokenType t = (it != kw.end()) ? it->second : TokenType::IDENTIFIER;
    return makeToken(t, value, startLine, startCol);
}

// -----------------------------------------------------------------------------
// Main tokenize loop
// -----------------------------------------------------------------------------
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    tokens.reserve(256); // reasonable starting capacity

    while (!isAtEnd()) {
        int startLine = line_;
        int startCol  = col_;
        char c = current();

        // ── Inline whitespace (spaces, tabs, carriage returns) ─────────────────
        if (c == ' ' || c == '\t' || c == '\r') {
            skipInlineWhitespace();
            continue;
        }

        // ── Newline → explicit NEWLINE token ──────────────────────────────────
        if (c == '\n') {
            tokens.push_back(makeToken(TokenType::NEWLINE, "\\n", startLine, startCol));
            advance();
            continue;
        }

        // ── Line comments // ──────────────────────────────────────────────────
        if (c == '/' && peekNext() == '/') {
            skipLineComment();
            continue;
        }

        // ── String literal ────────────────────────────────────────────────────
        if (c == '"') {
            tokens.push_back(readStringLiteral());
            continue;
        }

        // ── Char literal ──────────────────────────────────────────────────────
        if (c == '\'') {
            tokens.push_back(readCharLiteral());
            continue;
        }

        // ── Number literal ────────────────────────────────────────────────────
        if (std::isdigit(c)) {
            tokens.push_back(readNumber());
            continue;
        }

        // ── Identifier / keyword ──────────────────────────────────────────────
        if (std::isalpha(c) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        // ── Operators and symbols (consume the leading character first) ────────
        advance(); // c is consumed; peekNext() is now the char after c

        switch (c) {
            // ── Arithmetic ─────────────────────────────────────────────────────
            case '+':
                if (!isAtEnd() && current() == '+') {
                    advance();
                    tokens.push_back(makeToken(TokenType::INCREMENT, "++", startLine, startCol));
                } else {
                    tokens.push_back(makeToken(TokenType::PLUS, "+", startLine, startCol));
                }
                break;

            case '-':
                if (!isAtEnd() && current() == '-') {
                    advance();
                    tokens.push_back(makeToken(TokenType::DECREMENT, "--", startLine, startCol));
                } else {
                    tokens.push_back(makeToken(TokenType::MINUS, "-", startLine, startCol));
                }
                break;

            case '*':
                tokens.push_back(makeToken(TokenType::STAR, "*", startLine, startCol));
                break;

            case '/':
                // Note: '//' is handled above, so bare '/' reaches here.
                tokens.push_back(makeToken(TokenType::SLASH, "/", startLine, startCol));
                break;

            case '%':
                tokens.push_back(makeToken(TokenType::MOD, "%", startLine, startCol));
                break;

            // ── Assignment / equality ───────────────────────────────────────────
            case '=':
                if (!isAtEnd() && current() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenType::EQUAL, "==", startLine, startCol));
                } else {
                    tokens.push_back(makeToken(TokenType::ASSIGN, "=", startLine, startCol));
                }
                break;

            // ── Not / not-equal ─────────────────────────────────────────────────
            case '!':
                if (!isAtEnd() && current() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenType::NOT_EQUAL, "!=", startLine, startCol));
                } else {
                    tokens.push_back(makeToken(TokenType::NOT, "!", startLine, startCol));
                }
                break;

            // ── Comparison ──────────────────────────────────────────────────────
            case '>':
                if (!isAtEnd() && current() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenType::GREATER_EQUAL, ">=", startLine, startCol));
                } else {
                    tokens.push_back(makeToken(TokenType::GREATER, ">", startLine, startCol));
                }
                break;

            case '<':
                if (!isAtEnd() && current() == '=') {
                    advance();
                    tokens.push_back(makeToken(TokenType::LESS_EQUAL, "<=", startLine, startCol));
                } else {
                    tokens.push_back(makeToken(TokenType::LESS, "<", startLine, startCol));
                }
                break;

            // ── Logical AND (&&) ────────────────────────────────────────────────
            case '&':
                if (!isAtEnd() && current() == '&') {
                    advance();
                    tokens.push_back(makeToken(TokenType::AND, "&&", startLine, startCol));
                } else {
                    reportError("Unexpected '&' — did you mean '&&'?");
                    tokens.push_back(makeToken(TokenType::UNKNOWN, "&", startLine, startCol));
                }
                break;

            // ── Logical OR (||) ─────────────────────────────────────────────────
            case '|':
                if (!isAtEnd() && current() == '|') {
                    advance();
                    tokens.push_back(makeToken(TokenType::OR, "||", startLine, startCol));
                } else {
                    reportError("Unexpected '|' — did you mean '||'?");
                    tokens.push_back(makeToken(TokenType::UNKNOWN, "|", startLine, startCol));
                }
                break;

            // ── Grouping / brackets ─────────────────────────────────────────────
            case '(': tokens.push_back(makeToken(TokenType::LPAREN,   "(", startLine, startCol)); break;
            case ')': tokens.push_back(makeToken(TokenType::RPAREN,   ")", startLine, startCol)); break;
            case '{': tokens.push_back(makeToken(TokenType::LBRACE,   "{", startLine, startCol)); break;
            case '}': tokens.push_back(makeToken(TokenType::RBRACE,   "}", startLine, startCol)); break;
            case '[': tokens.push_back(makeToken(TokenType::LBRACKET, "[", startLine, startCol)); break;
            case ']': tokens.push_back(makeToken(TokenType::RBRACKET, "]", startLine, startCol)); break;

            // ── Punctuation ─────────────────────────────────────────────────────
            case ',': tokens.push_back(makeToken(TokenType::COMMA,     ",", startLine, startCol)); break;
            case '.': tokens.push_back(makeToken(TokenType::DOT,       ".", startLine, startCol)); break;
            case ';': tokens.push_back(makeToken(TokenType::SEMICOLON, ";", startLine, startCol)); break;

            // ── Unknown character ───────────────────────────────────────────────
            default:
                reportError(std::string("Unexpected character: '") + c + "'");
                tokens.push_back(makeToken(TokenType::UNKNOWN,
                                           std::string(1, c), startLine, startCol));
                break;
        }
    }

    // Always terminate with EOF
    tokens.push_back(makeToken(TokenType::EOF_TOKEN, "", line_, col_));
    return tokens;
}

} // namespace DNA
