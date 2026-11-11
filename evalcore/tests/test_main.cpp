// Dependency-free unit tests for evalcore. Exit status is the failure count.
#include <cstdio>
#include <functional>
#include <string>

#include "evalcore/evaluator.hpp"
#include "evalcore/golden.hpp"
#include "evalcore/catalog.hpp"
#include "evalcore/hash.hpp"
#include "evalcore/minisign.hpp"
#include "evalcore/packstore.hpp"
#include "evalcore/takehome.hpp"
#include "evalcore/realhourly.hpp"

#include <filesystem>
#include <fstream>
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


// ------------------------------------------------------------------ M2: packs on disk

namespace fs = std::filesystem;

static std::string fixture(const std::string& rel) {
    std::string s;
    if (!read_whole_file(std::string(JOBEVAL_FIXTURE_DIR) + "/" + rel, s)) {
        ++g_failures;
        std::printf("  FAILED: cannot read fixture %s\n", rel.c_str());
    }
    return s;
}

static std::string hex_of(const std::array<uint8_t, 32>& a) { return to_hex(a.data(), a.size()); }
static std::string hex_of(const std::array<uint8_t, 64>& a) { return to_hex(a.data(), a.size()); }

static void test_hashes() {
    std::puts("hashes");
    Json v = Json::parse(fixture("vectors/hashes.json"));
    auto want = [&](const char* k) { return v.find(k)->str(); };
    CHECK_EQ(hex_of(sha256(std::string())), want("sha256_empty"));
    CHECK_EQ(hex_of(sha256(std::string("abc"))), want("sha256_abc"));
    CHECK_EQ(sha256_hex(std::string(1000, 'a')), want("sha256_1000a"));
    CHECK_EQ(hex_of(sha512(nullptr, 0)), want("sha512_empty"));
    const std::string abc = "abc";
    CHECK_EQ(hex_of(sha512(reinterpret_cast<const uint8_t*>(abc.data()), abc.size())), want("sha512_abc"));
    const std::string thousand(1000, 'a');
    CHECK_EQ(hex_of(sha512(reinterpret_cast<const uint8_t*>(thousand.data()), thousand.size())), want("sha512_1000a"));
    CHECK_EQ(hex_of(blake2b512(reinterpret_cast<const uint8_t*>(abc.data()), abc.size())), want("blake2b_abc"));
    // Block-boundary lengths exercise the padding paths (55/56/63/64 and 111/112/127/128).
    for (size_t n : {55u, 56u, 63u, 64u, 65u, 111u, 112u, 127u, 128u, 129u}) {
        std::string m(n, 'x');
        CHECK(sha256_hex(m).size() == 64);
        CHECK(to_hex(sha512(reinterpret_cast<const uint8_t*>(m.data()), m.size()).data(), 64).size() == 128);
    }
    Bytes b;
    CHECK(base64_decode("aGVsbG8=", b) && std::string(b.begin(), b.end()) == "hello");
    CHECK(base64_decode("aGVsbA==", b) && std::string(b.begin(), b.end()) == "hell");
    CHECK(base64_decode("aGVs", b) && std::string(b.begin(), b.end()) == "hel");
    CHECK(!base64_decode("aGVsbG8", b));    // missing padding
    CHECK(!base64_decode("aGV$bG8=", b));   // bad character
    CHECK(!base64_decode("a=Vs", b));       // padding in the wrong place
    CHECK_EQ(base64_encode(reinterpret_cast<const uint8_t*>("hello"), 5), "aGVsbG8=");
}

static MinisignKey test_key() {
    MinisignKey k;
    std::string err;
    CHECK(parse_minisign_public_key(fixture("test.pub"), k, err));
    return k;
}

