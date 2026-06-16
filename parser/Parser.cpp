#include "Parser.h"

#include <cassert>
#include <cctype>
#include <sstream>

// =============================================================================
//  DNA v0.1 — Parser implementation
//  File: parser/Parser.cpp
// =============================================================================

namespace DNA {

// =============================================================================
// Constructor
// =============================================================================
Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)), pos_(0) {
    // Ensure there is always at least an EOF token
    if (tokens_.empty()) {
        tokens_.push_back(Token(TokenType::EOF_TOKEN, "", 0, 0));
    }
}

// =============================================================================
// Token-stream navigation
// =============================================================================
const Token& Parser::cur() const {
    return tokens_[pos_];
}

const Token& Parser::peekAt(std::size_t offset) const {
    std::size_t idx = pos_ + offset;
    if (idx >= tokens_.size()) return tokens_.back(); // EOF_TOKEN
    return tokens_[idx];
}

const Token& Parser::advance() {
    const Token& t = tokens_[pos_];
    if (!isAtEnd()) ++pos_;
    return t;
}

bool Parser::check(TokenType t) const {
    return cur().type == t;
}

bool Parser::match(TokenType t) {
    if (check(t)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TokenType t, const std::string& msg) {
    if (check(t)) return advance();

    int ln = cur().line;
    std::ostringstream oss;
    oss << "[Parse Error] Line " << ln << ": ";
    if (!msg.empty()) {
        oss << msg;
    } else {
        oss << "Expected " << tokenTypeName(t)
            << " but got " << tokenTypeName(cur().type);
        if (!cur().value.empty()) oss << " ('" << cur().value << "')";
    }
    reportError(oss.str(), ln);

    // Return a safe dummy so callers don't crash on the reference
    static Token dummy(TokenType::UNKNOWN, "", 0, 0);
    return dummy;
}

bool Parser::isAtEnd() const {
    return cur().type == TokenType::EOF_TOKEN;
}

void Parser::skipNewlines() {
    while (check(TokenType::NEWLINE)) advance();
}

// =============================================================================
// Error handling
// =============================================================================
void Parser::reportError(const std::string& msg, int /*line*/) {
    errors_.push_back(msg);
}

/// Skip tokens until we see something that could start a new statement or
/// declaration (NEWLINE, RBRACE, keyword), then consume trailing newlines.
void Parser::synchronize() {
    if (!isAtEnd()) {
        advance();
    }
    while (!isAtEnd()) {
        switch (cur().type) {
            case TokenType::NEWLINE:
                skipNewlines();
                return;
            case TokenType::RBRACE:
            case TokenType::ACTION:
            case TokenType::CLASS:
            case TokenType::LOAD:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::FOR:
            case TokenType::RETURN:
                return;
            default:
                advance();
                break;
        }
    }
}

// =============================================================================
// Type utilities
// =============================================================================
bool Parser::isTypeKeyword(TokenType t) const {
    switch (t) {
        case TokenType::INT:    case TokenType::FLOAT:
        case TokenType::DOUBLE: case TokenType::CHAR:
        case TokenType::BOOL:   case TokenType::STRING:
        case TokenType::VOID:
            return true;
        default:
            return false;
    }
}

/// Returns true when the upcoming (non-newline) tokens look like a var decl:
///   - Built-in type keyword     → definitely a var decl
///   - IDENTIFIER  IDENTIFIER    → class-type var decl  (e.g. Student s)
bool Parser::isVarDeclStart() const {
    if (isTypeKeyword(cur().type)) return true;

    if (cur().type == TokenType::IDENTIFIER) {
        // Look past newlines for a second identifier
        std::size_t i = pos_ + 1;
        while (i < tokens_.size() && tokens_[i].type == TokenType::NEWLINE) ++i;
        if (i < tokens_.size() && tokens_[i].type == TokenType::IDENTIFIER)
            return true;
    }
    return false;
}

TypeInfo Parser::parseType() {
    const Token& t = cur();

    if (isTypeKeyword(t.type)) {
        advance();
        switch (t.type) {
            case TokenType::INT:    return TypeInfo(TypeInfo::Kind::INT);
            case TokenType::FLOAT:  return TypeInfo(TypeInfo::Kind::FLOAT);
            case TokenType::DOUBLE: return TypeInfo(TypeInfo::Kind::DOUBLE);
            case TokenType::CHAR:   return TypeInfo(TypeInfo::Kind::CHAR);
            case TokenType::BOOL:   return TypeInfo(TypeInfo::Kind::BOOL);
            case TokenType::STRING: return TypeInfo(TypeInfo::Kind::STRING);
            case TokenType::VOID:   return TypeInfo(TypeInfo::Kind::VOID);
            default: break;
        }
    }

    if (t.type == TokenType::IDENTIFIER) {
        std::string name = t.value;
        advance();
        return TypeInfo(name); // user-defined class type
    }

    reportError("[Parse Error] Line " + std::to_string(t.line)
                + ": Expected type name, got '" + t.value + "'");
    return TypeInfo(); // UNKNOWN
}

// =============================================================================
// Parameter / argument lists
// =============================================================================

/// Parses  ( Type name, Type name, … )
/// Consumes the surrounding parentheses.
std::vector<Parameter> Parser::parseParamList() {
    expect(TokenType::LPAREN, "Expected '(' to start parameter list");
    std::vector<Parameter> params;

    skipNewlines();
    while (!check(TokenType::RPAREN) && !isAtEnd()) {
        Parameter p;
        p.type = parseType();
        const Token& nameToken = expect(TokenType::IDENTIFIER, "Expected parameter name");
        p.name = nameToken.value;
        params.push_back(std::move(p));

        skipNewlines();
        if (!match(TokenType::COMMA)) break;
        skipNewlines();
        if (check(TokenType::RPAREN)) {
            std::ostringstream oss;
            oss << "[Parse Error] Line " << cur().line << ": Trailing comma in parameter list";
            reportError(oss.str());
            throw ParseError(oss.str());
        }
    }

    expect(TokenType::RPAREN, "Expected ')' to close parameter list");
    return params;
}

/// Parses  expr, expr, …
/// Does NOT consume surrounding parentheses (caller handles those).
NodeList Parser::parseArgList() {
    NodeList args;
    skipNewlines();

    if (check(TokenType::RPAREN)) return args; // empty list

    args.push_back(parseExpression());
    skipNewlines();

    while (match(TokenType::COMMA)) {
        skipNewlines();
        if (check(TokenType::RPAREN)) break; // trailing comma tolerance
        args.push_back(parseExpression());
        skipNewlines();
    }

    return args;
}

// =============================================================================
// Top-level parsing
// =============================================================================
std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();

    skipNewlines();
    while (!isAtEnd()) {
        try {
            auto decl = parseTopLevelDecl();
            if (decl) program->declarations.push_back(std::move(decl));
        } catch (const ParseError&) {
            synchronize();
        }
        skipNewlines();
    }

    return program;
}

