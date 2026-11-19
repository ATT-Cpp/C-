#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// ============================================================================
// 1. LEXER
// ============================================================================

enum class TokenKind {
    // Literals & Identifiers
    NUMBER,
    STRING,
    IDENT,

    // Keywords
    LET,
    FN,
    RETURN,
    IF,
    ELSE,
    WHILE,
    PRINT,
    TRUE,
    FALSE,
    TYPE_NUM,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_STR,

    // Operators & Delimiters
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    BANG,       // !
    BANG_EQ,    // !=
    EQ,         // =
    EQ_EQ,      // ==
    LT,         // <
    LT_EQ,      // <=
    GT,         // >
    GT_EQ,      // >=
    AND_AND,    // &&
    OR_OR,      // ||
    ARROW,      // ->
    COLON,      // :
    COMMA,      // ,
    SEMICOLON,  // ;
    LPAREN,     // (
    RPAREN,     // )
    LBRACE,     // {
    RBRACE,     // }

    END_OF_FILE
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    double num_value = 0.0;
    int line = 1;
};

class Lexer {
public:
    explicit Lexer(const std::string& source) : src_(source) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (!is_at_end()) {
            skip_whitespace_and_comments();
            if (is_at_end()) break;
            tokens.push_back(scan_token());
        }
        tokens.push_back(Token{TokenKind::END_OF_FILE, "", 0.0, line_});
        return tokens;
    }

private:
    const std::string& src_;
    size_t pos_ = 0;
    int line_ = 1;

    bool is_at_end() const { return pos_ >= src_.size(); }
    char peek() const { return is_at_end() ? '\0' : src_[pos_]; }
    char peek_next() const { return (pos_ + 1 >= src_.size()) ? '\0' : src_[pos_ + 1]; }

    char advance() {
        char c = src_[pos_++];
        if (c == '\n') ++line_;
        return c;
    }

    bool match(char expected) {
        if (is_at_end() || src_[pos_] != expected) return false;
        ++pos_;
        return true;
    }

    void skip_whitespace_and_comments() {
        while (!is_at_end()) {
            char c = peek();
            if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
                advance();
            } else if (c == '/' && peek_next() == '/') {
                while (!is_at_end() && peek() != '\n') advance();
            } else {
                break;
            }
        }
    }

    Token make_token(TokenKind kind, const std::string& lex, double val = 0.0) const {
        return Token{kind, lex, val, line_};
    }

    Token scan_token() {
        char c = advance();

        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = pos_ - 1;
            while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
            if (!is_at_end() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peek_next()))) {
                advance();
                while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
            }
            std::string text = src_.substr(start, pos_ - start);
            return make_token(TokenKind::NUMBER, text, std::stod(text));
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = pos_ - 1;
            while (!is_at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
                advance();
            }
            std::string text = src_.substr(start, pos_ - start);
            static const std::unordered_map<std::string, TokenKind> keywords = {
                {"let", TokenKind::LET},
                {"fn", TokenKind::FN},
                {"return", TokenKind::RETURN},
                {"if", TokenKind::IF},
                {"else", TokenKind::ELSE},
                {"while", TokenKind::WHILE},
                {"print", TokenKind::PRINT},
                {"true", TokenKind::TRUE},
                {"false", TokenKind::FALSE},
                {"num", TokenKind::TYPE_NUM},
                {"bool", TokenKind::TYPE_BOOL},
                {"void", TokenKind::TYPE_VOID},
                {"str", TokenKind::TYPE_STR},
            };
            auto it = keywords.find(text);
            return make_token(it != keywords.end() ? it->second : TokenKind::IDENT, text);
        }

        switch (c) {
            case '"': {
                std::string value;
                while (!is_at_end() && peek() != '"') {
                    char ch = advance();
                    if (ch != '\\') {
                        value.push_back(ch);
                        continue;
                    }
                    if (is_at_end()) break;
                    char escaped = advance();
                    switch (escaped) {
                        case '"': value.push_back('"'); break;
                        case '\\': value.push_back('\\'); break;
                        case 'n': value.push_back('\n'); break;
                        case 't': value.push_back('\t'); break;
                        default:
                            throw std::runtime_error("Lex error on line " + std::to_string(line_) +
                                                     ": unsupported string escape");
                    }
                }
                if (is_at_end()) {
                    throw std::runtime_error("Lex error on line " + std::to_string(line_) +
                                             ": unterminated string");
                }
                advance();
                return make_token(TokenKind::STRING, value);
            }
            case '+': return make_token(TokenKind::PLUS, "+");
            case '-':
                if (match('>')) return make_token(TokenKind::ARROW, "->");
                return make_token(TokenKind::MINUS, "-");
            case '*': return make_token(TokenKind::STAR, "*");
            case '/': return make_token(TokenKind::SLASH, "/");
            case '!': return make_token(match('=') ? TokenKind::BANG_EQ : TokenKind::BANG, pos_ - 1 ? "!=" : "!");
            case '=': return make_token(match('=') ? TokenKind::EQ_EQ : TokenKind::EQ, "=");
            case '<': return make_token(match('=') ? TokenKind::LT_EQ : TokenKind::LT, "<");
            case '>': return make_token(match('=') ? TokenKind::GT_EQ : TokenKind::GT, ">");
            case '&':
                if (match('&')) return make_token(TokenKind::AND_AND, "&&");
                throw std::runtime_error("Lex error on line " + std::to_string(line_) + ": unexpected '&'");
            case '|':
                if (match('|')) return make_token(TokenKind::OR_OR, "||");
                throw std::runtime_error("Lex error on line " + std::to_string(line_) + ": unexpected '|'");
            case ':': return make_token(TokenKind::COLON, ":");
            case ',': return make_token(TokenKind::COMMA, ",");
            case ';': return make_token(TokenKind::SEMICOLON, ";");
            case '(': return make_token(TokenKind::LPAREN, "(");
            case ')': return make_token(TokenKind::RPAREN, ")");
            case '{': return make_token(TokenKind::LBRACE, "{");
            case '}': return make_token(TokenKind::RBRACE, "}");
            default:
                throw std::runtime_error("Lex error on line " + std::to_string(line_) +
                                         ": unexpected character '" + std::string(1, c) + "'");
        }
    }
};

