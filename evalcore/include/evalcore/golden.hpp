// Golden-test runner: checks a pack's computed numbers against expected
// values (from official calculators or independent arithmetic).
#pragma once

#include <string>
#include <vector>

#include "evalcore/pack.hpp"

namespace jobeval {

struct CaseResult {
    std::string id;
    std::string status;  // official | derived | pending
    bool skipped = false;
    bool passed = false;
    std::vector<std::string> failures;
};

struct SuiteResult {
    std::string suite;
    std::string pack;
    std::vector<CaseResult> cases;
    int passed() const;
    int failed() const;
    int skipped() const;
};

// Suite document:
//   { "suite": "...", "pack": "CA-NB-2026", "tolerance": {"abs": "0.50"},
//     "cases": [ { "id", "status", "inputs": {..}, "expect": { "<rule>|@<slot>": N | null } } ] }
// Cases with status "pending" (or a null expectation) are skipped, never passed.
SuiteResult run_suite(const PackSet& packs, const Json& suite, Diagnostics& diag);

}  // namespace jobeval
