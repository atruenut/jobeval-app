// The first-cut evaluation: gross pay from wage and hours, then take-home using whatever the
// loaded pack says about income tax and contributions.
//
// Standard names the app and packs agree on:
//   inputs the app supplies : gross_annual, wage (hourly)
//   output slots            : income_tax, contributions (employee-side deductions)
// A pack without a slot simply contributes nothing for it. Any other input a pack declares
// is passed through from TakeHomeInput::extra, or falls back to the pack's default.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "evalcore/evaluator.hpp"

namespace jobeval {

struct TakeHomeInput {
    Decimal hourly_wage;
    Decimal hours_per_week;
    Decimal weeks_per_year;
    std::string as_of;  // YYYY-MM-DD, selects effective-dated parameters
    std::map<std::string, Value> extra;
};

struct SlotLine {
    std::string id;     // rule id
    std::string label;  // display label (falls back to id)
    Decimal amount;     // annual
};

struct TakeHomeResult {
    Decimal gross_annual;
    Decimal income_tax;     // annual
    Decimal contributions;  // annual
    Decimal net_annual;
    Decimal net_monthly;
    Decimal hours_per_year;
    Decimal net_hourly;  // net_annual / hours_per_year; zero when no hours are worked
    std::vector<SlotLine> income_tax_lines;
    std::vector<SlotLine> contribution_lines;
};

// Returns false and sets `error` if the pack cannot be evaluated for these inputs (for
// example no parameter value is in effect on `as_of`). `trace` lists every rule evaluated.
bool compute_take_home(const std::shared_ptr<const Bundle>& bundle, const TakeHomeInput& in, TakeHomeResult& out,
                       std::vector<TraceEntry>& trace, std::string& error);

}  // namespace jobeval