// ============================================================================
// 2. AST NODES & TYPES
// ============================================================================

enum class Type {
    NUM,
    BOOL,
    STR,
    VOID,
    UNKNOWN
};

inline std::string type_name(Type t) {
    switch (t) {
        case Type::NUM: return "num";
        case Type::BOOL: return "bool";
        case Type::STR: return "str";
        case Type::VOID: return "void";
        default: return "unknown";
    }
}

struct Expr {
    int line;
    Type inferred_type = Type::UNKNOWN;
    explicit Expr(int line) : line(line) {}
    virtual ~Expr() = default;
};

using ExprPtr = std::unique_ptr<Expr>;

struct NumberExpr : Expr {
    double value;
    NumberExpr(double v, int line) : Expr(line), value(v) { inferred_type = Type::NUM; }
};

struct BoolExpr : Expr {
    bool value;
    BoolExpr(bool v, int line) : Expr(line), value(v) { inferred_type = Type::BOOL; }
};

struct StringExpr : Expr {
    std::string value;
    StringExpr(std::string v, int line) : Expr(line), value(std::move(v)) { inferred_type = Type::STR; }
};

struct VarExpr : Expr {
    std::string name;
    VarExpr(std::string n, int line) : Expr(line), name(std::move(n)) {}
};

struct AssignExpr : Expr {
    std::string name;
    ExprPtr value;
    AssignExpr(std::string n, ExprPtr v, int line) : Expr(line), name(std::move(n)), value(std::move(v)) {}
};

struct UnaryExpr : Expr {
    TokenKind op;
    ExprPtr operand;
    UnaryExpr(TokenKind o, ExprPtr e, int line) : Expr(line), op(o), operand(std::move(e)) {}
};

struct BinaryExpr : Expr {
    ExprPtr left;
    TokenKind op;
    ExprPtr right;
    BinaryExpr(ExprPtr l, TokenKind o, ExprPtr r, int line)
        : Expr(line), left(std::move(l)), op(o), right(std::move(r)) {}
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
    CallExpr(std::string c, std::vector<ExprPtr> a, int line)
        : Expr(line), callee(std::move(c)), args(std::move(a)) {}
};

struct Stmt {
    int line;
    explicit Stmt(int line) : line(line) {}
    virtual ~Stmt() = default;
};

using StmtPtr = std::unique_ptr<Stmt>;

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
    BlockStmt(std::vector<StmtPtr> stmts, int line) : Stmt(line), statements(std::move(stmts)) {}
};

struct LetStmt : Stmt {
    std::string name;
    ExprPtr initializer;
    LetStmt(std::string n, ExprPtr init, int line) : Stmt(line), name(std::move(n)), initializer(std::move(init)) {}
};

struct ExprStmt : Stmt {
    ExprPtr expr;
    ExprStmt(ExprPtr e, int line) : Stmt(line), expr(std::move(e)) {}
};

struct PrintStmt : Stmt {
    ExprPtr expr;
    PrintStmt(ExprPtr e, int line) : Stmt(line), expr(std::move(e)) {}
};

struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr then_branch;
    StmtPtr else_branch; // optional
    IfStmt(ExprPtr cond, StmtPtr then_b, StmtPtr else_b, int line)
        : Stmt(line), condition(std::move(cond)), then_branch(std::move(then_b)), else_branch(std::move(else_b)) {}
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;
    WhileStmt(ExprPtr cond, StmtPtr b, int line) : Stmt(line), condition(std::move(cond)), body(std::move(b)) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value; // optional
    ReturnStmt(ExprPtr v, int line) : Stmt(line), value(std::move(v)) {}
};

struct Param {
    std::string name;
    Type type;
};

struct FnDecl : Stmt {
    std::string name;
    std::vector<Param> params;
    Type return_type;
    std::unique_ptr<BlockStmt> body;
    FnDecl(std::string n, std::vector<Param> p, Type ret, std::unique_ptr<BlockStmt> b, int line)
        : Stmt(line), name(std::move(n)), params(std::move(p)), return_type(ret), body(std::move(b)) {}
};

// ============================================================================
// 3. PARSER
// ============================================================================

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

    std::vector<StmtPtr> parse_program() {
        std::vector<StmtPtr> stmts;
        while (!is_at_end()) {
            stmts.push_back(declaration());
        }
        return stmts;
    }

