#include "evalcore/evaluator.hpp"

#include <algorithm>

namespace jobeval {

namespace {

Decimal num_field(const Json& obj, const char* key, const std::string& ctx) {
    const Json* v = obj.find(key);
    if (!v || !v->is_number()) throw EvalError(ctx + ": missing numeric field '" + key + "'");
    return Decimal::parse(v->str());
}

// A schedule table is a list of { <limit_key>: N, <val_key>: X } rows in
// ascending limit order; the last row omits the limit and covers the rest.
struct Row {
    bool open;
    Decimal limit;
    Decimal val;
};

std::vector<Row> read_table(const Json& table, const char* val_key, const std::string& fn) {
    if (!table.is_array() || table.items().empty()) throw EvalError(fn + "(): table must be a non-empty list");
    std::vector<Row> rows;
    const size_t n = table.items().size();
    for (size_t i = 0; i < n; ++i) {
        const Json& r = table.at(i);
        if (!r.is_object()) throw EvalError(fn + "(): each table row must be an object");
        Row row;
        const Json* up = r.find("up_to");
        row.open = up == nullptr;
        if (row.open && i + 1 != n) throw EvalError(fn + "(): only the last row may omit 'up_to'");
        if (!row.open) {
            if (!up->is_number()) throw EvalError(fn + "(): 'up_to' must be a number");
            row.limit = Decimal::parse(up->str());
            if (!rows.empty() && !rows.back().open && row.limit <= rows.back().limit) {
                throw EvalError(fn + "(): 'up_to' values must be strictly ascending");
            }
        }
        row.val = num_field(r, val_key, fn + "()");
        rows.push_back(row);
    }
    if (!rows.back().open) throw EvalError(fn + "(): the last row must omit 'up_to' (it covers everything above)");
    return rows;
}

}  // namespace

Decimal fn_bracket(const Decimal& base, const Json& table) {
    const auto rows = read_table(table, "rate", "bracket");
    if (base <= Decimal()) return Decimal();
    Decimal tax;
    Decimal lower;
    for (const auto& row : rows) {
        if (row.open || base <= row.limit) {
            tax = tax + (base - lower) * row.val;
            break;
        }
        tax = tax + (row.limit - lower) * row.val;
        lower = row.limit;
    }
    return tax;
}

Decimal fn_tier_lookup(const Decimal& key, const Json& table) {
    const auto rows = read_table(table, "value", "tier_lookup");
    for (const auto& row : rows) {
        if (row.open || key <= row.limit) return row.val;
    }
    return rows.back().val;
}

Decimal fn_phase_down(const Decimal& x, const Json& params) {
    const Decimal hi = num_field(params, "max", "phase_down()");
    const Decimal lo = num_field(params, "min", "phase_down()");
    const Decimal from = num_field(params, "from", "phase_down()");
    const Decimal to = num_field(params, "to", "phase_down()");
    if (to <= from) throw EvalError("phase_down(): 'to' must be greater than 'from'");
    if (x <= from) return hi;
    if (x >= to) return lo;
    return hi - (hi - lo) * ((x - from) / (to - from));
}

Decimal fn_phase_out(const Decimal& credit, const Decimal& x, const Decimal& start, const Decimal& rate) {
    const Decimal over = max(Decimal(), x - start);
    return max(Decimal(), credit - rate * over);
}

Evaluator::Evaluator(std::shared_ptr<const Bundle> bundle, EvalInputs inputs)
    : bundle_(std::move(bundle)), inputs_(std::move(inputs)) {}

Value Evaluator::param(const std::string& path) {
    std::string pid;
    std::vector<std::string> fields;
    if (!split_param_path(bundle_->params, path, pid, fields)) throw EvalError("unknown parameter '$" + path + "'");
    const ParamDef& pd = bundle_->params.at(pid);
    if (pd.dated && inputs_.date.empty()) {
        throw EvalError("parameter '" + pid + "' is date-dependent but no 'as_of' date was supplied");
    }
    const Json* cur = &param_value_on(pd, inputs_.date);
    for (const auto& f : fields) {
        if (!cur->is_object() || !(cur = cur->find(f))) throw EvalError("parameter '$" + path + "' has no field '" + f + "'");
    }
    return Value::from_json(*cur);
}

Value Evaluator::input_or_rule(const std::string& name) {
    if (bundle_->rules.count(name)) return rule(name);
    auto it = inputs_.values.find(name);
    if (it != inputs_.values.end()) return it->second;
    auto def = bundle_->inputs.find(name);
    if (def != bundle_->inputs.end() && def->second.has_default) return Value::num(def->second.default_value);
    throw EvalError("input '" + name + "' was not provided and has no default");
}

const Value& Evaluator::rule(const std::string& id) {
    auto hit = memo_.find(id);
    if (hit != memo_.end()) return hit->second;
    auto it = bundle_->rules.find(id);
    if (it == bundle_->rules.end()) throw EvalError("unknown rule '" + id + "'");
    if (std::find(active_.begin(), active_.end(), id) != active_.end()) {
        throw EvalError("rule '" + id + "' depends on itself");
    }
    if (active_.size() > 200) throw EvalError("rule dependency chain too deep");
    const Rule& r = it->second;
    active_.push_back(id);
    Value v;
    try {
        v = eval(*r.expr->root);
    } catch (const EvalError& e) {
        active_.pop_back();
        throw EvalError("rule '" + id + "': " + e.what());
    }
    active_.pop_back();

    TraceEntry t;
    t.id = id;
    t.label = r.label;
    t.pack = r.pack;
    t.expr = r.expr->source;
    t.value = v;
    Refs refs;
    collect_refs(*r.expr->root, refs);
    for (const auto& name : refs.rules) {
        try {
            t.used.emplace_back(name, input_or_rule(name).describe());
        } catch (const EvalError&) {
        }
    }
    for (const auto& path : refs.params) {
        try {
            t.used.emplace_back("$" + path, param(path).describe());
        } catch (const EvalError&) {
        }
    }
    for (const auto& s : refs.slots) {
        try {
            t.used.emplace_back("@" + s, slot(s).to_string());
        } catch (const EvalError&) {
        }
    }
    trace_.push_back(std::move(t));
    return memo_.emplace(id, std::move(v)).first->second;
}

Decimal Evaluator::slot(const std::string& name) {
    auto it = bundle_->slots.find(name);
    if (it == bundle_->slots.end()) throw EvalError("unknown output slot '" + name + "'");
    Decimal total;
    for (const auto& rid : it->second) total = total + rule(rid).as_num();
    return total;
}

Value Evaluator::eval_text(const std::string& source) {
    auto e = parse_expr(source);
    return eval(*e->root);
}

Value Evaluator::eval(const Node& n) {
    switch (n.kind) {
        case Node::Kind::Num: return Value::num(n.num);
        case Node::Kind::Str: return Value::str(n.str);
        case Node::Kind::Bool: return Value::boolean(n.b);
        case Node::Kind::Ref: return input_or_rule(n.name);
        case Node::Kind::Param: return param(n.name);
        case Node::Kind::Slot: return Value::num(slot(n.name));
        case Node::Kind::Neg: return Value::num(-eval(*n.kids[0]).as_num());
        case Node::Kind::Not: return Value::boolean(!eval(*n.kids[0]).as_bool());
        case Node::Kind::Call: return call(n);
        case Node::Kind::Bin: {
            const std::string& op = n.name;
            if (op == "and") return Value::boolean(eval(*n.kids[0]).as_bool() && eval(*n.kids[1]).as_bool());
            if (op == "or") return Value::boolean(eval(*n.kids[0]).as_bool() || eval(*n.kids[1]).as_bool());
            const Value a = eval(*n.kids[0]);
            const Value b = eval(*n.kids[1]);
            if (op == "==") return Value::boolean(a == b);
            if (op == "!=") return Value::boolean(!(a == b));
            const Decimal& x = a.as_num();
            const Decimal& y = b.as_num();
            if (op == "+") return Value::num(x + y);
            if (op == "-") return Value::num(x - y);
            if (op == "*") return Value::num(x * y);
            if (op == "/") return Value::num(x / y);
            if (op == "<") return Value::boolean(x < y);
            if (op == "<=") return Value::boolean(x <= y);
            if (op == ">") return Value::boolean(x > y);
            if (op == ">=") return Value::boolean(x >= y);
            throw EvalError("internal: unknown operator " + op);
        }
    }
    throw EvalError("internal: unknown node");
}

Value Evaluator::call(const Node& n) {
    const std::string& f = n.name;
    auto num = [&](size_t i) { return eval(*n.kids[i]).as_num(); };

    if (f == "if") {  // lazy: only the chosen branch is evaluated
        return eval(*n.kids[0]).as_bool() ? eval(*n.kids[1]) : eval(*n.kids[2]);
    }
    if (f == "min" || f == "max") {
        Decimal acc = num(0);
        for (size_t i = 1; i < n.kids.size(); ++i) {
            const Decimal v = num(i);
            acc = f == "min" ? min(acc, v) : max(acc, v);
        }
        return Value::num(acc);
    }
    if (f == "clamp") {
        const Decimal x = num(0), lo = num(1), hi = num(2);
        if (hi < lo) throw EvalError("clamp(): upper bound is below lower bound");
        return Value::num(min(max(x, lo), hi));
    }
    if (f == "abs") return Value::num(num(0).abs());
    if (f == "floor") return Value::num(num(0).floor());
    if (f == "ceil") return Value::num(num(0).ceil());
    if (f == "round") {
        const Decimal x = num(0);
        const Decimal step = n.kids.size() > 1 ? num(1) : Decimal::parse("0.01");
        RoundMode mode = RoundMode::HalfUp;
        if (n.kids.size() > 2) {
            const std::string name = eval(*n.kids[2]).as_str();
            if (!parse_round_mode(name, mode)) throw EvalError("round(): unknown mode '" + name + "'");
        }
        return Value::num(x.round_to(step, mode));
    }
    if (f == "bracket") return Value::num(fn_bracket(num(0), eval(*n.kids[1]).as_data()));
    if (f == "tier_lookup") return Value::num(fn_tier_lookup(num(0), eval(*n.kids[1]).as_data()));
    if (f == "phase_down") return Value::num(fn_phase_down(num(0), eval(*n.kids[1]).as_data()));
    if (f == "phase_out") return Value::num(fn_phase_out(num(0), num(1), num(2), num(3)));
    throw EvalError("internal: unimplemented function " + f);
}

Json trace_to_json(const std::vector<TraceEntry>& trace) {
    Json arr = Json::array();
    for (const auto& t : trace) {
        Json o = Json::object();
        o.set("id", Json::string(t.id));
        if (!t.label.empty()) o.set("label", Json::string(t.label));
        o.set("pack", Json::string(t.pack));
        o.set("expr", Json::string(t.expr));
        o.set("value", Json::string(t.value.describe()));
        Json used = Json::object();
        for (const auto& u : t.used) used.set(u.first, Json::string(u.second));
        o.set("used", used);
        arr.push(o);
    }
    return arr;
}

}  // namespace jobeval
