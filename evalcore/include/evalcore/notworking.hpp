// Not-working situations as opportunities to compare against jobs: unemployment benefits,
// student life, a savings runway, a gap year, caring for family. Money comes from three places:
//   * the region's unemployment benefit (pack slot `unemployment_weekly_benefit`), taxable;
//   * other taxable income the user enters (grants that are taxed, part-time work...);
//   * tax-free income the user enters (family benefits, gifts, loans drawn).
// Taxes on the taxable part come from the pack, exactly as for a job but with no pension or
// insurance contributions. Monthly figures treat the situation as lasting a full year for tax.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "evalcore/takehome.hpp"

namespace jobeval {

struct NotWorkingInputs {
    bool use_unemployment_benefit = false;
    Decimal prior_earnings_annual;   // earnings before the claim (benefit formula input)
    Decimal benefit_weeks;           // how long it lasts, 0 = unknown / not limited
    Decimal other_taxable_monthly;
    Decimal tax_free_monthly;
    Decimal monthly_expenses;        // what living this way costs; drives the runway
    Decimal savings;                 // balance available to cover any shortfall
    Decimal activity_hours_per_week; // job search, classes, study, caring: time this situation uses
    std::string as_of;
    std::map<std::string, Value> extra;
};

struct NotWorkingOutcome {
    Decimal weekly_benefit;          // from the pack, 0 if not used or the pack has none
    bool pack_has_benefit = false;   // the region's pack defines an unemployment benefit
    Decimal taxable_annual;          // benefit + other taxable income, per year
    TakeHomeResult take_home;        // tax on the taxable part (gross pay is zero)
    Decimal net_annual;              // taxable after tax + tax-free income
    Decimal net_monthly;
    Decimal monthly_shortfall;       // expenses beyond income, 0 if income covers them
    bool runway_unlimited = true;    // income covers expenses (or none were given)
    Decimal runway_months;           // savings / shortfall when there is a shortfall
    Decimal activity_hours_year;
};

bool evaluate_not_working(const std::shared_ptr<const Bundle>& bundle, const NotWorkingInputs& in, NotWorkingOutcome& out,
                          std::vector<TraceEntry>& trace, std::string& error);

}  // namespace jobeval