private:
    const std::vector<Token>& tokens_;
    size_t current_ = 0;

    bool is_at_end() const { return peek().kind == TokenKind::END_OF_FILE; }
    const Token& peek() const { return tokens_[current_]; }
    const Token& previous() const { return tokens_[current_ - 1]; }

    bool check(TokenKind kind) const { return !is_at_end() && peek().kind == kind; }

    const Token& advance() {
        if (!is_at_end()) ++current_;
        return previous();
    }

    bool match(std::initializer_list<TokenKind> kinds) {
        for (TokenKind k : kinds) {
            if (check(k)) {
                advance();
                return true;
            }
        }
        return false;
    }

    const Token& consume(TokenKind kind, const std::string& msg) {
        if (check(kind)) return advance();
        throw std::runtime_error("Parse error on line " + std::to_string(peek().line) + ": " + msg);
    }

    Type parse_type() {
        if (match({TokenKind::TYPE_NUM})) return Type::NUM;
        if (match({TokenKind::TYPE_BOOL})) return Type::BOOL;
        if (match({TokenKind::TYPE_STR})) return Type::STR;
        if (match({TokenKind::TYPE_VOID})) return Type::VOID;
        throw std::runtime_error("Parse error on line " + std::to_string(peek().line) +
                                 ": expected type (num, bool, str, void)");
    }

    StmtPtr declaration() {
        if (match({TokenKind::FN})) return fn_declaration();
        if (match({TokenKind::LET})) return let_declaration();
        return statement();
    }

    std::unique_ptr<FnDecl> fn_declaration() {
        Token name = consume(TokenKind::IDENT, "expected function name");
        consume(TokenKind::LPAREN, "expected '(' after function name");
        std::vector<Param> params;
        if (!check(TokenKind::RPAREN)) {
            do {
                Token param_name = consume(TokenKind::IDENT, "expected parameter name");
                consume(TokenKind::COLON, "expected ':' after parameter name");
                Type param_type = parse_type();
                params.push_back(Param{param_name.lexeme, param_type});
            } while (match({TokenKind::COMMA}));
        }
        consume(TokenKind::RPAREN, "expected ')' after parameters");
        Type ret_type = Type::VOID;
        if (match({TokenKind::ARROW})) {
            ret_type = parse_type();
        }
        auto body = block_statement();
        return std::make_unique<FnDecl>(name.lexeme, std::move(params), ret_type, std::move(body), name.line);
    }

    StmtPtr let_declaration() {
        Token name = consume(TokenKind::IDENT, "expected variable name");
        consume(TokenKind::EQ, "expected '=' in let declaration");
        ExprPtr init = expression();
        consume(TokenKind::SEMICOLON, "expected ';' after variable declaration");
        return std::make_unique<LetStmt>(name.lexeme, std::move(init), name.line);
    }

    StmtPtr statement() {
        if (match({TokenKind::IF})) return if_statement();
        if (match({TokenKind::WHILE})) return while_statement();
        if (match({TokenKind::PRINT})) return print_statement();
        if (match({TokenKind::RETURN})) return return_statement();
        if (check(TokenKind::LBRACE)) return block_statement();

        ExprPtr e = expression();
        consume(TokenKind::SEMICOLON, "expected ';' after expression");
        return std::make_unique<ExprStmt>(std::move(e), e->line);
    }

    std::unique_ptr<BlockStmt> block_statement() {
        Token open = consume(TokenKind::LBRACE, "expected '{'");
        std::vector<StmtPtr> stmts;
        while (!check(TokenKind::RBRACE) && !is_at_end()) {
            stmts.push_back(declaration());
        }
        consume(TokenKind::RBRACE, "expected '}' after block");
        return std::make_unique<BlockStmt>(std::move(stmts), open.line);
    }

    StmtPtr if_statement() {
        Token kw = previous();
        consume(TokenKind::LPAREN, "expected '(' after 'if'");
        ExprPtr cond = expression();
        consume(TokenKind::RPAREN, "expected ')' after if condition");
        StmtPtr then_b = statement();
        StmtPtr else_b = nullptr;
        if (match({TokenKind::ELSE})) {
            else_b = statement();
        }
        return std::make_unique<IfStmt>(std::move(cond), std::move(then_b), std::move(else_b), kw.line);
    }

    StmtPtr while_statement() {
        Token kw = previous();
        consume(TokenKind::LPAREN, "expected '(' after 'while'");
        ExprPtr cond = expression();
        consume(TokenKind::RPAREN, "expected ')' after while condition");
        StmtPtr body = statement();
        return std::make_unique<WhileStmt>(std::move(cond), std::move(body), kw.line);
    }

    StmtPtr print_statement() {
        Token kw = previous();
        ExprPtr e = expression();
        consume(TokenKind::SEMICOLON, "expected ';' after print statement");
        return std::make_unique<PrintStmt>(std::move(e), kw.line);
    }

    StmtPtr return_statement() {
        Token kw = previous();
        ExprPtr val = nullptr;
        if (!check(TokenKind::SEMICOLON)) {
            val = expression();
        }
        consume(TokenKind::SEMICOLON, "expected ';' after return statement");
        return std::make_unique<ReturnStmt>(std::move(val), kw.line);
    }

    ExprPtr expression() { return assignment(); }

    ExprPtr assignment() {
        ExprPtr expr = logical_or();
        if (match({TokenKind::EQ})) {
            Token equals = previous();
            ExprPtr value = assignment();
            if (auto* var = dynamic_cast<VarExpr*>(expr.get())) {
                return std::make_unique<AssignExpr>(var->name, std::move(value), equals.line);
            }
            throw std::runtime_error("Parse error on line " + std::to_string(equals.line) + ": invalid assignment target");
        }
        return expr;
    }

    ExprPtr logical_or() {
        ExprPtr expr = logical_and();
        while (match({TokenKind::OR_OR})) {
            Token op = previous();
            ExprPtr right = logical_and();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.line);
        }
        return expr;
    }

    ExprPtr logical_and() {
        ExprPtr expr = equality();
        while (match({TokenKind::AND_AND})) {
            Token op = previous();
            ExprPtr right = equality();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.line);
        }
        return expr;
    }

    ExprPtr equality() {
        ExprPtr expr = comparison();
        while (match({TokenKind::BANG_EQ, TokenKind::EQ_EQ})) {
            Token op = previous();
            ExprPtr right = comparison();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.line);
        }
        return expr;
    }

    ExprPtr comparison() {
        ExprPtr expr = term();
        while (match({TokenKind::LT, TokenKind::LT_EQ, TokenKind::GT, TokenKind::GT_EQ})) {
            Token op = previous();
            ExprPtr right = term();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.line);
        }
        return expr;
    }

    ExprPtr term() {
        ExprPtr expr = factor();
        while (match({TokenKind::PLUS, TokenKind::MINUS})) {
            Token op = previous();
            ExprPtr right = factor();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.line);
        }
        return expr;
    }

    ExprPtr factor() {
        ExprPtr expr = unary();
        while (match({TokenKind::STAR, TokenKind::SLASH})) {
            Token op = previous();
            ExprPtr right = unary();
            expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.line);
        }
        return expr;
    }

    ExprPtr unary() {
        if (match({TokenKind::BANG, TokenKind::MINUS})) {
            Token op = previous();
            ExprPtr operand = unary();
            return std::make_unique<UnaryExpr>(op.kind, std::move(operand), op.line);
        }
        return primary();
    }

    ExprPtr primary() {
        if (match({TokenKind::NUMBER})) {
            Token t = previous();
            return std::make_unique<NumberExpr>(t.num_value, t.line);
        }
        if (match({TokenKind::STRING})) {
            Token t = previous();
            return std::make_unique<StringExpr>(t.lexeme, t.line);
        }
        if (match({TokenKind::TRUE})) {
            return std::make_unique<BoolExpr>(true, previous().line);
        }
        if (match({TokenKind::FALSE})) {
            return std::make_unique<BoolExpr>(false, previous().line);
        }
        if (match({TokenKind::IDENT})) {
            Token name = previous();
            if (match({TokenKind::LPAREN})) {
                std::vector<ExprPtr> args;
                if (!check(TokenKind::RPAREN)) {
                    do {
                        args.push_back(expression());
                    } while (match({TokenKind::COMMA}));
                }
                consume(TokenKind::RPAREN, "expected ')' after arguments");
                return std::make_unique<CallExpr>(name.lexeme, std::move(args), name.line);
            }
            return std::make_unique<VarExpr>(name.lexeme, name.line);
        }
        if (match({TokenKind::LPAREN})) {
            ExprPtr expr = expression();
            consume(TokenKind::RPAREN, "expected ')' after expression");
            return expr;
        }
        throw std::runtime_error("Parse error on line " + std::to_string(peek().line) +
                                 ": unexpected token '" + peek().lexeme + "'");
    }
};