NodePtr Parser::parseTopLevelDecl() {
    skipNewlines();

    // Visibility modifier — may precede class or action
    Visibility vis = Visibility::PRIVATE;
    if (check(TokenType::PUBLIC))  { advance(); vis = Visibility::PUBLIC;  }
    else if (check(TokenType::PRIVATE)) { advance(); vis = Visibility::PRIVATE; }

    if (check(TokenType::LOAD))   return parseLoad();
    if (check(TokenType::CLASS))  return parseClass();
    if (check(TokenType::ACTION)) return parseAction(vis);

    // Bare type → top-level variable declaration (allowed for globals)
    if (isVarDeclStart()) return parseVarDecl();

    std::ostringstream oss;
    oss << "[Parse Error] Line " << cur().line
        << ": Unexpected token at top level: '" << cur().value << "'";
    reportError(oss.str());
    throw ParseError(oss.str());
}

// ── load <module> ──────────────────────────────────────────────────────────────
std::unique_ptr<LoadNode> Parser::parseLoad() {
    int ln = cur().line, col = cur().column;
    expect(TokenType::LOAD, "Expected 'load'");

    const Token& mod = expect(TokenType::IDENTIFIER, "Expected module name after 'load'");
    auto node = std::make_unique<LoadNode>();
    node->line   = ln;
    node->column = col;
    node->moduleName = mod.value;
    skipNewlines();
    return node;
}

