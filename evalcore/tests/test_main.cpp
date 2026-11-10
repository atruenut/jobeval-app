// Dependency-free unit tests for evalcore. Exit status is the failure count.
#include <cstdio>
#include <functional>
#include <string>

#include "evalcore/evaluator.hpp"
#include "evalcore/golden.hpp"
#include "evalcore/pack.hpp"

using namespace jobeval;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                                       \
    do {                                                                                     \
        ++g_checks;                                                                          \
        const std::string _a = (a), _b = (b);                                                \
        if (_a != _b) {                                                                      \
            ++g_failures;                                                                    \
            std::printf("  FAILED %s:%d: %s\n    got:      %s\n    expected: %s\n", __FILE__, \
                        __LINE__, #a, _a.c_str(), _b.c_str());                               \
        }                                                                                    \
    } while (0)

template <typename E>
static bool throws(const std::function<void()>& fn) {
    try {
        fn();
    } catch (const E&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

static Decimal D(const char* s) { return Decimal::parse(s); }

static void test_decimal() {
    std::puts("decimal");
    CHECK_EQ((D("0.1") + D("0.2")).to_string(), "0.3");
    CHECK_EQ(D("1.500").to_string(), "1.5");
    CHECK_EQ(D("-0.0595").to_string(), "-0.0595");
    CHECK_EQ(D("0").to_string(), "0");
    CHECK_EQ(D("-0.0").to_string(), "0");
    CHECK_EQ(D("1e3").to_string(), "1000");
    CHECK_EQ(D("2.5e-3").to_string(), "0.0025");
    CHECK_EQ((D("58523") * D("0.14")).to_string(), "8193.22");
    CHECK_EQ((D("1") / D("3")).to_string(), "0.333333333");
    CHECK_EQ((D("-7") / D("2")).to_string(), "-3.5");
    CHECK_EQ(D("2.675").to_fixed(2), "2.68");   // half away from zero, exact (no binary error)
    CHECK_EQ(D("-2.675").to_fixed(2), "-2.68");
    CHECK_EQ(D("12").to_fixed(2), "12.00");
    CHECK_EQ(D("0.004").to_fixed(2), "0.00");
    CHECK_EQ(D("1234.5").round_to(D("1"), RoundMode::HalfEven).to_string(), "1234");
    CHECK_EQ(D("1235.5").round_to(D("1"), RoundMode::HalfEven).to_string(), "1236");
    CHECK_EQ(D("0.12").round_to(D("0.05"), RoundMode::HalfUp).to_string(), "0.1");
    CHECK_EQ(D("-1.5").floor().to_string(), "-2");
    CHECK_EQ(D("-1.5").ceil().to_string(), "-1");
    CHECK(D("1.5") > D("1.49"));
    CHECK(throws<EvalError>([] { D("1.0000000001"); }));  // 10 decimals refused, never silently rounded
    CHECK(throws<EvalError>([] { D("abc"); }));
    CHECK(throws<EvalError>([] { D("1.2.3"); }));
    CHECK(throws<EvalError>([] { D("1") / D("0"); }));
    CHECK(throws<EvalError>([] { D("999999999") * D("999999999") * D("10"); }));
}

static void test_json() {
    std::puts("json");
    Json j = Json::parse(R"J({"a":[1,2.50,"x\né"],"b":{"c":true,"d":null},"n":0.0595})J");
    CHECK(j.is_object());
    CHECK_EQ(j.find("n")->str(), "0.0595");  // number text preserved exactly
    CHECK_EQ(j.find("a")->at(1).str(), "2.50");
    CHECK_EQ(j.find("a")->at(2).str(), std::string("x\n\xc3\xa9"));
    CHECK(j.find("b")->find("d")->is_null());
    CHECK_EQ(Json::parse(j.dump()).dump(), j.dump());  // round trip is stable
    CHECK(throws<JsonError>([] { Json::parse("{\"a\":1,\"a\":2}"); }));  // duplicate keys rejected
    CHECK(throws<JsonError>([] { Json::parse("[1,]"); }));
    CHECK(throws<JsonError>([] { Json::parse("{\"a\":1} x"); }));
    CHECK(throws<JsonError>([] { Json::parse("\"abc"); }));
    try {
        Json::parse("{\n  \"a\": ?\n}");
        CHECK(false);
    } catch (const JsonError& e) {
        CHECK(std::string(e.what()).find("2:8") != std::string::npos);  // line:col in message
    }
}

static std::shared_ptr<const Bundle> bundle_of(const char* pack_json, Diagnostics& diag, const char* id) {
    static PackSet* set = nullptr;
    delete set;
    set = new PackSet();
    set->add(Json::parse(pack_json), "test", diag);
    return set->bundle_for(id, diag);
}

static const char* kMini = R"J({
  "schema": 1, "id": "T-2026", "kind": "country", "region": {"country": "TT"},
  "valid": {"from": "2026-01-01", "to": "2026-12-31"}, "revision": 1, "tier": "partial", "currency": "TTD",
  "sources": {"s": {"authority": "official", "title": "test"}},
  "parameters": {
    "brackets": {"source": "s", "value": [
      {"up_to": 58523, "rate": 0.14}, {"up_to": 117045, "rate": 0.205}, {"rate": 0.33}]},
    "bpa": {"source": "s", "value": {"max": 16452, "min": 14829, "from": 181440, "to": 258482}},
    "cpp": {"source": "s", "value": {"rate": 0.0595, "exemption": 3500, "ympe": 74600, "yampe": 85000, "rate2": 0.04}},
    "mw": {"effective": [
      {"from": "2025-04-01", "value": 15.65, "source": "s"},
      {"from": "2026-04-01", "value": 15.90, "source": "s"}]}
  },
  "inputs": {"gross": {"type": "money"}, "wage": {"type": "money", "default": 20}},
  "rules": [
    {"id": "tax.fed", "expr": "bracket(gross, $brackets)"},
    {"id": "bpa", "expr": "phase_down(gross, $bpa)"},
    {"id": "cpp.base", "expr": "clamp(gross - $cpp.exemption, 0, $cpp.ympe - $cpp.exemption) * $cpp.rate"},
    {"id": "cpp.second", "expr": "clamp(gross - $cpp.ympe, 0, $cpp.yampe - $cpp.ympe) * $cpp.rate2"},
    {"id": "ot", "expr": "max(wage, 1.5 * $mw)"},
    {"id": "flag", "expr": "if(gross > 100000 and not (wage < 10), 1, 0)"}
  ],
  "outputs": {"deductions": ["cpp.base", "cpp.second"], "income_tax": ["tax.fed"], "misc": ["bpa", "ot", "flag"]}
})J";

static Value run(const std::shared_ptr<const Bundle>& b, const char* rule, std::map<std::string, Value> in,
                 const char* date = "2026-06-01") {
    EvalInputs ei;
    ei.values = std::move(in);
    ei.date = date;
    Evaluator ev(b, ei);
    return ev.rule(rule);
}

static Value N(const char* s) { return Value::num(D(s)); }

static void test_evaluator() {
    std::puts("evaluator");
    Diagnostics diag;
    auto b = bundle_of(kMini, diag, "T-2026");
    CHECK(b != nullptr);
    if (!b) {
        std::fputs(diag.format().c_str(), stderr);
        return;
    }
    CHECK_EQ(run(b, "tax.fed", {{"gross", N("80000")}}).as_num().to_string(), "12596.005");
    CHECK_EQ(run(b, "tax.fed", {{"gross", N("58523")}}).as_num().to_string(), "8193.22");  // exactly on an edge
    CHECK_EQ(run(b, "tax.fed", {{"gross", N("0")}}).as_num().to_string(), "0");
    CHECK_EQ(run(b, "tax.fed", {{"gross", N("-5")}}).as_num().to_string(), "0");
    CHECK_EQ(run(b, "cpp.base", {{"gross", N("50000")}}).as_num().to_string(), "2766.75");
    CHECK_EQ(run(b, "cpp.base", {{"gross", N("80000")}}).as_num().to_string(), "4230.45");  // capped at the YMPE
    CHECK_EQ(run(b, "cpp.base", {{"gross", N("2000")}}).as_num().to_string(), "0");         // under the exemption
    CHECK_EQ(run(b, "cpp.second", {{"gross", N("80000")}}).as_num().to_string(), "216");
    CHECK_EQ(run(b, "cpp.second", {{"gross", N("200000")}}).as_num().to_string(), "416");   // capped at the YAMPE
    CHECK_EQ(run(b, "bpa", {{"gross", N("100000")}}).as_num().to_string(), "16452");
    CHECK_EQ(run(b, "bpa", {{"gross", N("300000")}}).as_num().to_string(), "14829");
    CHECK_EQ(run(b, "bpa", {{"gross", N("200000")}}).as_num().to_fixed(2), "16061.01");
    // Effective-dated parameters: the value in force on the date wins.
    CHECK_EQ(run(b, "ot", {}, "2026-03-31").as_num().to_string(), "23.475");  // 1.5 x 15.65
    CHECK_EQ(run(b, "ot", {}, "2026-04-01").as_num().to_string(), "23.85");   // 1.5 x 15.90
    CHECK_EQ(run(b, "ot", {{"wage", N("30")}}, "2026-06-01").as_num().to_string(), "30");  // wage above the floor
    CHECK(throws<EvalError>([&] { run(b, "ot", {}, "2024-01-01"); }));  // before any value is in effect
    CHECK(throws<EvalError>([&] { run(b, "ot", {}, ""); }));            // date required
    CHECK(throws<EvalError>([&] { run(b, "tax.fed", {}); }));           // missing input without default
    CHECK_EQ(run(b, "flag", {{"gross", N("150000")}}).as_num().to_string(), "1");
    CHECK_EQ(run(b, "flag", {{"gross", N("5")}}).as_num().to_string(), "0");

    EvalInputs ei;
    ei.values = {{"gross", N("80000")}};
    ei.date = "2026-06-01";
    Evaluator ev(b, ei);
    CHECK_EQ(ev.slot("deductions").to_string(), "4446.45");
    CHECK_EQ(ev.eval_text("1 + 2 * 3").as_num().to_string(), "7");
    CHECK_EQ(ev.eval_text("(1 + 2) * 3").as_num().to_string(), "9");
    CHECK_EQ(ev.eval_text("-2 * -3").as_num().to_string(), "6");
    CHECK_EQ(ev.eval_text("10 - 4 - 3").as_num().to_string(), "3");  // left associative
    CHECK_EQ(ev.eval_text("round(2.675)").as_num().to_string(), "2.68");
    CHECK_EQ(ev.eval_text("round(2.5, 1, \"half_even\")").as_num().to_string(), "2");
    CHECK_EQ(ev.eval_text("min(3, 1, 2) + max(3, 1, 2)").as_num().to_string(), "4");
    CHECK(throws<EvalError>([&] { ev.eval_text("1 / 0"); }));
    CHECK(throws<EvalError>([&] { ev.eval_text("1 + true"); }));  // type errors are errors, not coercions
    CHECK(throws<ParseError>([&] { ev.eval_text("1 +"); }));
    CHECK(throws<ParseError>([&] { ev.eval_text("nope(1)"); }));
    CHECK(throws<ParseError>([&] { ev.eval_text("clamp(1, 2)"); }));  // arity checked at parse time
    CHECK(throws<ParseError>([&] { ev.eval_text("a = 1"); }));
    CHECK(throws<EvalError>([&] { ev.eval_text("$nothing.here"); }));
    // Trace records every rule evaluated, with the values it read.
    ev.rule("cpp.base");
    bool found = false;
    for (const auto& t : ev.trace()) {
        if (t.id == "cpp.base") {
            found = true;
            CHECK_EQ(t.value.describe(), "4230.45");
            CHECK(!t.used.empty());
        }
    }
    CHECK(found);
}

static void test_schedules() {
    std::puts("schedules");
    Json t = Json::parse(R"J([{"up_to":100,"value":1},{"up_to":200,"value":2},{"value":3}])J");
    CHECK_EQ(fn_tier_lookup(D("50"), t).to_string(), "1");
    CHECK_EQ(fn_tier_lookup(D("100"), t).to_string(), "1");  // limit is inclusive
    CHECK_EQ(fn_tier_lookup(D("100.01"), t).to_string(), "2");
    CHECK_EQ(fn_tier_lookup(D("1000"), t).to_string(), "3");
    CHECK_EQ(fn_phase_out(D("1000"), D("30000"), D("20000"), D("0.15")).to_string(), "0");     // fully phased out
    CHECK_EQ(fn_phase_out(D("1000"), D("25000"), D("20000"), D("0.10")).to_string(), "500");
    CHECK_EQ(fn_phase_out(D("1000"), D("15000"), D("20000"), D("0.10")).to_string(), "1000");  // below the start
    CHECK(throws<EvalError>([&] { fn_bracket(D("1"), Json::parse(R"J([{"up_to":1,"rate":0.1}])J")); }));  // no open row
    CHECK(throws<EvalError>([&] { fn_bracket(D("1"), Json::parse(R"J([{"up_to":5,"rate":0.1},{"up_to":3,"rate":0.2},{"rate":0.3}])J")); }));
}

static const char* kChild = R"J({
  "schema": 1, "id": "T-XX-2026", "kind": "subregion", "extends": "T-2026", "region": {"country": "TT", "subregion": "XX"},
  "valid": {"from": "2026-01-01", "to": "2026-12-31"}, "revision": 1, "tier": "partial",
  "sources": {"s2": {"authority": "official", "title": "child"}},
  "parameters": {"xx.rate": {"source": "s2", "value": 0.1}},
  "rules": [{"id": "xx.tax", "expr": "gross * $xx.rate"}],
  "append": {"income_tax": ["xx.tax"]}
})J";