// ============================================================================
// 4. SEMANTIC ANALYZER (TYPE CHE
struct FnSignature {
    std::vector<Type> params;
    Type return_type;
};

class SemanticAnalyzer {
public:
    void analyze(const std::vector<StmtPtr>& program) {
        // First pass: register all top-level functions
        for (const auto& stmt : program) {
            if (auto* fn = dynamic_cast<FnDecl*>(stmt.get())) {
                if (functions_.count(fn->name)) {
                    throw std::runtime_error("Semantic error on line " + std::to_string(fn->line) +
                                             ": duplicate function '" + fn->name + "'");
                }
                std::vector<Type> p_types;
                for (const auto& p : fn->params) p_types.push_back(p.type);
                functions_[fn->name] = FnSignature{std::move(p_types), fn->return_type};
            }
        }

        // Second pass: analyze statements & bodies
        push_scope();
        for (const auto& stmt : program) {
            check_stmt(*stmt, Type::VOID);
        }
        pop_scope();
    }

private:
    std::vector<std::unordered_map<std::string, Type>> scopes_;
    std::unordered_map<std::string, FnSignature> functions_;

    void push_scope() { scopes_.emplace_back(); }
    void pop_scope() { scopes_.pop_back(); }

    void declare_var(const std::string& name, Type t, int line) {
        auto& current = scopes_.back();
        if (current.count(name)) {
            throw std::runtime_error("Semantic error on line " + std::to_string(line) +
                                     ": variable '" + name + "' already declared in this scope");
        }
        current[name] = t;
    }

