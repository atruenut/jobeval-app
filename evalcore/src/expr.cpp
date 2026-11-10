#include "evalcore/expr.hpp"

#include <cctype>

namespace jobeval {

namespace {

struct FnSpec {
    const char* name;
    int min_args;
    int max_args;  // -1 = unlimited
};

const FnSpec kFunctions[] = {
    {"min", 2, -1},       {"max", 2, -1},        {"clamp", 3, 3},      {"abs", 1, 1},
    {"floor", 1, 1},      {"ceil", 1, 1},        {"round", 1, 3},      {"if", 3, 3},
    {"bracket", 2, 2},    {"tier_lookup", 2, 2}, {"phase_down", 2, 2}, {"phase_out", 4, 4},
};

const FnSpec* find_fn(const std::string& name) {
    for (const auto& f : kFunctions) {
        if (name == f.name) return &f;
    }
    return nullptr;
}

enum class Tok { End, Num, Str, Ident, Param, Slot, Op, LParen, RParen, Comma };

struct Token {
    Tok kind = Tok::End;
    std::string text;
    size_t pos = 0;
};

class Lexer {
public:
    explicit Lexer(const std::string& s) : s_(s) {}

    std::vector<Token> run() {
        std::vector<Token> out;
        while (true) {
            skip_ws();
            Token t;
            t.pos = i_;
            if (i_ >= s_.size()) {
                t.kind = Tok::End;
                out.push_back(t);
                return out;
            }
            char c = s_[i_];
            if (std::isdigit(static_cast<unsigned char>(c))) {
                size_t start = i_;
                while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
                if (i_ + 1 < s_.size() && s_[i_] == '.' && std::isdigit(static_cast<unsigned char>(s_[i_ + 1]))) {
                    ++i_;
                    while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) ++i_;
                }
                t.kind = Tok::Num;
                t.text = s_.substr(start, i_ - start);
            } else if (is_ident_start(c)) {
                t.kind = Tok::Ident;
                t.text = ident();
            } else if (c == '$' || c == '@') {
                ++i_;
                if (i_ >= s_.size() || !is_ident_start(s_[i_])) {
                    throw ParseError(std::string("expected a name after '") + c + "'", t.pos);
                }
                t.kind = c == '$' ? Tok::Param : Tok::Slot;
                t.text = ident();
            } else if (c == '"' || c == '\'') {
                char q = c;
                ++i_;
                size_t start = i_;
                while (i_ < s_.size() && s_[i_] != q) ++i_;
                if (i_ >= s_.size()) throw ParseError("unterminated string", t.pos);
                t.kind = Tok::Str;
                t.text = s_.substr(start, i_ - start);
                ++i_;
            } else if (c == '(') {
                t.kind = Tok::LParen;
                ++i_;
            } else if (c == ')') {
                t.kind = Tok::RParen;
                ++i_;
            } else if (c == ',') {
                t.kind = Tok::Comma;
                ++i_;
            } else if (c == '+' || c == '-' || c == '*' || c == '/') {
                t.kind = Tok::Op;
                t.text = std::string(1, c);
                ++i_;
            } else if (c == '<' || c == '>' || c == '=' || c == '!') {
                t.kind = Tok::Op;
                t.text = std::string(1, c);
                ++i_;
                if (i_ < s_.size() && s_[i_] == '=') {
                    t.text.push_back('=');
                    ++i_;
                } else if (c == '=' || c == '!') {
                    throw ParseError(std::string("unexpected '") + c + "' (use == or !=)", t.pos);
                }
            } else {
                throw ParseError(std::string("unexpected character '") + c + "'", t.pos);
            }
            out.push_back(std::move(t));
        }
    }

private:
    const std::string& s_;
    size_t i_ = 0;

    static bool is_ident_start(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
    static bool is_ident_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

    void skip_ws() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) ++i_;
    }

    // dotted name: seg(.seg)*
    std::string ident() {
        size_t start = i_;
        while (true) {
            while (i_ < s_.size() && is_ident_char(s_[i_])) ++i_;
            if (i_ + 1 < s_.size() && s_[i_] == '.' && is_ident_start(s_[i_ + 1])) {
                ++i_;
                continue;
            }
            break;
        }
        return s_.substr(start, i_ - start);
    }
};

class Parser {
public:
    explicit Parser(std::vector<Token> toks) : t_(std::move(toks)) {}

    std::unique_ptr<Node> run() {
        auto n = parse_or();
        if (cur().kind != Tok::End) throw ParseError("unexpected '" + describe(cur()) + "'", cur().pos);
        return n;
    }

private:
    std::vector<Token> t_;
    size_t i_ = 0;

    const Token& cur() const { return t_[i_]; }
    void next() {
        if (i_ + 1 < t_.size()) ++i_;
    }
    static std::string describe(const Token& t) {
        switch (t.kind) {
            case Tok::End: return "end of expression";
            case Tok::LParen: return "(";
            case Tok::RParen: return ")";
            case Tok::Comma: return ",";
            default: return t.text;
        }
    }
    bool is_kw(const char* kw) const { return cur().kind == Tok::Ident && cur().text == kw; }

    static std::unique_ptr<Node> make(Node::Kind k, size_t pos) {
        auto n = std::make_unique<Node>();
        n->kind = k;
        n->pos = pos;
        return n;
    }
    static std::unique_ptr<Node> binary(const std::string& op, std::unique_ptr<Node> a, std::unique_ptr<Node> b,
                                        size_t pos) {
        auto n = make(Node::Kind::Bin, pos);
        n->name = op;
        n->kids.push_back(std::move(a));
        n->kids.push_back(std::move(b));
        return n;
    }