// ── class Name { members } ────────────────────────────────────────────────────
std::unique_ptr<ClassNode> Parser::parseClass() {
    int ln = cur().line, col = cur().column;
    expect(TokenType::CLASS, "Expected 'class'");

    const Token& nameTok = expect(TokenType::IDENTIFIER, "Expected class name");
    auto node = std::make_unique<ClassNode>();
    node->line   = ln;
    node->column = col;
    node->name   = nameTok.value;

    skipNewlines();
    expect(TokenType::LBRACE, "Expected '{' after class name");
    skipNewlines();

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        try {
            auto member = parseClassMember();
            if (member) node->members.push_back(std::move(member));
        } catch (const ParseError&) {
            synchronize();
        }
        skipNewlines();
    }

    expect(TokenType::RBRACE, "Expected '}' to close class body");
    skipNewlines();
    return node;
}

// ── action ReturnType name(params) { body } ───────────────────────────────────
std::unique_ptr<ActionNode> Parser::parseAction(Visibility vis) {
    int ln = cur().line, col = cur().column;
    expect(TokenType::ACTION, "Expected 'action'");

    TypeInfo returnType = parseType();

    const Token& nameTok = expect(TokenType::IDENTIFIER, "Expected function name");
    auto node = std::make_unique<ActionNode>();
    node->line       = ln;
    node->column     = col;
    node->visibility = vis;
    node->returnType = returnType;
    node->name       = nameTok.value;
    node->params     = parseParamList();

    skipNewlines();
    node->body = parseBlock();
    return node;
}

// =============================================================================
// Class member parsing
// =============================================================================
NodePtr Parser::parseClassMember() {
    skipNewlines();

    // Optional access modifier
    Visibility vis = Visibility::PRIVATE;
    if (check(TokenType::PUBLIC))       { advance(); vis = Visibility::PUBLIC;  }
    else if (check(TokenType::PRIVATE)) { advance(); vis = Visibility::PRIVATE; }

    if (check(TokenType::ACTION))    return parseAction(vis);
    if (check(TokenType::CONSTRUCT)) return parseConstruct(vis);
    if (isVarDeclStart())            return parseFieldDecl(vis);

    std::ostringstream oss;
    oss << "[Parse Error] Line " << cur().line
        << ": Expected class member (field, action, or construct), got '"
        << cur().value << "'";
    reportError(oss.str());
    throw ParseError(oss.str());
}

std::unique_ptr<FieldDeclNode> Parser::parseFieldDecl(Visibility vis) {
    int ln = cur().line, col = cur().column;
    auto node = std::make_unique<FieldDeclNode>();
    node->line       = ln;
    node->column     = col;
    node->visibility = vis;
    node->type       = parseType();

    const Token& nameTok = expect(TokenType::IDENTIFIER, "Expected field name");
    node->name = nameTok.value;

    if (match(TokenType::ASSIGN)) {
        skipNewlines();
        node->initializer = parseExpression();
    }

    skipNewlines();
    return node;
}

// ── construct([params]) { body } ──────────────────────────────────────────────
std::unique_ptr<ConstructNode> Parser::parseConstruct(Visibility vis) {
    int ln = cur().line, col = cur().column;
    expect(TokenType::CONSTRUCT, "Expected 'construct'");

    auto node = std::make_unique<ConstructNode>();
    node->line       = ln;
    node->column     = col;
    node->visibility = vis;
    node->params     = parseParamList();

    skipNewlines();
    node->body = parseBlock();
    return node;
}

// =============================================================================
// Block and statement parsing
// =============================================================================
std::unique_ptr<BlockNode> Parser::parseBlock() {
    int ln = cur().line, col = cur().column;
    expect(TokenType::LBRACE, "Expected '{' to open block");
    skipNewlines();

    auto block = std::make_unique<BlockNode>();
    block->line   = ln;
    block->column = col;

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        skipNewlines();
        if (check(TokenType::RBRACE) || isAtEnd()) break;

        try {
            auto stmt = parseStatement();
            if (stmt) {
                block->statements.push_back(std::move(stmt));
                
                int endLine = tokens_[pos_ - 1].line;
                bool hasSemicolon = match(TokenType::SEMICOLON);
                skipNewlines();
                
                if (!check(TokenType::RBRACE) && !isAtEnd()) {
                    int nextStartLine = cur().line;
                    if (nextStartLine == endLine && !hasSemicolon) {
                        std::ostringstream oss;
                        oss << "[Parse Error] Line " << nextStartLine
                            << ": Expected newline or semicolon between statements, got '"
                            << cur().value << "'";
                        reportError(oss.str());
                        throw ParseError(oss.str());
                    }
                }
            }
        } catch (const ParseError&) {
            synchronize();
        }
        skipNewlines();
    }

    expect(TokenType::RBRACE, "Expected '}' to close block");
    return block;
}