    Type lookup_var(const std::string& name, int line) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        throw std::runtime_error("Semantic error on line " + std::to_string(line) +
                                 ": undeclared variable '" + name + "'");
    }

    void check_stmt(Stmt& s, Type current_fn_ret) {
        if (auto* b = dynamic_cast<BlockStmt*>(&s)) {
            push_scope();
            for (const auto& inner : b->statements) check_stmt(*inner, current_fn_ret);
            pop_scope();
        } else if (auto* l = dynamic_cast<LetStmt*>(&s)) {
            Type init_t = check_expr(*l->initializer);
            if (init_t == Type::VOID) {
                throw std::runtime_error("Semantic error on line " + std::to_string(l->line) +
                                         ": cannot bind void expression to '" + l->name + "'");
            }
            declare_var(l->name, init_t, l->line);
        } else if (auto* e = dynamic_cast<ExprStmt*>(&s)) {
            check_expr(*e->expr);
        } else if (auto* p = dynamic_cast<PrintStmt*>(&s)) {
            Type t = check_expr(*p->expr);
            if (t == Type::VOID) {
                throw std::runtime_error("Semantic error on line " + std::to_string(p->line) +
                                         ": cannot print void expression");
            }
        } else if (auto* i = dynamic_cast<IfStmt*>(&s)) {
            if (check_expr(*i->condition) != Type::BOOL) {
                throw std::runtime_error("Semantic error on line " + std::to_string(i->line) +
                                         ": 'if' condition must be bool");
            }
            check_stmt(*i->then_branch, current_fn_ret);
            if (i->else_branch) check_stmt(*i->else_branch, current_fn_ret);
        } else if (auto* w = dynamic_cast<WhileStmt*>(&s)) {
            if (check_expr(*w->condition) != Type::BOOL) {
                throw std::runtime_error("Semantic error on line " + std::to_string(w->line) +
                                         ": 'while' condition must be bool");
            }
            check_stmt(*w->body, current_fn_ret);
        } else if (auto* r = dynamic_cast<ReturnStmt*>(&s)) {
            Type ret_t = r->value ? check_expr(*r->value) : Type::VOID;
            if (ret_t != current_fn_ret) {
                throw std::runtime_error("Semantic error on line " + std::to_string(r->line) +
                                         ": return type '" + type_name(ret_t) +
                                         "' does not match expected '" + type_name(current_fn_ret) + "'");
            }
        } else if (auto* fn = dynamic_cast<FnDecl*>(&s)) {
            push_scope();
            for (const auto& param : fn->params) {
                declare_var(param.name, param.type, fn->line);
            }
            for (const auto& body_stmt : fn->body->statements) {
                check_stmt(*body_stmt, fn->return_type);
            }
            pop_scope();
        }
    }

    Type check_expr(Expr& e) {
        if (auto* n = dynamic_cast<NumberExpr*>(&e)) {
            return n->inferred_type = Type::NUM;
        }
        if (auto* b = dynamic_cast<BoolExpr*>(&e)) {
            return b->inferred_type = Type::BOOL;
        }
        if (auto* s = dynamic_cast<StringExpr*>(&e)) {
            return s->inferred_type = Type::STR;
        }
        if (auto* v = dynamic_cast<VarExpr*>(&e)) {
            return v->inferred_type = lookup_var(v->name, v->line);
        }
        if (auto* a = dynamic_cast<AssignExpr*>(&e)) {
            Type var_t = lookup_var(a->name, a->line);
            Type val_t = check_expr(*a->value);
            if (var_t != val_t) {
                throw std::runtime_error("Semantic error on line " + std::to_string(a->line) +
                                         ": cannot assign '" + type_name(val_t) +
                                         "' to variable '" + a->name + "' of type '" + type_name(var_t) + "'");
            }
            return a->inferred_type = var_t;
        }
        if (auto* u = dynamic_cast<UnaryExpr*>(&e)) {
            Type op_t = check_expr(*u->operand);
            if (u->op == TokenKind::MINUS) {
                if (op_t != Type::NUM) {
                    throw std::runtime_error("Semantic error on line " + std::to_string(u->line) +
                                             ": unary '-' requires num");
                }
                return u->inferred_type = Type::NUM;
            }
            if (u->op == TokenKind::BANG) {
                if (op_t != Type::BOOL) {
                    throw std::runtime_error("Semantic error on line " + std::to_string(u->line) +
                                             ": unary '!' requires bool");
                }
                return u->inferred_type = Type::BOOL;
            }
        }
        if (auto* bin = dynamic_cast<BinaryExpr*>(&e)) {
            Type lt = check_expr(*bin->left);
            Type rt = check_expr(*bin->right);
            switch (bin->op) {
                case TokenKind::PLUS:
                case TokenKind::MINUS:
                case TokenKind::STAR:
                case TokenKind::SLASH:
                    if (lt != Type::NUM || rt != Type::NUM) {
                        throw std::runtime_error("Semantic error on line " + std::to_string(bin->line) +
                                                 ": arithmetic operators require num operands");
                    }
                    return bin->inferred_type = Type::NUM;
                case TokenKind::LT:
                case TokenKind::LT_EQ:
                case TokenKind::GT:
                case TokenKind::GT_EQ:
                    if (lt != Type::NUM || rt != Type::NUM) {
                        throw std::runtime_error("Semantic error on line " + std::to_string(bin->line) +
                                                 ": comparison operators require num operands");
                    }
                    return bin->inferred_type = Type::BOOL;
                case TokenKind::EQ_EQ:
                case TokenKind::BANG_EQ:
                    if (lt != rt || lt == Type::VOID) {
                        throw std::runtime_error("Semantic error on line " + std::to_string(bin->line) +
                                                 ": equality operands must share a non-void type");
                    }
                    return bin->inferred_type = Type::BOOL;
                case TokenKind::AND_AND:
                case TokenKind::OR_OR:
                    if (lt != Type::BOOL || rt != Type::BOOL) {
                        throw std::runtime_error("Semantic error on line " + std::to_string(bin->line) +
                                                 ": logical operators require bool operands");
                    }
                    return bin->inferred_type = Type::BOOL;
                default:
                    break;
            }
        }
        if (auto* call = dynamic_cast<CallExpr*>(&e)) {
            auto it = functions_.find(call->callee);
            if (it == functions_.end()) {
                throw std::runtime_error("Semantic error on line " + std::to_string(call->line) +
                                         ": unknown function '" + call->callee + "'");
            }
            const auto& sig = it->second;
            if (call->args.size() != sig.params.size()) {
                throw std::runtime_error("Semantic error on line " + std::to_string(call->line) +
                                         ": function '" + call->callee + "' expects " +
                                         std::to_string(sig.params.size()) + " arguments, got " +
                                         std::to_string(call->args.size()));
            }
            for (size_t i = 0; i < call->args.size(); ++i) {
                Type arg_t = check_expr(*call->args[i]);
                if (arg_t != sig.params[i]) {
                    throw std::runtime_error("Semantic error on line " + std::to_string(call->line) +
                                             ": argument " + std::to_string(i + 1) + " of '" + call->callee +
                                             "' expected '" + type_name(sig.params[i]) +
                                             "', got '" + type_name(arg_t) + "'");
                }
            }
            return call->inferred_type = sig.return_type;
        }
        throw std::runtime_error("Semantic error: unhandled expression");
    }
};

