#include "evalcore/golden.hpp"

#include "evalcore/evaluator.hpp"

namespace jobeval {

int SuiteResult::passed() const {
    int n = 0;
    for (const auto& c : cases) n += (!c.skipped && c.passed) ? 1 : 0;
    return n;
}
int SuiteResult::failed() const {
    int n = 0;
    for (const auto& c : cases) n += (!c.skipped && !c.passed) ? 1 : 0;
    return n;
}
int SuiteResult::skipped() const {
    int n = 0;
    for (const auto& c : cases) n += c.skipped ? 1 : 0;
    return n;
}

SuiteResult run_suite(const PackSet& packs, const Json& suite, Diagnostics& diag) {
    SuiteResult res;
    const Json* name = suite.find("suite");
    const Json* pack = suite.find("pack");
    res.suite = name && name->is_string() ? name->str() : "(unnamed)";
    if (!pack || !pack->is_string()) {
        diag.error(res.suite, "suite is missing 'pack'");
        return res;
    }
    res.pack = pack->str();

    Decimal tol = Decimal::parse("0.005");
    if (const Json* t = suite.find("tolerance")) {
        if (const Json* a = t->find("abs")) {
            Decimal parsed;
            if (a->is_number() && Decimal::try_parse(a->str(), parsed)) tol = parsed;
            else diag.error(res.suite, "tolerance.abs must be a number");
        }
    }

    auto bundle = packs.bundle_for(res.pack, diag);
    if (!bundle) return res;

    const Json* cases = suite.find("cases");
    if (!cases || !cases->is_array()) {
        diag.error(res.suite, "suite is missing 'cases'");
        return res;
    }

    for (const auto& cj : cases->items()) {
        CaseResult cr;
        const Json* id = cj.find("id");
        cr.id = id && id->is_string() ? id->str() : "(no id)";
        const Json* st = cj.find("status");
        cr.status = st && st->is_string() ? st->str() : "derived";
        if (cr.status != "official" && cr.status != "derived" && cr.status != "pending") {
            cr.failures.push_back("status must be official, derived or pending");
            res.cases.push_back(cr);
            continue;
        }
        if (cr.status == "pending") {
            cr.skipped = true;
            res.cases.push_back(cr);
            continue;
        }

        EvalInputs in;
        bool ok = true;
        if (const Json* inputs = cj.find("inputs")) {
            for (size_t i = 0; i < inputs->keys().size(); ++i) {
                const std::string& k = inputs->keys()[i];
                const Json& v = inputs->value_at(i);
                if (k == "as_of") {
                    if (v.is_string()) in.date = v.str();
                    continue;
                }
                try {
                    in.values[k] = Value::from_json(v);
                } catch (const EvalError& e) {
                    cr.failures.push_back("input '" + k + "': " + e.what());
                    ok = false;
                }
            }
        }
        const Json* expect = cj.find("expect");
        if (!expect || !expect->is_object() || expect->keys().empty()) {
            cr.failures.push_back("case has no expectations");
            ok = false;
        }
        if (ok) {
            Evaluator ev(bundle, in);
            for (size_t i = 0; i < expect->keys().size(); ++i) {
                const std::string& key = expect->keys()[i];
                const Json& want = expect->value_at(i);
                if (want.is_null()) continue;  // awaiting a reference value
                try {
                    Decimal got = !key.empty() && key[0] == '@' ? ev.slot(key.substr(1)) : ev.rule(key).as_num();
                    Decimal exp = Decimal::parse(want.str());
                    const Decimal diff = (got - exp).abs();
                    if (diff > tol) {
                        cr.failures.push_back(key + ": expected " + exp.to_string() + " but got " + got.to_string() +
                                              " (off by " + diff.to_string() + ", tolerance " + tol.to_string() + ")");
                    }
                } catch (const EvalError& e) {
                    cr.failures.push_back(key + ": " + e.what());
                }
            }
        }
        cr.passed = cr.failures.empty();
        res.cases.push_back(std::move(cr));
    }
    return res;
}

}  // namespace jobeval
