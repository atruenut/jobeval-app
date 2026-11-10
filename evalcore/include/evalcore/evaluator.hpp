#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "evalcore/pack.hpp"
#include "evalcore/value.hpp"

namespace jobeval {

// One evaluated rule, recorded in evaluation order. This is what powers
// "click any number to see why".
struct TraceEntry {
    std::string id;
    std::string label;
    std::string pack;
    std::string expr;
    Value value;
    std::vector<std::pair<std::string, std::string>> used;  // name -> described value
};

struct EvalInputs {
    std::map<std::string, Value> values;
    std::string date;  // YYYY-MM-DD; selects effective-dated parameter values
};

class Evaluator {
public:
    Evaluator(std::shared_ptr<const Bundle> bundle, EvalInputs inputs);

    // Evaluates a rule (memoized). Throws EvalError.
    const Value& rule(const std::string& id);
    // Sum of the rules listed in an output slot.
    Decimal slot(const std::string& name);
    // Evaluates an ad-hoc expression against the bundle (used by tests/tools).
    Value eval_text(const std::string& source);

    const std::vector<TraceEntry>& trace() const { return trace_; }
    const Bundle& bundle() const { return *bundle_; }

private:
    Value eval(const Node& n);
    Value call(const Node& n);
    Value param(const std::string& path);
    Value input_or_rule(const std::string& name);

    std::shared_ptr<const Bundle> bundle_;
    EvalInputs inputs_;
    std::map<std::string, Value> memo_;
    std::vector<std::string> active_;  // rules currently being evaluated
    std::vector<TraceEntry> trace_;
};

Json trace_to_json(const std::vector<TraceEntry>& trace);

// Pure schedule functions (exposed for unit tests).
Decimal fn_bracket(const Decimal& base, const Json& table);
Decimal fn_tier_lookup(const Decimal& key, const Json& table);
Decimal fn_phase_down(const Decimal& x, const Json& params);
Decimal fn_phase_out(const Decimal& credit, const Decimal& x, const Decimal& start, const Decimal& rate);

}  // namespace jobeval