// ============================================================================
// 5. BYTECODE & VM
// ============================================================================

using Value = std::variant<double, bool, std::string, std::monostate>;

enum class OpCode : uint8_t {
    CONST,
    POP,
    PRINT,
    ADD,
    SUB,
    MUL,
    DIV,
    NEG,
    NOT,
    EQ,
    NEQ,
    LT,
    LTE,
    GT,
    GTE,
    GET_LOCAL,
    SET_LOCAL,
    JUMP,
    JUMP_IF_FALSE,
    CALL,
    RETURN
};

struct Instruction {
    OpCode op;
    int64_t operand = 0;
    std::string str_operand;
};

struct BytecodeFunction {
    std::string name;
    int arity = 0;
    std::vector<Instruction> code;
    std::vector<Value> constants;
};

struct ProgramBytecode {
    std::unordered_map<std::string, BytecodeFunction> functions;
    BytecodeFunction main;
};

class BytecodeCompiler {
public:
    ProgramBytecode compile(const std::vector<StmtPtr>& program) {
        ProgramBytecode out;

        for (const auto& stmt : program) {
            if (auto* fn = dynamic_cast<FnDecl*>(stmt.get())) {
                BytecodeFunction bf;
                bf.name = fn->name;
                bf.arity = static_cast<int>(fn->params.size());
                current_fn_ = &bf;
                locals_.clear();
                scope_depth_ = 0;

                begin_scope();
                for (const auto& p : fn->params) {
                    add_local(p.name);
                }
                for (const auto& s : fn->body->statements) {
                    emit_stmt(*s);
                }
                // Default return if function falls off the end
                emit_const(std::monostate{});
                emit(OpCode::RETURN);
                end_scope();

                out.functions[fn->name] = std::move(bf);
            }
        }

        current_fn_ = &out.main;
        out.main.name = "<main>";
        out.main.arity = 0;
        locals_.clear();
        scope_depth_ = 0;

        begin_scope();
        for (const auto& stmt : program) {
            if (!dynamic_cast<FnDecl*>(stmt.get())) {
                emit_stmt(*stmt);
            }
        }
        emit_const(std::monostate{});
        emit(OpCode::RETURN);
        end_scope();

        return out;
    }

private:
    struct Local {
        std::string name;
        int depth;
    };

    BytecodeFunction* current_fn_ = nullptr;
    std::vector<Local> locals_;
    int scope_depth_ = 0;

    size_t emit(OpCode op, int64_t operand = 0, const std::string& s = "") {
        current_fn_->code.push_back(Instruction{op, operand, s});
        return current_fn_->code.size() - 1;
    }

    void emit_const(Value v) {
        int64_t idx = static_cast<int64_t>(current_fn_->constants.size());
        current_fn_->constants.push_back(std::move(v));
        emit(OpCode::CONST, idx);
    }

    size_t emit_jump(OpCode op) {
        return emit(op, -1);
    }

    void patch_jump(size_t jump_idx) {
        current_fn_->code[jump_idx].operand = static_cast<int64_t>(current_fn_->code.size());
    }

    void begin_scope() { ++scope_depth_; }

    void end_scope() {
        --scope_depth_;
        while (!locals_.empty() && locals_.back().depth > scope_depth_) {
            locals_.pop_back();
            emit(OpCode::POP);
        }
    }

    void add_local(const std::string& name) {
        locals_.push_back(Local{name, scope_depth_});
    }

    int resolve_local(const std::string& name) const {
        for (int i = static_cast<int>(locals_.size()) - 1; i >= 0; --i) {
            if (locals_[i].name == name) return i;
        }
        throw std::runtime_error("Compiler error: unknown local '" + name + "'");
    }