static void test_minisign() {
    std::puts("minisign");
    const MinisignKey key = test_key();
    const std::string msg = fixture("vectors/message.txt");
    const std::string ed = fixture("vectors/prehashed.minisig");
    const std::string legacy = fixture("vectors/legacy.minisig");

    SignatureCheck ok = verify_minisign(key, msg, ed);
    CHECK(ok.ok);
    CHECK_EQ(ok.trusted_comment, "timestamp:1 file:message.txt");
    CHECK(verify_minisign(key, msg, legacy).ok);  // legacy "Ed" (unhashed) signatures
    CHECK(verify_minisign(key, msg, ed + "\r\n").ok);

    CHECK(!verify_minisign(key, msg + "x", ed).ok);  // altered data
    SignatureCheck altered = verify_minisign(key, msg + "x", ed);
    CHECK(altered.error.find("does not match the data") != std::string::npos);
    CHECK(altered.trusted_comment.empty());

    std::string forged = ed;
    forged.replace(forged.find("timestamp:1"), 11, "timestamp:2");  // trusted comment is signed too
    SignatureCheck tc = verify_minisign(key, msg, forged);
    CHECK(!tc.ok);
    CHECK(tc.error.find("trusted comment") != std::string::npos);

    MinisignKey other_id = key;
    other_id.key_id[0] ^= 1;
    CHECK(verify_minisign(other_id, msg, ed).error.find("different key") != std::string::npos);
    MinisignKey other_pk = key;
    other_pk.public_key[5] ^= 1;
    CHECK(!verify_minisign(other_pk, msg, ed).ok);

    CHECK(!verify_minisign(key, msg, "").ok);
    CHECK(!verify_minisign(key, msg, "untrusted comment: x\nAAAA\ntrusted comment: y\nAAAA\n").ok);
    // The legacy file must not verify as if it were prehashed, and vice versa.
    std::string swapped = legacy;
    CHECK(!verify_minisign(key, msg + "\n", swapped).ok);

    // Key parsing: bare base64, full .pub file, and rejects.
    MinisignKey k;
    std::string err;
    CHECK(parse_minisign_public_key("untrusted comment: minisign public key X\n" + fixture("test.pub"), k, err));
    CHECK(parse_minisign_public_key("RWTO7IvPGG06wlW/mdSpbzkrKXHcF9xgPI5AOWBeei+EJPZ/dQw16RrC", k, err));  // the published key
    CHECK(!parse_minisign_public_key("not base64!", k, err));
    CHECK(!parse_minisign_public_key("QUJD", k, err));
}

static void test_catalog() {
    std::puts("catalog");
    Catalog c;
    Diagnostics diag;
    CHECK(parse_catalog(fixture("site_v1/index.json"), c, diag));
    CHECK_EQ(std::to_string(c.packs.size()), "2");
    CHECK_EQ(c.generated, "2026-11-01T00:00:00Z");
    CHECK(c.find("T-XX-2026") != nullptr && c.find("T-XX-2026")->extends == "T-2026");
    CHECK(c.find("nope") == nullptr);

    CHECK(is_safe_pack_path("packs/CA-2026-r1.93616814.json"));
    CHECK(!is_safe_pack_path("packs/../evil.json"));
    CHECK(!is_safe_pack_path("../packs/x.json"));
    CHECK(!is_safe_pack_path("packs/sub/x.json"));
    CHECK(!is_safe_pack_path("packs/x.exe"));
    CHECK(!is_safe_pack_path("/etc/passwd"));
    CHECK(!is_safe_pack_path("packs/.hidden.json"));
    CHECK(!is_safe_pack_path("packs/a b.json"));
    CHECK(!is_safe_pack_path("packs\\x.json"));

    std::string bad = fixture("site_v1/index.json");
    bad.replace(bad.find("\"file\": \"packs/T-2026"), 20, "\"file\": \"packs/../T-");
    Diagnostics d2;
    Catalog c2;
    CHECK(!parse_catalog(bad, c2, d2));
    CHECK(d2.format().find("unsafe") != std::string::npos);
    Diagnostics d3;
    CHECK(!parse_catalog("{\"catalog\":2,\"generated\":\"x\",\"packs\":[]}", c2, d3));
    CHECK(!parse_catalog("[]", c2, d3));

    std::vector<const CatalogEntry*> plan;
    Diagnostics d4;
    CHECK(plan_install(c, "T-XX-2026", {}, plan, d4));
    CHECK(plan.size() == 2 && plan[0]->id == "T-2026" && plan[1]->id == "T-XX-2026");  // parent first
    std::map<std::string, InstallRecord> have;
    have["T-2026"] = {"T-2026", 1, c.find("T-2026")->file, c.find("T-2026")->sha256};
    CHECK(plan_install(c, "T-XX-2026", have, plan, d4));
    CHECK(plan.size() == 1 && plan[0]->id == "T-XX-2026");
    CHECK(!plan_install(c, "missing", {}, plan, d4));
    Catalog future = c;
    future.packs[0].requires_engine = ">=9.0";
    Diagnostics d5;
    CHECK(!plan_install(future, "T-XX-2026", {}, plan, d5));
    CHECK(d5.format().find("update the app") != std::string::npos);
}

