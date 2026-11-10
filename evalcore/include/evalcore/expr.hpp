// The rule language.
//
//   expr    := or
//   or      := and ("or" and)*
//   and     := not ("and" not)*
//   not     := "not" not | cmp
//   cmp     := sum (("<"|"<="|">"|">="|"=="|"!=") sum)?
//   sum     := prod (("+"|"-") prod)*
//   prod    := unary (("*"|"/") unary)*
//   unary   := "-" unary | primary
//   primary := number | "string" | true | false | ref | $param.path | @slot
//            | name "(" args ")" | "(" expr ")"
//
// `ref` is a dotted name resolving to a rule or an input ("cpp.base",
// "gross_annual"). `$a.b.c` names a pack parameter (longest matching
// parameter id, remainder is a field path). `@slot` is the sum of the rules
// listed in an output slot across every loaded layer.
//
// The language is total: no loops, recursion, user functions or I/O.
#pragma once

#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "evalcore/decimal.hpp"

namespace jobeval {

struct ParseError : std::runtime_error {
    ParseError(const std::string& msg, size_t position)
        : std::runtime_error(msg + " (at column " + std::to_string(position + 1) + ")"), pos(position) {}
    size_t pos;
};

struct Node {
    enum class Kind { Num, Str, Bool, Ref, Param, Slot, Neg, Not, Bin, Call };
    Kind kind = Kind::Num;
    std::string name;  // ref / param / slot path, operator, or function name
    Decimal num;
    bool b = false;
    std::string str;
    std::vector<std::unique_ptr<Node>> kids;
    size_t pos = 0;
};

struct Expr {
    std::string source;
    std::unique_ptr<Node> root;
};

// Throws ParseError on syntax errors, unknown functions, and wrong arity.
std::shared_ptr<const Expr> parse_expr(const std::string& source);

struct Refs {
    std::set<std::string> rules;   // bare dotted names (rules or inputs)
    std::set<std::string> params;  // $paths
    std::set<std::string> slots;   // @slots
};
void collect_refs(const Node& node, Refs& out);

// Names of the built-in functions (for docs and tooling).
const std::vector<std::string>& builtin_function_names();

}  // namespace jobeval