NodePtr Parser::parseStatement() {
    skipNewlines();

    switch (cur().type) {
        case TokenType::IF:       return parseIf();
        case TokenType::WHILE:    return parseWhile();
        case TokenType::FOR:      return parseFor();
        case TokenType::RETURN:   return parseReturn();

        case TokenType::BREAK: {
            auto node = std::make_unique<BreakNode>();
            node->line   = cur().line;
            node->column = cur().column;
            advance();
            skipNewlines();
            return node;
        }
        case TokenType::CONTINUE: {
            auto node = std::make_unique<ContinueNode>();
            node->line   = cur().line;
            node->column = cur().column;
            advance();
            skipNewlines();
            return node;
        }

        default:
            if (isVarDeclStart()) return parseVarDecl();
            return parseExprStatement();
    }
}

/// Parse a variable declaration.
/// forLoopInit=true  → stop at SEMICOLON (for-loop header context).
/// forLoopInit=false → consume trailing newlines.
std::unique_ptr<VarDeclNode> Parser::parseVarDecl(bool forLoopInit) {
    int ln = cur().line, col = cur().column;
    auto node = std::make_unique<VarDeclNode>();
    node->line   = ln;
    node->column = col;
    node->type   = parseType();

    const Token& nameTok = expect(TokenType::IDENTIFIER, "Expected variable name");
    node->name = nameTok.value;

    if (match(TokenType::ASSIGN)) {
        skipNewlines();
        node->initializer = parseExpression();
    }

    if (!forLoopInit) skipNewlines();
    return node;
}

// ── if(condition) { } [else { }] ─────────────────────────────────────────────
std::unique_ptr<IfNode> Parser::parseIf() {
    int ln = cur().line, col = cur().column;
    expect(TokenType::IF, "Expected 'if'");
    expect(TokenType::LPAREN, "Expected '(' after 'if'");
    auto cond = parseExpression();
    expect(TokenType::RPAREN, "Expected ')' after if condition");

    skipNewlines();
    auto thenBlock = parseBlock();

    std::unique_ptr<BlockNode> elseBlock;
    skipNewlines();
    if (match(TokenType::ELSE)) {
        skipNewlines();
        elseBlock = parseBlock();
    }

    auto node = std::make_unique<IfNode>();
    node->line      = ln;
    node->column    = col;
    node->condition = std::move(cond);
    node->thenBlock = std::move(thenBlock);
    node->elseBlock = std::move(elseBlock);
    return node;
}

// ── while(condition) { } ──────────────────────────────────────────────────────
std::unique_ptr<WhileNode> Parser::parseWhile() {
    int ln = cur().line, col = cur().column;
    expect(TokenType::WHILE, "Expected 'while'");
    expect(TokenType::LPAREN, "Expected '(' after 'while'");
    auto cond = parseExpression();
    expect(TokenType::RPAREN, "Expected ')' after while condition");

    skipNewlines();
    auto body = parseBlock();

    auto node = std::make_unique<WhileNode>();
    node->line      = ln;
    node->column    = col;
    node->condition = std::move(cond);
    node->body      = std::move(body);
    return node;
}

// ── for(init; condition; update) { } ─────────────────────────────────────────
std::unique_ptr<ForNode> Parser::parseFor() {
    int ln = cur().line, col = cur().column;
    expect(TokenType::FOR, "Expected 'for'");
    expect(TokenType::LPAREN, "Expected '(' after 'for'");

    // init — a variable declaration terminated by SEMICOLON
    auto init = parseVarDecl(/*forLoopInit=*/true);
    expect(TokenType::SEMICOLON, "Expected ';' after for-loop initialiser");

    // condition
    auto cond = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';' after for-loop condition");

    // update — expression (typically i++ or i--)
    auto update = parseExpression();
    expect(TokenType::RPAREN, "Expected ')' after for-loop update");

    skipNewlines();
    auto body = parseBlock();

    auto node = std::make_unique<ForNode>();
    node->line      = ln;
    node->column    = col;
    node->init      = std::move(init);
    node->condition = std::move(cond);
    node->update    = std::move(update);
    node->body      = std::move(body);
    return node;
}