static int rev_of(const std::map<std::string, InstallRecord>& m, const char* id) {
    auto it = m.find(id);
    return it == m.end() ? -1 : it->second.revision;
}

static fs::path scratch_dir(const char* name) {
    fs::path p = fs::temp_directory_path() / (std::string("jobeval-test-") + name);
    std::error_code ec;
    fs::remove_all(p, ec);
    fs::create_directories(p);
    return p;
}

static fs::path copy_site(const char* version, const fs::path& to) {
    fs::copy(fs::path(JOBEVAL_FIXTURE_DIR) / version, to, fs::copy_options::recursive);
    return to;
}

static std::string slot_tax(PackStore& store, const char* pack, const char* gross) {
    PackSet set;
    Diagnostics diag;
    store.load_installed(set, diag);
    auto b = set.bundle_for(pack, diag);
    if (!b) return "bundle failed: " + diag.format();
    EvalInputs in;
    in.values["gross"] = N(gross);
    Evaluator ev(b, in);
    return ev.slot("income_tax").to_string();
}

static void test_packstore() {
    std::puts("packstore");
    const MinisignKey key = test_key();
    const fs::path root = scratch_dir("store");
    const fs::path v1 = copy_site("site_v1", root / "v1");
    const fs::path v2 = copy_site("site_v2", root / "v2");
    DirFetcher f1((root / "v1").string()), f2((root / "v2").string());

    {  // install, no-op reinstall, upgrade, previous kept, rollback
        PackStore store((root / "cache").string(), key);
        Catalog cat;
        Diagnostics diag;
        CHECK(store.refresh_catalog(f1, cat, diag));
        CHECK(store.install(cat, "T-XX-2026", f1, diag));
        CHECK_EQ(std::to_string(store.installed().size()), "2");
        CHECK_EQ(slot_tax(store, "T-XX-2026", "20000"), "2700");  // 700 + 2000
        CHECK(store.install(cat, "T-XX-2026", f1, diag));          // already installed: no-op
        CHECK(store.updates_available(cat).empty());

        Catalog cat2;
        CHECK(store.refresh_catalog(f2, cat2, diag));
        CHECK_EQ(std::to_string(store.updates_available(cat2).size()), "1");
        CHECK(store.install(cat2, "T-XX-2026", f2, diag));
        CHECK_EQ(std::to_string(rev_of(store.installed(), "T-XX-2026")), "2");
        CHECK_EQ(std::to_string(rev_of(store.previous(), "T-XX-2026")), "1");
        CHECK_EQ(slot_tax(store, "T-XX-2026", "20000"), "3100");   // 700 + 2400

        CHECK(store.rollback("T-XX-2026", diag));
        CHECK_EQ(std::to_string(rev_of(store.installed(), "T-XX-2026")), "1");
        CHECK_EQ(slot_tax(store, "T-XX-2026", "20000"), "2700");
        CHECK(!store.rollback("T-2026", diag));  // never replaced: nothing to go back to

        // A validly signed but OLDER catalog is refused once a newer one is cached.
        Diagnostics d_old;
        Catalog old_cat;
        CHECK(!store.refresh_catalog(f1, old_cat, d_old));
        CHECK(d_old.format().find("older than the one already cached") != std::string::npos);
        Catalog cached;
        Diagnostics d_c;
        CHECK(store.load_cached_catalog(cached, d_c));
        CHECK_EQ(cached.generated, "2026-11-02T00:00:00Z");  // cache kept the newer catalog
    }
    {  // a tampered pack download is refused and leaves the cache untouched
        const fs::path bad = copy_site("site_v1", root / "tampered_pack");
        std::string path;
        for (auto& e : fs::directory_iterator(bad / "packs")) {
            if (e.path().filename().string().rfind("T-XX-2026", 0) == 0) path = e.path().string();
        }
        std::string text;
        read_whole_file(path, text);
        text[text.find("0.10")] = '9';
        { std::ofstream(path, std::ios::binary | std::ios::trunc) << text; }
        PackStore store((root / "cache_tp").string(), key);
        DirFetcher fb(bad.string());
        Catalog cat;
        Diagnostics diag;
        CHECK(store.refresh_catalog(fb, cat, diag));  // catalog itself is fine
        CHECK(!store.install(cat, "T-XX-2026", fb, diag));
        CHECK(diag.format().find("hash mismatch") != std::string::npos);
        CHECK(store.installed().empty());
    }
    {  // a modified catalog fails its signature
        const fs::path bad = copy_site("site_v1", root / "tampered_index");
        std::string idx;
        read_whole_file((bad / "index.json").string(), idx);
        idx.replace(idx.find("2026-11-01"), 10, "2099-01-01");
        { std::ofstream(bad / "index.json", std::ios::binary | std::ios::trunc) << idx; }
        PackStore store((root / "cache_ti").string(), key);
        DirFetcher fb(bad.string());
        Catalog cat;
        Diagnostics diag;
        CHECK(!store.refresh_catalog(fb, cat, diag));
        CHECK(diag.format().find("signature check failed") != std::string::npos);
        CHECK(!fs::exists(root / "cache_ti" / "catalog" / "index.json"));  // nothing cached
    }
    {  // unsigned catalogs are refused
        const fs::path bad = copy_site("site_v1", root / "unsigned");
        fs::remove(bad / "index.json.minisig");
        PackStore store((root / "cache_un").string(), key);
        DirFetcher fb(bad.string());
        Catalog cat;
        Diagnostics diag;
        CHECK(!store.refresh_catalog(fb, cat, diag));
        CHECK(diag.format().find("unsigned catalogs are refused") != std::string::npos);
    }
    {  // wrong key
        MinisignKey wrong = key;
        wrong.key_id[3] ^= 0x40;
        PackStore store((root / "cache_wk").string(), wrong);
        Catalog cat;
        Diagnostics diag;
        CHECK(!store.refresh_catalog(f1, cat, diag));
    }
    {  // cached pack files are re-verified on every load
        PackStore store((root / "cache_ct").string(), key);
        Catalog cat;
        Diagnostics diag;
        CHECK(store.refresh_catalog(f1, cat, diag));
        CHECK(store.install(cat, "T-2026", f1, diag));
        auto inst = store.installed();
        CHECK(inst.count("T-2026") == 1);
        if (!inst.count("T-2026")) return;
        const std::string file = (root / "cache_ct" / inst.at("T-2026").file).string();
        std::string text;
        read_whole_file(file, text);
        text[text.find("13000")] = '9';
        { std::ofstream(file, std::ios::binary | std::ios::trunc) << text; }
        PackSet set;
        Diagnostics d2;
        CHECK(!store.load_installed(set, d2));
        CHECK(d2.format().find("recorded hash") != std::string::npos);
    }
    {  // a catalog naming a missing file fails cleanly, all-or-nothing
        const fs::path bad = copy_site("site_v1", root / "missing_file");
        for (auto& e : fs::directory_iterator(bad / "packs")) {
            if (e.path().filename().string().rfind("T-XX-2026", 0) == 0) fs::remove(e.path());
        }
        PackStore store((root / "cache_mf").string(), key);
        DirFetcher fb(bad.string());
        Catalog cat;
        Diagnostics diag;
        CHECK(store.refresh_catalog(fb, cat, diag));
        CHECK(!store.install(cat, "T-XX-2026", fb, diag));
        CHECK(store.installed().empty());  // the parent was not installed either
    }
    std::error_code ec;
    fs::remove_all(root, ec);
}