    void emit_stmt(Stmt& s) {
        if (auto* b = dynamic_cast<BlockStmt*>(&s)) {
            begin_scope();
            for (const auto& inner : b->statements) emit_stmt(*inner);
            end_scope();
        } else if (auto* l = dynamic_cast<LetStmt*>(&s)) {
            emit_expr(*l->initializer);
            add_local(l->name);
        } else if (auto* e = dynamic_cast<ExprStmt*>(&s)) {
            emit_expr(*e->expr);
            emit(OpCode::POP);
        } else if (auto* p = dynamic_cast<PrintStmt*>(&s)) {
            emit_expr(*p->expr);
            emit(OpCode::PRINT);
        } else if (auto* i = dynamic_cast<IfStmt*>(&s)) {
            emit_expr(*i->condition);
            size_t then_jump = emit_jump(OpCode::JUMP_IF_FALSE);
            emit(OpCode::POP); // pop condition when true
            emit_stmt(*i->then_branch);
            size_t else_jump = emit_jump(OpCode::JUMP);
            patch_jump(then_jump);
            emit(OpCode::POP); // pop condition when false
            if (i->else_branch) {
                emit_stmt(*i->else_branch);
            }
            patch_jump(else_jump);
        } else if (auto* w = dynamic_cast<WhileStmt*>(&s)) {
            size_t loop_start = current_fn_->code.size();
            emit_expr(*w->condition);
            size_t exit_jump = emit_jump(OpCode::JUMP_IF_FALSE);
            emit(OpCode::POP);
            emit_stmt(*w->body);
            emit(OpCode::JUMP, static_cast<int64_t>(loop_start));
            patch_jump(exit_jump);
            emit(OpCode::POP);
        } else if (auto* r = dynamic_cast<ReturnStmt*>(&s)) {
            if (r->value) {
                emit_expr(*r->value);
            } else {
                emit_const(std::monostate{});
            }
            emit(OpCode::RETURN);
        }
    }

    void emit_expr(Expr& e) {
        if (auto* n = dynamic_cast<NumberExpr*>(&e)) {
            emit_const(n->value);
        } else if (auto* b = dynamic_cast<BoolExpr*>(&e)) {
            emit_const(b->value);
        } else if (auto* s = dynamic_cast<StringExpr*>(&e)) {
            emit_const(s->value);
        } else if (auto* v = dynamic_cast<VarExpr*>(&e)) {
            emit(OpCode::GET_LOCAL, resolve_local(v->name));
        } else if (auto* a = dynamic_cast<AssignExpr*>(&e)) {
            emit_expr(*a->value);
            int slot = resolve_local(a->name);
            emit(OpCode::SET_LOCAL, slot);
        } else if (auto* u = dynamic_cast<UnaryExpr*>(&e)) {
            emit_expr(*u->operand);
            emit(u->op == TokenKind::MINUS ? OpCode::NEG : OpCode::NOT);
        } else if (auto* bin = dynamic_cast<BinaryExpr*>(&e)) {
            if (bin->op == TokenKind::AND_AND) {
                emit_expr(*bin->left);
                size_t end_jump = emit_jump(OpCode::JUMP_IF_FALSE);
                emit(OpCode::POP);
                emit_expr(*bin->right);
                patch_jump(end_jump);
                return;
            }
            if (bin->op == TokenKind::OR_OR) {
                emit_expr(*bin->left);
                size_t false_jump = emit_jump(OpCode::JUMP_IF_FALSE);
                size_t true_jump = emit_jump(OpCode::JUMP);
                patch_jump(false_jump);
                emit(OpCode::POP);
                emit_expr(*bin->right);
                patch_jump(true_jump);
                return;
            }

            emit_expr(*bin->left);
            emit_expr(*bin->right);
            switch (bin->op) {
                case TokenKind::PLUS: emit(OpCode::ADD); break;
                case TokenKind::MINUS: emit(OpCode::SUB); break;
                case TokenKind::STAR: emit(OpCode::MUL); break;
                case TokenKind::SLASH: emit(OpCode::DIV); break;
                case TokenKind::EQ_EQ: emit(OpCode::EQ); break;
                case TokenKind::BANG_EQ: emit(OpCode::NEQ); break;
                case TokenKind::LT: emit(OpCode::LT); break;
                case TokenKind::LT_EQ: emit(OpCode::LTE); break;
                case TokenKind::GT: emit(OpCode::GT); break;
                case TokenKind::GT_EQ: emit(OpCode::GTE); break;
                default: break;
            }
        } else if (auto* call = dynamic_cast<CallExpr*>(&e)) {
            for (const auto& arg : call->args) {
                emit_expr(*arg);
            }
            emit(OpCode::CALL, static_cast<int64_t>(call->args.size()), call->callee);
        }
    }
};