// ── return [expr] ─────────────────────────────────────────────────────────────
std::unique_ptr<ReturnNode> Parser::parseReturn() {
    int ln = cur().line, col = cur().column;
    expect(TokenType::RETURN, "Expected 'return'");

    auto node = std::make_unique<ReturnNode>();
    node->line   = ln;
    node->column = col;

    // Parse the return value unless we immediately see a newline or '}'
    if (!check(TokenType::NEWLINE) && !check(TokenType::RBRACE) && !isAtEnd()) {
        node->value = parseExpression();
    }

    skipNewlines();
    return node;
}

// ── Expression statement ──────────────────────────────────────────────────────
NodePtr Parser::parseExprStatement() {
    int ln = cur().line, col = cur().column;
    auto expr = parseExpression();
    skipNewlines();

    auto node = std::make_unique<ExprStmtNode>();
    node->line   = ln;
    node->column = col;
    node->expr   = std::move(expr);
    return node;
}

// =============================================================================
// Expression parsing  (Pratt-style recursive descent)
// =============================================================================

NodePtr Parser::parseExpression() {
    return parseAssignment();
}

// ── Assignment  (right-associative) ──────────────────────────────────────────
NodePtr Parser::parseAssignment() {
    auto left = parseLogicalOr();

    if (check(TokenType::ASSIGN)) {
        int ln = cur().line, col = cur().column;
        advance(); // consume =
        skipNewlines();
        auto value = parseAssignment(); // right-associative

        auto node = std::make_unique<AssignmentNode>();
        node->line   = ln;
        node->column = col;
        node->target = std::move(left);
        node->value  = std::move(value);
        return node;
    }

    return left;
}