static const char* kTakeHomePack = R"J({
  "schema": 1, "id": "H-2026", "kind": "country", "region": {"country": "HH"},
  "valid": {"from": "2026-01-01"}, "revision": 1, "tier": "partial", "currency": "HHH",
  "sources": {"s": {"authority": "official", "title": "t"}},
  "parameters": {
    "tax": {"source": "s", "value": [{"up_to": 10000, "rate": 0}, {"rate": 0.2}]},
    "cpp": {"source": "s", "value": {"rate": 0.05, "cap": 3000}},
    "region_factor": {"source": "s", "value": 1}
  },
  "inputs": {"gross_annual": {"type": "money"}, "wage": {"type": "money", "default": 0},
             "union_dues": {"type": "money", "default": 100, "label": "Union dues"}},
  "rules": [
    {"id": "income.tax", "label": {"en": "Income tax"}, "expr": "bracket(gross_annual, $tax)"},
    {"id": "pension", "label": {"en": "Pension"}, "expr": "min(gross_annual * $cpp.rate, $cpp.cap)"},
    {"id": "dues", "label": {"en": "Union dues"}, "expr": "union_dues"}
  ],
  "outputs": {"income_tax": ["income.tax"], "contributions": ["pension", "dues"]}
})J";

static void test_take_home() {
    std::puts("take-home");
    Diagnostics diag;
    auto b = bundle_of(kTakeHomePack, diag, "H-2026");
    CHECK(b != nullptr);
    if (!b) return;
    TakeHomeInput in;
    in.hourly_wage = D("20");
    in.hours_per_week = D("40");
    in.weeks_per_year = D("50");  // 40000 gross
    in.as_of = "2026-06-01";
    TakeHomeResult r;
    std::vector<TraceEntry> trace;
    std::string err;
    CHECK(compute_take_home(b, in, r, trace, err));
    CHECK_EQ(r.gross_annual.to_string(), "40000");
    CHECK_EQ(r.income_tax.to_string(), "6000");        // (40000-10000) * 0.2
    CHECK_EQ(r.contributions.to_string(), "2100");     // pension capped at the pack maximum 40000*0.05=2000, + dues 100
    CHECK_EQ(r.net_annual.to_string(), "31900");
    CHECK_EQ(r.net_monthly.to_fixed(2), "2658.33");
    CHECK_EQ(r.hours_per_year.to_string(), "2000");
    CHECK_EQ(r.net_hourly.to_string(), "15.95");
    CHECK(r.contribution_lines.size() == 2 && r.contribution_lines[0].label == "Pension");
    CHECK(!trace.empty());

    in.extra["union_dues"] = N("0");  // a pack-declared input can be overridden
    CHECK(compute_take_home(b, in, r, trace, err));
    CHECK_EQ(r.contributions.to_string(), "2000");

    in.hours_per_week = D("0");  // no hours: no division by zero, hourly figure is zero
    CHECK(compute_take_home(b, in, r, trace, err));
    CHECK_EQ(r.net_hourly.to_string(), "0");
    CHECK_EQ(r.gross_annual.to_string(), "0");

    in.hourly_wage = D("-1");
    CHECK(!compute_take_home(b, in, r, trace, err));
    CHECK(err.find("negative") != std::string::npos);

    // A pack with no slots at all is a valid (if empty) region: take-home equals gross.
    const char* bare = R"J({"schema":1,"id":"E-1","kind":"country","region":{"country":"EE"},"valid":{"from":"2026-01-01"},"revision":1,"tier":"estimate","currency":"EEE"})J";
    Diagnostics d2;
    auto b2 = bundle_of(bare, d2, "E-1");
    CHECK(b2 != nullptr);
    if (b2) {
        TakeHomeInput in2;
        in2.hourly_wage = D("10");
        in2.hours_per_week = D("10");
        in2.weeks_per_year = D("52");
        CHECK(compute_take_home(b2, in2, r, trace, err));
        CHECK_EQ(r.net_annual.to_string(), "5200");
    }
}

