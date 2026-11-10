// eval-cli: validation, evaluation and golden tests for jurisdiction packs.
// This is what the data repo's CI runs; the GUI links the same library.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "evalcore/evaluator.hpp"
#include "evalcore/golden.hpp"
#include "evalcore/pack.hpp"

namespace fs = std::filesystem;
using namespace jobeval;

namespace {

const char* kUsage =
    "usage:\n"
    "  eval-cli validate <packs-dir|pack.json>...         check packs (schema, references, tiers)\n"
    "  eval-cli eval <pack-id> --packs <dir> [--date YYYY-MM-DD] [--input name=value]... [--trace]\n"
    "  eval-cli test --packs <dir> <suite.json|dir>...    run golden tests\n"
    "  eval-cli functions                                 list rule-language functions\n"
    "exit status: 0 ok, 1 failures, 2 usage or I/O error\n";

bool read_file(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// Expands files and directories (recursively) into a sorted list of *.json files.
std::vector<fs::path> collect_json(const std::vector<std::string>& args) {
    std::vector<fs::path> out;
    for (const auto& a : args) {
        fs::path p(a);
        std::error_code ec;
        if (fs::is_directory(p, ec)) {
            for (auto& e : fs::recursive_directory_iterator(p, ec)) {
                if (e.is_regular_file() && e.path().extension() == ".json") out.push_back(e.path());
            }
        } else {
            out.push_back(p);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Loads every pack document under the given paths. Suites (documents with a
// "suite" key) are skipped so a directory can hold both.
bool load_packs(const std::vector<std::string>& paths, PackSet& set, Diagnostics& diag) {
    bool ok = true;
    for (const auto& p : collect_json(paths)) {
        std::string text;
        if (!read_file(p, text)) {
            diag.error(p.string(), "cannot read file");
            ok = false;
            continue;
        }
        try {
            Json doc = Json::parse(text);
            if (doc.find("suite") || doc.find("catalog")) continue;
            if (!set.add(doc, p.string(), diag)) ok = false;
        } catch (const JsonError& e) {
            diag.error(p.string(), e.what());
            ok = false;
        }
    }
    return ok;
}

int cmd_validate(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    PackSet set;
    Diagnostics diag;
    load_packs(args, set, diag);
    int bundles = 0;
    for (const auto& id : set.ids()) {
        auto b = set.bundle_for(id, diag);
        if (b) {
            ++bundles;
            std::printf("ok    %-24s tier=%-8s rules=%zu params=%zu chain=%zu\n", id.c_str(), b->tier.c_str(),
                        b->rules.size(), b->params.size(), b->chain.size());
        }
    }
    std::fputs(diag.format().c_str(), stderr);
    std::printf("%zu pack(s) loaded, %d valid, %zu error(s), %zu warning(s)\n", set.ids().size(), bundles,
                diag.error_count(), diag.warning_count());
    return diag.has_errors() ? 1 : 0;
}

int cmd_eval(const std::vector<std::string>& args) {
    std::string pack_id, packs_dir, date;
    std::vector<std::pair<std::string, std::string>> inputs;
    bool trace = false;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--packs" && i + 1 < args.size()) packs_dir = args[++i];
        else if (a == "--date" && i + 1 < args.size()) date = args[++i];
        else if (a == "--trace") trace = true;
        else if (a == "--input" && i + 1 < args.size()) {
            const std::string kv = args[++i];
            size_t eq = kv.find('=');
            if (eq == std::string::npos) {
                std::fprintf(stderr, "bad --input '%s' (want name=value)\n", kv.c_str());
                return 2;
            }
            inputs.emplace_back(kv.substr(0, eq), kv.substr(eq + 1));
        } else if (pack_id.empty() && a[0] != '-') pack_id = a;
        else {
            std::fputs(kUsage, stderr);
            return 2;
        }
    }
    if (pack_id.empty() || packs_dir.empty()) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    PackSet set;
    Diagnostics diag;
    load_packs({packs_dir}, set, diag);
    auto bundle = set.bundle_for(pack_id, diag);
    if (!bundle) {
        std::fputs(diag.format().c_str(), stderr);
        return 1;
    }
    EvalInputs in;
    in.date = date;
    for (const auto& kv : inputs) {
        Decimal d;
        if (Decimal::try_parse(kv.second, d)) in.values[kv.first] = Value::num(d);
        else in.values[kv.first] = Value::str(kv.second);
    }
    Evaluator ev(bundle, in);
    try {
        for (const auto& slot : bundle->slots) {
            std::printf("@%-20s %s\n", slot.first.c_str(), ev.slot(slot.first).to_fixed(2).c_str());
        }
        for (const auto& rid : bundle->rule_order) {
            if (rid.rfind("statutory.", 0) == 0) continue;
            const Value& v = ev.rule(rid);
            std::printf("%-24s %s\n", rid.c_str(), v.is_num() ? v.as_num().to_fixed(2).c_str() : v.describe().c_str());
        }
    } catch (const EvalError& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    if (trace) std::printf("%s\n", trace_to_json(ev.trace()).dump(2).c_str());
    return 0;
}

int cmd_test(const std::vector<std::string>& args) {
    std::string packs_dir;
    std::vector<std::string> suites;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--packs" && i + 1 < args.size()) packs_dir = args[++i];
        else suites.push_back(args[i]);
    }
    if (packs_dir.empty() || suites.empty()) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    PackSet set;
    Diagnostics diag;
    load_packs({packs_dir}, set, diag);
    if (diag.has_errors()) {
        std::fputs(diag.format().c_str(), stderr);
        return 1;
    }
    int passed = 0, failed = 0, skipped = 0;
    for (const auto& path : collect_json(suites)) {
        std::string text;
        if (!read_file(path, text)) {
            std::fprintf(stderr, "cannot read %s\n", path.string().c_str());
            return 2;
        }
        Json doc;
        try {
            doc = Json::parse(text);
        } catch (const JsonError& e) {
            std::fprintf(stderr, "%s: %s\n", path.string().c_str(), e.what());
            return 1;
        }
        if (!doc.find("suite")) continue;
        Diagnostics sd;
        SuiteResult r = run_suite(set, doc, sd);
        std::printf("suite %s (pack %s): %d passed, %d failed, %d skipped\n", r.suite.c_str(), r.pack.c_str(),
                    r.passed(), r.failed(), r.skipped());
        for (const auto& c : r.cases) {
            if (c.skipped) std::printf("  skip  %s [pending]\n", c.id.c_str());
            else if (c.passed) std::printf("  pass  %s [%s]\n", c.id.c_str(), c.status.c_str());
            else {
                std::printf("  FAIL  %s [%s]\n", c.id.c_str(), c.status.c_str());
                for (const auto& f : c.failures) std::printf("        %s\n", f.c_str());
            }
        }
        std::fputs(sd.format().c_str(), stderr);
        passed += r.passed();
        failed += r.failed();
        skipped += r.skipped();
        if (sd.has_errors()) ++failed;
    }
    std::printf("total: %d passed, %d failed, %d skipped\n", passed, failed, skipped);
    return failed > 0 ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fputs(kUsage, stderr);
        return 2;
    }
    const std::string cmd = argv[1];
    std::vector<std::string> rest(argv + 2, argv + argc);
    if (cmd == "validate") return cmd_validate(rest);
    if (cmd == "eval") return cmd_eval(rest);
    if (cmd == "test") return cmd_test(rest);
    if (cmd == "functions") {
        for (const auto& f : builtin_function_names()) std::printf("%s\n", f.c_str());
        return 0;
    }
    std::fputs(kUsage, stderr);
    return 2;
}