// ── Logical OR  (||) ─────────────────────────────────────────────────────────
NodePtr Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();

    while (check(TokenType::OR)) {
        int ln = cur().line, col = cur().column;
        advance();
        skipNewlines();
        auto right = parseLogicalAnd();

        auto node   = std::make_unique<BinaryExprNode>();
        node->line  = ln; node->column = col;
        node->op    = "||";
        node->left  = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// ── Logical AND  (&&) ────────────────────────────────────────────────────────
NodePtr Parser::parseLogicalAnd() {
    auto left = parseEquality();

    while (check(TokenType::AND)) {
        int ln = cur().line, col = cur().column;
        advance();
        skipNewlines();
        auto right = parseEquality();

        auto node   = std::make_unique<BinaryExprNode>();
        node->line  = ln; node->column = col;
        node->op    = "&&";
        node->left  = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// ── Equality  (== !=) ─────────────────────────────────────────────────────────
NodePtr Parser::parseEquality() {
    auto left = parseComparison();

    while (check(TokenType::EQUAL) || check(TokenType::NOT_EQUAL)) {
        int ln = cur().line, col = cur().column;
        std::string op = (cur().type == TokenType::EQUAL) ? "==" : "!=";
        advance();
        skipNewlines();
        auto right = parseComparison();

        auto node   = std::make_unique<BinaryExprNode>();
        node->line  = ln; node->column = col;
        node->op    = op;
        node->left  = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// ── Comparison  (> < >= <=) ──────────────────────────────────────────────────
NodePtr Parser::parseComparison() {
    auto left = parseAddition();

    while (check(TokenType::GREATER)       || check(TokenType::LESS)       ||
           check(TokenType::GREATER_EQUAL)  || check(TokenType::LESS_EQUAL)) {
        int ln = cur().line, col = cur().column;
        std::string op;
        switch (cur().type) {
            case TokenType::GREATER:       op = ">";  break;
            case TokenType::LESS:          op = "<";  break;
            case TokenType::GREATER_EQUAL: op = ">="; break;
            case TokenType::LESS_EQUAL:    op = "<="; break;
            default: break;
        }
        advance();
        skipNewlines();
        auto right = parseAddition();

        auto node   = std::make_unique<BinaryExprNode>();
        node->line  = ln; node->column = col;
        node->op    = op;
        node->left  = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// ── Addition / Subtraction  (+ -) ────────────────────────────────────────────
NodePtr Parser::parseAddition() {
    auto left = parseMultiplication();

    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        int ln = cur().line, col = cur().column;
        std::string op = (cur().type == TokenType::PLUS) ? "+" : "-";
        advance();
        skipNewlines();
        auto right = parseMultiplication();

        auto node   = std::make_unique<BinaryExprNode>();
        node->line  = ln; node->column = col;
        node->op    = op;
        node->left  = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// ── Multiplication / Division / Mod  (* / %) ─────────────────────────────────
NodePtr Parser::parseMultiplication() {
    auto left = parseUnary();

    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::MOD)) {
        int ln = cur().line, col = cur().column;
        std::string op;
        switch (cur().type) {
            case TokenType::STAR:  op = "*"; break;
            case TokenType::SLASH: op = "/"; break;
            case TokenType::MOD:   op = "%"; break;
            default: break;
        }
        advance();
        skipNewlines();
        auto right = parseUnary();

        auto node   = std::make_unique<BinaryExprNode>();
        node->line  = ln; node->column = col;
        node->op    = op;
        node->left  = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// ── Unary prefix  (! -) ──────────────────────────────────────────────────────
NodePtr Parser::parseUnary() {
    if (check(TokenType::NOT) || check(TokenType::MINUS)) {
        int ln = cur().line, col = cur().column;
        std::string op = (cur().type == TokenType::NOT) ? "!" : "-";
        advance();
        auto operand = parseUnary(); // right-recursive

        auto node    = std::make_unique<UnaryExprNode>();
        node->line   = ln; node->column = col;
        node->op     = op;
        node->operand = std::move(operand);
        return node;
    }
    return parsePostfix();
}

// ── Postfix  (++ --) ─────────────────────────────────────────────────────────
NodePtr Parser::parsePostfix() {
    auto expr = parsePrimary();

    while (check(TokenType::INCREMENT) || check(TokenType::DECREMENT)) {
        int ln = cur().line, col = cur().column;
        std::string op = (cur().type == TokenType::INCREMENT) ? "++" : "--";
        advance();

        auto node    = std::make_unique<PostfixExprNode>();
        node->line   = ln; node->column = col;
        node->op     = op;
        node->operand = std::move(expr);
        expr = std::move(node);
    }

    // Member access chaining and method calls:  expr.field  |  expr.method(args)
    while (check(TokenType::DOT)) {
        int ln = cur().line, col = cur().column;
        advance(); // consume '.'
        const Token& memberTok = expect(TokenType::IDENTIFIER, "Expected member name after '.'");
        std::string memberName = memberTok.value;

        if (check(TokenType::LPAREN)) {
            // Method call:  expr.method(args)  →  CallExprNode
            // We encode the receiver inside the args list as the first hidden argument.
            // For the AST, we use a dedicated MemberAccessNode as callee representation
            // by storing the object in a wrapping ExprStmt and the call separately.
            // Simplest v0.1 approach: emit a CallExprNode with callee = "obj.method"
            // and store the receiver as the zeroth arg.
            advance(); // consume '('
            auto args = parseArgList();
            expect(TokenType::RPAREN, "Expected ')' after method arguments");

            // Build:  MemberAccess(object=expr, member=method)
            // and represent the call as a CallExprNode whose callee encodes the object.
            // We store the object via a MemberAccessNode kept as a special wrapper.
            auto receiver = std::make_unique<MemberAccessNode>();
            receiver->line   = ln; receiver->column = col;
            receiver->object = std::move(expr);
            receiver->member = memberName;

            // CallExprNode — callee string is "." + memberName for method calls.
            // The receiver MemberAccessNode is inserted as the implicit first arg.
            auto call    = std::make_unique<CallExprNode>();
            call->line   = ln; call->column = col;
            call->callee = "." + memberName;

            // Prepend receiver as first arg so semantic analysis can resolve it
            NodeList allArgs;
            allArgs.push_back(std::move(receiver));
            for (auto& a : args) allArgs.push_back(std::move(a));
            call->args = std::move(allArgs);

            expr = std::move(call);
        } else {
            // Plain field access:  expr.field
            auto node    = std::make_unique<MemberAccessNode>();
            node->line   = ln; node->column = col;
            node->object = std::move(expr);
            node->member = memberName;
            expr = std::move(node);
        }
    }

    return expr;
}

// ── Primary ──────────────────────────────────────────────────────────────────
NodePtr Parser::parsePrimary() {
    const Token& t = cur();

    // ── Integer literal ────────────────────────────────────────────────────────
    if (t.type == TokenType::NUMBER) {
        advance();
        auto n = std::make_unique<LiteralNode>(LiteralNode::Kind::INT, t.value);
        n->line = t.line; n->column = t.column;
        return n;
    }

    // ── Decimal literal ────────────────────────────────────────────────────────
    if (t.type == TokenType::DECIMAL) {
        advance();
        auto n = std::make_unique<LiteralNode>(LiteralNode::Kind::FLOAT, t.value);
        n->line = t.line; n->column = t.column;
        return n;
    }

    // ── String literal ─────────────────────────────────────────────────────────
    if (t.type == TokenType::STRING_LITERAL) {
        advance();
        auto n = std::make_unique<LiteralNode>(LiteralNode::Kind::STRING, t.value);
        n->line = t.line; n->column = t.column;
        return n;
    }

    // ── Char literal ───────────────────────────────────────────────────────────
    if (t.type == TokenType::CHAR_LITERAL) {
        advance();
        auto n = std::make_unique<LiteralNode>(LiteralNode::Kind::CHAR, t.value);
        n->line = t.line; n->column = t.column;
        return n;
    }

    // ── Boolean literals ───────────────────────────────────────────────────────
    if (t.type == TokenType::TRUE) {
        advance();
        auto n = std::make_unique<LiteralNode>(LiteralNode::Kind::BOOL_TRUE, "true");
        n->line = t.line; n->column = t.column;
        return n;
    }
    if (t.type == TokenType::FALSE) {
        advance();
        auto n = std::make_unique<LiteralNode>(LiteralNode::Kind::BOOL_FALSE, "false");
        n->line = t.line; n->column = t.column;
        return n;
    }

    // ── Built-in: print(args) ─────────────────────────────────────────────────
    if (t.type == TokenType::PRINT) {
        int ln = t.line, col = t.column;
        advance();
        expect(TokenType::LPAREN, "Expected '(' after 'print'");
        auto args = parseArgList();
        expect(TokenType::RPAREN, "Expected ')' after print arguments");

        auto node    = std::make_unique<CallExprNode>();
        node->line   = ln; node->column = col;
        node->callee = "print";
        node->args   = std::move(args);
        return node;
    }

    // ── Built-in: input(prompt) ───────────────────────────────────────────────
    if (t.type == TokenType::INPUT) {
        int ln = t.line, col = t.column;
        advance();
        expect(TokenType::LPAREN, "Expected '(' after 'input'");
        auto args = parseArgList();
        expect(TokenType::RPAREN, "Expected ')' after input arguments");

        auto node    = std::make_unique<CallExprNode>();
        node->line   = ln; node->column = col;
        node->callee = "input";
        node->args   = std::move(args);
        return node;
    }

    // ── Grouped expression  (expr) ────────────────────────────────────────────
    if (t.type == TokenType::LPAREN) {
        advance(); // consume (
        skipNewlines();
        auto expr = parseExpression();
        skipNewlines();
        expect(TokenType::RPAREN, "Expected ')' to close grouped expression");
        return expr;
    }

    // ── Identifier: variable, function call, or object creation ───────────────
    if (t.type == TokenType::IDENTIFIER) {
        int ln = t.line, col = t.column;
        std::string name = t.value;
        advance();

        // Function call / object creation
        if (match(TokenType::LPAREN)) {
            auto args = parseArgList();
            expect(TokenType::RPAREN, "Expected ')' after arguments");

            // Heuristic: uppercase first letter → ObjectCreationNode
            // e.g.  Student()  vs  add()
            if (!name.empty() && std::isupper(static_cast<unsigned char>(name[0]))) {
                auto node      = std::make_unique<ObjectCreationNode>();
                node->line     = ln; node->column = col;
                node->className = name;
                node->args     = std::move(args);
                return node;
            }

            auto node    = std::make_unique<CallExprNode>();
            node->line   = ln; node->column = col;
            node->callee = name;
            node->args   = std::move(args);
            return node;
        }

        // Plain identifier
        auto node = std::make_unique<IdentifierNode>(name);
        node->line   = ln;
        node->column = col;
        return node;
    }

    // ── Error recovery ────────────────────────────────────────────────────────
    std::ostringstream oss;
    oss << "[Parse Error] Line " << t.line
        << ": Unexpected token in expression: '"
        << t.value << "' (" << tokenTypeName(t.type) << ")";
    reportError(oss.str(), t.line);
    advance(); // consume the unexpected token

    // Return a dummy integer literal so callers receive a valid node
    auto dummy = std::make_unique<LiteralNode>(LiteralNode::Kind::INT, "0");
    dummy->line   = t.line;
    dummy->column = t.column;
    return dummy;
}

} // namespace DNA