static void test_packs() {
    std::puts("packs");
    {  // layering: the child's tax is appended to the parent's income_tax slot
        PackSet set;
        Diagnostics diag;
        CHECK(set.add(Json::parse(kMini), "mini", diag));
        CHECK(set.add(Json::parse(kChild), "child", diag));
        auto b = set.bundle_for("T-XX-2026", diag);
        CHECK(b != nullptr);
        if (b) {
            CHECK_EQ(b->tier, "partial");
            CHECK_EQ(b->currency, "TTD");  // inherited
            CHECK_EQ(std::to_string(b->chain.size()), "2");
            EvalInputs ei;
            ei.values = {{"gross", N("80000")}};
            ei.date = "2026-06-01";
            Evaluator ev(b, ei);
            CHECK_EQ(ev.slot("income_tax").to_string(), "20596.005");  // 12596.005 + 8000
        }
    }
    {  // a child may not silently shadow a parent's rule
        std::string shadow = kChild;
        shadow.replace(shadow.find("\"xx.tax\""), 8, "\"tax.fed\"");
        shadow.replace(shadow.find("[\"xx.tax\"]"), 10, "[\"tax.fed\"]");
        PackSet set;
        Diagnostics diag;
        set.add(Json::parse(kMini), "mini", diag);
        set.add(Json::parse(shadow), "child", diag);
        CHECK(set.bundle_for("T-XX-2026", diag) == nullptr);
        CHECK(diag.format().find("already defined by a parent") != std::string::npos);
    }
    {  // ... unless it says so explicitly
        std::string over = kChild;
        over.replace(over.find("\"xx.tax\""), 8, "\"tax.fed\"");
        over.replace(over.find("[\"xx.tax\"]"), 10, "[]");
        over.insert(over.find("\"rules\""), "\"replace\": [\"tax.fed\"],\n  ");
        PackSet set;
        Diagnostics diag;
        set.add(Json::parse(kMini), "mini", diag);
        set.add(Json::parse(over), "child", diag);
        auto b = set.bundle_for("T-XX-2026", diag);
        CHECK(b != nullptr);
        if (b) {
            EvalInputs ei;
            ei.values = {{"gross", N("80000")}};
            Evaluator ev(b, ei);
            CHECK_EQ(ev.rule("tax.fed").as_num().to_string(), "8000");
        }
    }
    {  // dependency cycle
        const char* cyc = R"J({
          "schema": 1, "id": "C-1", "kind": "country", "region": {"country": "CC"},
          "valid": {"from": "2026-01-01"}, "revision": 1, "tier": "estimate", "currency": "CCC",
          "rules": [{"id": "a", "expr": "b + 1"}, {"id": "b", "expr": "a + 1"}], "outputs": {"o": ["a", "b"]}
        })J";
        PackSet set;
        Diagnostics diag;
        set.add(Json::parse(cyc), "cyc", diag);
        CHECK(set.bundle_for("C-1", diag) == nullptr);
        CHECK(diag.format().find("dependency cycle") != std::string::npos);
    }
    {  // unresolved references, bad expressions, missing sources
        const char* bad = R"J({
          "schema": 1, "id": "B-1", "kind": "country", "region": {"country": "BB"},
          "valid": {"from": "2026-01-01"}, "revision": 1, "tier": "partial", "currency": "BBB",
          "parameters": {"p": {"source": "ghost", "value": 1}},
          "rules": [{"id": "a", "expr": "zzz + $nope"}, {"id": "b", "expr": "1 +"}]
        })J";
        PackSet set;
        Diagnostics diag;
        CHECK(!set.add(Json::parse(bad), "bad", diag));  // the bad expression fails the load
        CHECK(diag.format().find("expression error") != std::string::npos);
    }
    {  // tier gating: "full" needs official sources and nothing left unmodelled
        std::string full = kMini;
        full.replace(full.find("\"partial\""), 9, "\"full\"");
        full.replace(full.find("\"authority\": \"official\""), 23, "\"authority\": \"third_party\"");
        PackSet set;
        Diagnostics diag;
        set.add(Json::parse(full), "full", diag);
        CHECK(set.bundle_for("T-2026", diag) == nullptr);
        CHECK(diag.format().find("requires official sources") != std::string::npos);

        std::string nm = kMini;
        nm.replace(nm.find("\"partial\""), 9, "\"full\"");
        nm.insert(nm.rfind('}'), ", \"not_modelled\": [\"credits\"]");
        PackSet set2;
        Diagnostics d2;
        set2.add(Json::parse(nm), "nm", d2);
        CHECK(set2.bundle_for("T-2026", d2) == nullptr);
        CHECK(d2.format().find("not_modelled") != std::string::npos);
    }
    {  // engine version gate
        std::string future = kMini;
        future.replace(future.find("\"revision\": 1"), 13, "\"revision\": 1, \"requires_engine\": \">=9.0\"");
        PackSet set;
        Diagnostics diag;
        CHECK(!set.add(Json::parse(future), "future", diag));
        CHECK(diag.format().find("update the app") != std::string::npos);
    }
}

