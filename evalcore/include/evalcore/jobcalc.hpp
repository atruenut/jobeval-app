// One job, start to finish: pay (hourly or salary, overtime by law and by company policy,
// bonus, shift premium) -> take-home via the region pack -> real hourly.
// Jurisdiction-blind: all tax and legal minimums come from the pack's bundle.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "evalcore/realhourly.hpp"
#include "evalcore/takehome.hpp"

namespace jobeval {

enum class PayBasis { Hourly, Salary };

// How overtime is paid.
//   None    : straight time for every hour (salaried / exempt work).
//   Legal   : the region's legal minimum only (threshold and rate from the pack, if it has any).
//   Company : the employer's policy: hours beyond `company_threshold_hours` (or, when
//             `use_legal_threshold` is set, the region's threshold) pay `company_multiplier`
//             times the wage. The legal minimum still applies as a floor wherever it is higher.
enum class OvertimeMode { None, Legal, Company };

struct JobInputs {
    // Pay
    PayBasis basis = PayBasis::Hourly;
    Decimal pay;  // hourly wage, or annual salary when basis == Salary
    Decimal hours_per_week;
    Decimal paid_weeks;
    OvertimeMode overtime = OvertimeMode::Legal;
    Decimal company_multiplier;         // e.g. 1.5 or 2 (times the hourly wage)
    bool use_legal_threshold = true;    // Company mode: start overtime where the law does (if it has a threshold)
    Decimal company_threshold_hours;    // otherwise this
    Decimal bonus_annual;               // taxable
    Decimal shift_premium_per_hour;     // taxable extra pay per hour on the shifts it applies to
    Decimal shift_premium_percent;      // % of paid hours it applies to (0-100)

    // Time off, schedule
    Decimal paid_vacation_weeks;
    Decimal paid_days_off;
    Decimal days_per_week;
    Decimal unpaid_minutes_per_day;

    // Costs and benefits (see realhourly.hpp)
    Decimal commute_distance, commute_minutes, cost_per_distance, daily_parking_and_tolls, daily_transit_fare;
    Decimal work_costs_annual, benefits_monthly_value, benefit_premiums_monthly, pension_match_percent,
        other_benefits_annual;

    std::string as_of;
    std::map<std::string, Value> extra;  // pack-declared inputs
};

struct JobOutcome {
    Decimal hourly_wage;  // given, or derived from salary / (hours x paid weeks)
    TakeHomeResult take_home;
    RealHourlyResult real;
};

bool evaluate_job(const std::shared_ptr<const Bundle>& bundle, const JobInputs& in, JobOutcome& out,
                  std::vector<TraceEntry>& trace, std::string& error);

}  // namespace jobeval