    std::unique_ptr<Node> parse_or() {
        auto left = parse_and();
        while (is_kw("or")) {
            size_t p = cur().pos;
            next();
            left = binary("or", std::move(left), parse_and(), p);
        }
        return left;
    }
    std::unique_ptr<Node> parse_and() {
        auto left = parse_not();
        while (is_kw("and")) {
            size_t p = cur().pos;
            next();
            left = binary("and", std::move(left), parse_not(), p);
        }
        return left;
    }
    std::unique_ptr<Node> parse_not() {
        if (is_kw("not")) {
            size_t p = cur().pos;
            next();
            auto n = make(Node::Kind::Not, p);
            n->kids.push_back(parse_not());
            return n;
        }
        return parse_cmp();
    }
    std::unique_ptr<Node> parse_cmp() {
        auto left = parse_sum();
        if (cur().kind == Tok::Op) {
            const std::string& op = cur().text;
            if (op == "<" || op == "<=" || op == ">" || op == ">=" || op == "==" || op == "!=") {
                std::string o = op;
                size_t p = cur().pos;
                next();
                left = binary(o, std::move(left), parse_sum(), p);
            }
        }
        return left;
    }
    std::unique_ptr<Node> parse_sum() {
        auto left = parse_prod();
        while (cur().kind == Tok::Op && (cur().text == "+" || cur().text == "-")) {
            std::string o = cur().text;
            size_t p = cur().pos;
            next();
            left = binary(o, std::move(left), parse_prod(), p);
        }
        return left;
    }
    std::unique_ptr<Node> parse_prod() {
        auto left = parse_unary();
        while (cur().kind == Tok::Op && (cur().text == "*" || cur().text == "/")) {
            std::string o = cur().text;
            size_t p = cur().pos;
            next();
            left = binary(o, std::move(left), parse_unary(), p);
        }
        return left;
    }
    std::unique_ptr<Node> parse_unary() {
        if (cur().kind == Tok::Op && cur().text == "-") {
            size_t p = cur().pos;
            next();
            auto n = make(Node::Kind::Neg, p);
            n->kids.push_back(parse_unary());
            return n;
        }
        return parse_primary();
    }
    std::unique_ptr<Node> parse_primary() {
        const Token tok = cur();
        switch (tok.kind) {
            case Tok::Num: {
                next();
                auto n = make(Node::Kind::Num, tok.pos);
                std::string err;
                if (!Decimal::try_parse(tok.text, n->num, &err)) throw ParseError(err, tok.pos);
                return n;
            }
            case Tok::Str: {
                next();
                auto n = make(Node::Kind::Str, tok.pos);
                n->str = tok.text;
                return n;
            }
            case Tok::Param: {
                next();
                auto n = make(Node::Kind::Param, tok.pos);
                n->name = tok.text;
                return n;
            }
            case Tok::Slot: {
                next();
                auto n = make(Node::Kind::Slot, tok.pos);
                n->name = tok.text;
                return n;
            }
            case Tok::LParen: {
                next();
                auto n = parse_or();
                if (cur().kind != Tok::RParen) throw ParseError("expected ')'", cur().pos);
                next();
                return n;
            }
            case Tok::Ident: {
                if (tok.text == "true" || tok.text == "false") {
                    next();
                    auto n = make(Node::Kind::Bool, tok.pos);
                    n->b = tok.text == "true";
                    return n;
                }
                next();
                if (cur().kind == Tok::LParen) return parse_call(tok);
                auto n = make(Node::Kind::Ref, tok.pos);
                n->name = tok.text;
                return n;
            }
            default:
                throw ParseError("unexpected '" + describe(tok) + "'", tok.pos);
        }
    }
    std::unique_ptr<Node> parse_call(const Token& name_tok) {
        const FnSpec* spec = find_fn(name_tok.text);
        if (!spec) throw ParseError("unknown function '" + name_tok.text + "'", name_tok.pos);
        next();  // (
        auto n = make(Node::Kind::Call, name_tok.pos);
        n->name = name_tok.text;
        if (cur().kind != Tok::RParen) {
            while (true) {
                n->kids.push_back(parse_or());
                if (cur().kind == Tok::Comma) {
                    next();
                    continue;
                }
                break;
            }
        }
        if (cur().kind != Tok::RParen) throw ParseError("expected ')' after arguments", cur().pos);
        next();
        const int argc = static_cast<int>(n->kids.size());
        if (argc < spec->min_args || (spec->max_args >= 0 && argc > spec->max_args)) {
            std::string want = spec->max_args < 0
                                   ? "at least " + std::to_string(spec->min_args)
                                   : (spec->min_args == spec->max_args
                                          ? std::to_string(spec->min_args)
                                          : std::to_string(spec->min_args) + " to " + std::to_string(spec->max_args));
            throw ParseError(name_tok.text + "() takes " + want + " argument(s), got " + std::to_string(argc),
                             name_tok.pos);
        }
        return n;
    }
};

}  // namespace

std::shared_ptr<const Expr> parse_expr(const std::string& source) {
    Lexer lex(source);
    Parser p(lex.run());
    auto e = std::make_shared<Expr>();
    e->source = source;
    e->root = p.run();
    return e;
}

void collect_refs(const Node& node, Refs& out) {
    switch (node.kind) {
        case Node::Kind::Ref: out.rules.insert(node.name); break;
        case Node::Kind::Param: out.params.insert(node.name); break;
        case Node::Kind::Slot: out.slots.insert(node.name); break;
        default: break;
    }
    for (const auto& k : node.kids) collect_refs(*k, out);
}

const std::vector<std::string>& builtin_function_names() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (const auto& f : kFunctions) v.emplace_back(f.name);
        return v;
    }();
    return names;
}

}  // namespace jobeval