static void test_golden() {
    std::puts("golden");
    PackSet set;
    Diagnostics diag;
    set.add(Json::parse(kMini), "mini", diag);
    Json suite = Json::parse(R"J({
      "suite": "T", "pack": "T-2026", "tolerance": {"abs": "0.01"},
      "cases": [
        {"id": "ok", "status": "derived", "inputs": {"gross": 50000, "as_of": "2026-06-01"},
         "expect": {"cpp.base": 2766.75, "@deductions": 2766.75}},
        {"id": "wrong", "status": "derived", "inputs": {"gross": 50000}, "expect": {"cpp.base": 2766.00}},
        {"id": "waiting", "status": "pending", "inputs": {"gross": 1}, "expect": {"tax.fed": null}},
        {"id": "null-skips-key", "status": "derived", "inputs": {"gross": 50000}, "expect": {"cpp.base": 2766.75, "tax.fed": null}}
      ]})J");
    SuiteResult r = run_suite(set, suite, diag);
    CHECK_EQ(std::to_string(r.passed()), "2");
    CHECK_EQ(std::to_string(r.failed()), "1");
    CHECK_EQ(std::to_string(r.skipped()), "1");
    CHECK(r.cases[1].failures.size() == 1 && r.cases[1].failures[0].find("expected 2766 but got 2766.75") != std::string::npos);
}

int main() {
    test_decimal();
    test_json();
    test_evaluator();
    test_schedules();
    test_packs();
    test_golden();
    std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures;
}