static const char* kOvertimePack = R"J({
  "schema": 1, "id": "O-2026", "kind": "country", "region": {"country": "OO"},
  "valid": {"from": "2026-01-01"}, "revision": 1, "tier": "partial", "currency": "OOO",
  "sources": {"s": {"authority": "official", "title": "t"}},
  "parameters": {"min_wage": {"source": "s", "value": 15}, "ot_after": {"source": "s", "value": 44}},
  "inputs": {"gross_annual": {"type": "money"}, "wage": {"type": "money", "default": 0}},
  "rules": [
    {"id": "ot.rate", "expr": "max(wage, 1.5 * $min_wage)"},
    {"id": "ot.after", "expr": "$ot_after"}
  ],
  "outputs": {"overtime_rate": ["ot.rate"], "overtime_threshold_hours": ["ot.after"]}
})J";

static void test_overtime() {
    std::puts("overtime");
    Diagnostics diag;
    auto b = bundle_of(kOvertimePack, diag, "O-2026");
    CHECK(b != nullptr);
    if (!b) return;
    TakeHomeInput in;
    in.hourly_wage = D("20");
    in.hours_per_week = D("50");
    in.weeks_per_year = D("52");
    in.as_of = "2026-06-01";
    TakeHomeResult r;
    std::vector<TraceEntry> trace;
    std::string err;
    CHECK(compute_take_home(b, in, r, trace, err));
    // rate = max(20, 22.5) = 22.5; weekly = 20*44 + 22.5*6 = 1015; annual = 52780
    CHECK(r.overtime_applied);
    CHECK_EQ(r.overtime_rate.to_string(), "22.5");
    CHECK_EQ(r.overtime_hours_per_week.to_string(), "6");
    CHECK_EQ(r.overtime_pay_annual.to_string(), "7020");
    CHECK_EQ(r.gross_annual.to_string(), "52780");

    in.hourly_wage = D("30");  // above the floor: overtime never pays less than the wage
    CHECK(compute_take_home(b, in, r, trace, err));
    CHECK_EQ(r.overtime_rate.to_string(), "30");
    CHECK_EQ(r.gross_annual.to_string(), "78000");  // 30 * 50 * 52

    in.hourly_wage = D("20");
    in.hours_per_week = D("44");  // at the threshold: no overtime
    CHECK(compute_take_home(b, in, r, trace, err));
    CHECK(!r.overtime_applied);
    CHECK_EQ(r.gross_annual.to_string(), "45760");

    in.hours_per_week = D("50");
    in.use_statutory_overtime = false;  // salaried / exempt
    CHECK(compute_take_home(b, in, r, trace, err));
    CHECK(!r.overtime_applied);
    CHECK_EQ(r.gross_annual.to_string(), "52000");
}

