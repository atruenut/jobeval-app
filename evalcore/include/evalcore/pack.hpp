// Jurisdiction packs: data-only descriptions of tax, contribution, benefit and
// regulatory rules. The engine knows nothing about any country; everything it
// computes comes from a Bundle merged from a pack and its `extends` chain.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "evalcore/decimal.hpp"
#include "evalcore/expr.hpp"
#include "evalcore/json.hpp"

namespace jobeval {

constexpr int kEngineMajor = 1;
constexpr int kEngineMinor = 0;
constexpr int kPackSchema = 1;

enum class Severity { Error, Warning };

struct Diagnostic {
    Severity severity;
    std::string where;
    std::string message;
};

class Diagnostics {
public:
    void error(const std::string& where, const std::string& msg) { items_.push_back({Severity::Error, where, msg}); }
    void warn(const std::string& where, const std::string& msg) { items_.push_back({Severity::Warning, where, msg}); }
    bool has_errors() const;
    size_t error_count() const;
    size_t warning_count() const;
    const std::vector<Diagnostic>& items() const { return items_; }
    std::string format() const;  // one line per item
    void clear() { items_.clear(); }

private:
    std::vector<Diagnostic> items_;
};

struct SourceDef {
    std::string id;
    std::string authority;  // official | official_secondary | third_party
    std::string title;
    std::string url;
    std::string retrieved;
};

struct EffectiveValue {
    std::string from;  // YYYY-MM-DD, inclusive
    Json value;
    std::string source;
};

struct ParamDef {
    std::string id;
    std::string pack;  // pack that defined it
    std::string source;
    bool dated = false;                    // true: `history` is used instead of `value`
    Json value;                            // static value
    std::vector<EffectiveValue> history;   // ascending by `from`
};

struct InputDef {
    std::string name;
    std::string pack;
    std::string type;
    std::string label;
    bool has_default = false;
    Decimal default_value;
};

struct Rule {
    std::string id;
    std::string pack;
    std::string label;
    std::string note;
    bool is_public = false;  // exposed to the UI and tests; exempt from the unused-rule warning
    std::shared_ptr<const Expr> expr;
};

struct Pack {
    std::string id;
    std::string kind;  // country | subregion | locality
    std::string extends;
    std::string country;
    std::string subregion;
    std::string valid_from;
    std::string valid_to;
    std::string tier;  // full | partial | estimate
    std::string currency;
    int revision = 0;
    std::map<std::string, std::string> labels;
    std::map<std::string, SourceDef> sources;
    std::vector<ParamDef> params;
    std::vector<InputDef> inputs;
    std::vector<Rule> rules;
    std::vector<std::pair<std::string, std::vector<std::string>>> outputs;
    std::vector<std::pair<std::string, std::vector<std::string>>> appends;
    std::vector<std::pair<std::string, std::string>> statutory;  // name -> expression text
    std::vector<std::string> replace;
    std::vector<std::string> not_modelled;
    Json programs;  // carried through untouched until program evaluation lands
};

// Structural and per-pack checks. Returns false if the document is unusable.
bool load_pack(const Json& doc, const std::string& origin, Pack& out, Diagnostics& diag);

struct Bundle {
    std::vector<std::string> chain;  // pack ids, parent first
    std::map<std::string, ParamDef> params;
    std::map<std::string, Rule> rules;
    std::vector<std::string> rule_order;
    std::map<std::string, std::vector<std::string>> slots;
    std::map<std::string, InputDef> inputs;
    std::map<std::string, SourceDef> sources;
    std::string currency;
    std::string tier;  // weakest tier in the chain
    std::string valid_from;  // intersection of the chain's validity windows
    std::string valid_to;
    std::vector<std::string> not_modelled;
};

class PackSet {
public:
    // Loads and registers a compiled pack document. Returns false on errors.
    bool add(const Json& doc, const std::string& origin, Diagnostics& diag);
    const Pack* find(const std::string& id) const;
    std::vector<std::string> ids() const;

    // Merges `id` with its extends chain and validates the result (reference
    // resolution, collisions, cycles, tier rules). Null on errors.
    std::shared_ptr<const Bundle> bundle_for(const std::string& id, Diagnostics& diag) const;

private:
    std::map<std::string, Pack> packs_;
};

// Resolves the parameter id and field path for a `$a.b.c` reference: longest
// prefix that names a parameter wins; the remainder indexes into its value.
bool split_param_path(const std::map<std::string, ParamDef>& params, const std::string& path,
                      std::string& param_id, std::vector<std::string>& fields);

// The value of a parameter on a date (YYYY-MM-DD). Throws EvalError if no
// dated value is in effect.
const Json& param_value_on(const ParamDef& p, const std::string& date);

bool is_iso_date(const std::string& s);

// True if this engine can run a pack declaring `constraint` (">=MAJOR.MINOR"); otherwise `why`
// explains, in words suitable for showing to the user.
bool engine_supports(const std::string& constraint, std::string& why);

}  // namespace jobeval
