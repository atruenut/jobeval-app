// The first-cut evaluation: gross pay from wage and hours, then take-home using whatever the
// loaded pack says about income tax and contributions.
//
// Standard names the app and packs agree on:
//   inputs the app supplies : gross_annual, wage (hourly)
//   output slots            : income_tax, contributions (employee-side deductions);
//                             overtime_rate, overtime_threshold_hours (single rule each: the
//                             legal minimum hourly rate beyond a weekly hours threshold)
// A pack without a slot simply contributes nothing for it. Any other input a pack declares
// is passed through from TakeHomeInput::extra, or falls back to the pack's default.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "evalcore/evaluator.hpp"

namespace jobeval {

struct OvertimeTier {
    Decimal threshold_hours;  // weekly hours after which the rate applies
    Decimal hourly_rate;
};

struct TakeHomeInput {
    Decimal hourly_wage;
    Decimal hours_per_week;
    Decimal weeks_per_year;
    std::string as_of;  // YYYY-MM-DD, selects effective-dated parameters
    std::map<std::string, Value> extra;
    // Pay hours beyond the pack's weekly threshold at the pack's overtime rate (never below the
    // straight-time wage). Off for salaried or exempt work that earns no overtime.
    bool use_statutory_overtime = true;
    // Employer policy tiers, e.g. {40, 1.5 x wage} or {44, 2 x wage}. Hours beyond a tier's
    // weekly threshold earn at least its hourly rate; where tiers (including the legal one)
    // overlap, the highest applicable rate wins, so company policy can add to but never go
    // below the legal minimum.
    std::vector<OvertimeTier> company_overtime;
    // Salaried work: gross base pay is exactly this (hourly_wage is then informational).
    bool has_base_pay_override = false;
    Decimal base_pay_annual;
    // Taxable pay on top of base pay and overtime (bonus, shift premiums, commission...).
    Decimal other_pay_annual;
};

struct SlotLine {
    std::string id;     // rule id
    std::string label;  // display label (falls back to id)
    Decimal amount;     // annual
};

struct TakeHomeResult {
    Decimal gross_annual;
    bool overtime_applied = false;  // some hours fall beyond an overtime threshold
    Decimal overtime_hours_per_week;  // hours beyond the lowest threshold in force
    Decimal overtime_rate;            // average hourly rate paid for those hours
    Decimal overtime_pay_annual;      // total pay for those hours (included in gross_annual)
    Decimal other_pay_annual;         // as supplied; included in gross_annual
    Decimal legal_overtime_rate;      // the region's minimum rate (0 if the pack has none)
    Decimal legal_overtime_threshold; // the region's threshold (0 if none)
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