static RealHourlyInput sample_real_hourly() {
    RealHourlyInput in;
    in.net_annual = D("32000");
    in.gross_annual = D("40000");
    in.hours_per_week = D("40");
    in.paid_weeks = D("52");
    in.paid_vacation_weeks = D("2");
    in.days_per_week = D("5");
    in.unpaid_minutes_per_day = D("30");
    in.commute_distance = D("10");
    in.commute_minutes = D("20");
    in.cost_per_distance = D("0.25");
    in.work_costs_annual = D("300");
    in.benefits_monthly_value = D("100");
    in.pension_match_percent = D("3");
    return in;
}

static void test_real_hourly() {
    std::puts("real hourly");
    RealHourlyInput in = sample_real_hourly();
    RealHourlyResult r;
    std::string err;
    CHECK(compute_real_hourly(in, r, err));
    // Independent exact arithmetic: 50 weeks worked, 250 days, commute 2*20*250/60 h, etc.
    CHECK_EQ(r.weeks_worked.to_string(), "50");
    CHECK_EQ(r.days_worked.to_string(), "250");
    CHECK_EQ(r.hours_paid.to_string(), "2080");
    CHECK_EQ(r.hours_worked.to_string(), "2000");
    CHECK_EQ(r.commute_hours.to_fixed(6), "166.666667");
    CHECK_EQ(r.unpaid_hours.to_string(), "125");
    CHECK_EQ(r.total_hours.to_fixed(6), "2291.666667");
    CHECK_EQ(r.commute_distance_total.to_string(), "5000");
    CHECK_EQ(r.vehicle_cost.to_string(), "1250");
    CHECK_EQ(r.commute_cost.to_string(), "1250");
    CHECK_EQ(r.pension_match_value.to_string(), "1200");
    CHECK_EQ(r.benefits_value.to_string(), "2400");
    CHECK_EQ(r.value_annual.to_string(), "32850");
    CHECK_EQ(r.take_home_per_hour_paid.to_fixed(4), "15.3846");
    CHECK_EQ(r.take_home_per_hour_worked.to_string(), "16");
    CHECK_EQ(r.real_hourly.to_fixed(4), "14.3345");
    CHECK_EQ(r.hidden_cost_share.to_fixed(4), "0.0683");

    // A job with no commute, no extras and no vacation: real hourly equals take-home per hour.
    RealHourlyInput plain;
    plain.net_annual = D("31200");
    plain.gross_annual = D("40000");
    plain.hours_per_week = D("40");
    plain.paid_weeks = D("52");
    plain.days_per_week = D("5");
    CHECK(compute_real_hourly(plain, r, err));
    CHECK_EQ(r.real_hourly.to_string(), "15");
    CHECK_EQ(r.take_home_per_hour_paid.to_string(), "15");
    CHECK_EQ(r.hidden_cost_share.to_string(), "0");

    // Transit and parking are charged per day worked, not per paid day.
    RealHourlyInput t = plain;
    t.paid_vacation_weeks = D("4");
    t.daily_transit_fare = D("6");
    t.daily_parking_and_tolls = D("4");
    CHECK(compute_real_hourly(t, r, err));
    CHECK_EQ(r.days_worked.to_string(), "240");
    CHECK_EQ(r.transit_cost.to_string(), "1440");
    CHECK_EQ(r.parking_and_tolls.to_string(), "960");

    // Zero hours: no division by zero.
    RealHourlyInput z = plain;
    z.hours_per_week = D("0");
    CHECK(compute_real_hourly(z, r, err));
    CHECK_EQ(r.real_hourly.to_string(), "0");

    RealHourlyInput bad = plain;
    bad.paid_vacation_weeks = D("53");
    CHECK(!compute_real_hourly(bad, r, err));
    bad = plain;
    bad.commute_minutes = D("-5");
    CHECK(!compute_real_hourly(bad, r, err));
    bad = plain;
    bad.days_per_week = D("8");
    CHECK(!compute_real_hourly(bad, r, err));
    bad = plain;
    bad.pension_match_percent = D("101");
    CHECK(!compute_real_hourly(bad, r, err));
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // keep progress visible if a test ever crashes
    try {
    test_decimal();
    test_json();
    test_evaluator();
    test_schedules();
    test_packs();
    test_golden();
    test_hashes();
    test_minisign();
    test_catalog();
    test_packstore();
    test_take_home();
    test_overtime();
    test_real_hourly();
    } catch (const std::exception& e) {
        ++g_failures;
        std::printf("  FAILED: uncaught exception: %s\n", e.what());
    }
    std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures;
}