class VirtualMachine {
public:
    void run(const ProgramBytecode& prog) {
        std::vector<Value> stack;
        struct Frame {
            const BytecodeFunction* fn;
            size_t ip;
            size_t base_slot;
        };
        std::vector<Frame> frames;

        frames.push_back(Frame{&prog.main, 0, 0});

        while (!frames.empty()) {
            Frame& f = frames.back();
            if (f.ip >= f.fn->code.size()) {
                frames.pop_back();
                continue;
            }

            const Instruction& ins = f.fn->code[f.ip++];

            switch (ins.op) {
                case OpCode::CONST:
                    stack.push_back(f.fn->constants[static_cast<size_t>(ins.operand)]);
                    break;

                case OpCode::POP:
                    if (!stack.empty()) stack.pop_back();
                    break;

                case OpCode::PRINT: {
                    Value v = pop(stack);
                    print_value(v);
                    break;
                }

                case OpCode::GET_LOCAL: {
                    size_t idx = f.base_slot + static_cast<size_t>(ins.operand);
                    stack.push_back(stack[idx]);
                    break;
                }

                case OpCode::SET_LOCAL: {
                    size_t idx = f.base_slot + static_cast<size_t>(ins.operand);
                    stack[idx] = stack.back(); // leave assigned value on stack as expression value
                    break;
                }

                case OpCode::NEG: {
                    double v = std::get<double>(pop(stack));
                    stack.push_back(-v);
                    break;
                }

                case OpCode::NOT: {
                    bool v = std::get<bool>(pop(stack));
                    stack.push_back(!v);
                    break;
                }

                case OpCode::ADD:
                case OpCode::SUB:
                case OpCode::MUL:
                case OpCode::DIV: {
                    double rhs = std::get<double>(pop(stack));
                    double lhs = std::get<double>(pop(stack));
                    if (ins.op == OpCode::ADD) stack.push_back(lhs + rhs);
                    else if (ins.op == OpCode::SUB) stack.push_back(lhs - rhs);
                    else if (ins.op == OpCode::MUL) stack.push_back(lhs * rhs);
                    else {
                        if (rhs == 0.0) throw std::runtime_error("Runtime error: division by zero");
                        stack.push_back(lhs / rhs);
                    }
                    break;
                }

                case OpCode::EQ:
                case OpCode::NEQ: {
                    bool equal = values_equal(pop(stack), pop(stack));
                    stack.push_back(ins.op == OpCode::EQ ? equal : !equal);
                    break;
                }

                case OpCode::LT:
                case OpCode::LTE:
                case OpCode::GT:
                case OpCode::GTE: {
                    double rhs = std::get<double>(pop(stack));
                    double lhs = std::get<double>(pop(stack));
                    if (ins.op == OpCode::LT) stack.push_back(lhs < rhs);
                    else if (ins.op == OpCode::LTE) stack.push_back(lhs <= rhs);
                    else if (ins.op == OpCode::GT) stack.push_back(lhs > rhs);
                    else stack.push_back(lhs >= rhs);
                    break;
                }

                case OpCode::JUMP:
                    f.ip = static_cast<size_t>(ins.operand);
                    break;

                case OpCode::JUMP_IF_FALSE:
                    if (!std::get<bool>(stack.back())) {
                        f.ip = static_cast<size_t>(ins.operand);
                    }
                    break;

                case OpCode::CALL: {
                    auto fn = prog.functions.find(ins.str_operand);
                    if (fn == prog.functions.end()) {
                        throw std::runtime_error("Runtime error: unknown function '" + ins.str_operand + "'");
                    }
                    size_t base_slot = stack.size() - static_cast<size_t>(ins.operand);
                    frames.push_back(Frame{&fn->second, 0, base_slot});
                    break;
                }

                case OpCode::RETURN: {
                    Value result = pop(stack);
                    size_t base_slot = f.base_slot;
                    frames.pop_back();
                    stack.resize(base_slot);
                    if (!frames.empty()) stack.push_back(std::move(result));
                    break;
                }
            }
        }
    }

private:
    static Value pop(std::vector<Value>& stack) {
        if (stack.empty()) throw std::runtime_error("Runtime error: stack underflow");
        Value value = std::move(stack.back());
        stack.pop_back();
        return value;
    }

    static bool values_equal(const Value& a, const Value& b) {
        if (a.index() != b.index()) return false;
        if (std::holds_alternative<double>(a)) return std::get<double>(a) == std::get<double>(b);
        if (std::holds_alternative<bool>(a)) return std::get<bool>(a) == std::get<bool>(b);
        if (std::holds_alternative<std::string>(a)) return std::get<std::string>(a) == std::get<std::string>(b);
        return true;
    }

    static void print_value(const Value& v) {
        if (std::holds_alternative<double>(v)) {
            double d = std::get<double>(v);
            if (std::floor(d) == d && std::isfinite(d)) {
                std::cout << static_cast<long long>(d) << "\n";
            } else {
                std::cout << d << "\n";
            }
        } else if (std::holds_alternative<bool>(v)) {
            std::cout << (std::get<bool>(v) ? "true" : "false") << "\n";
        } else if (std::holds_alternative<std::string>(v)) {
            std::cout << std::get<std::string>(v) << "\n";
        } else {
            std::cout << "void\n";
        }
    }
};

// ============================================================================
// 6. DRIVER
// ============================================================================

int main() {
    try {
        const std::string source{
            std::istreambuf_iterator<char>(std::cin),
            std::istreambuf_iterator<char>()
        };
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        Parser parser(tokens);
        std::vector<StmtPtr> ast = parser.parse_program();
        SemanticAnalyzer analyzer;
        analyzer.analyze(ast);
        BytecodeCompiler compiler;
        ProgramBytecode bytecode = compiler.compile(ast);
        VirtualMachine vm;
        vm.run(bytecode);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}