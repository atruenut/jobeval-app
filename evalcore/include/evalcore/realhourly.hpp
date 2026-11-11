// The "real hourly" figure: what a job pays per hour of your life it actually consumes.
//
//   value = take-home pay + employer benefits - commuting costs - work-related costs
//   time  = hours worked + commuting hours + unpaid time at work (breaks, changing, waiting)
//   real hourly = value / time
//
// Pure arithmetic on user-supplied numbers; it knows nothing about any jurisdiction. The
// take-home figure comes from takehome.hpp. All amounts are annual unless noted, in the
// pack's currency; distances are in whatever unit the per-distance cost uses (km or mi).
#pragma once

#include <string>

#include "evalcore/decimal.hpp"

namespace jobeval {

struct RealHourlyInput {
    // From the take-home calculation.
    Decimal net_annual;
    Decimal gross_annual;
    Decimal hours_per_week;  // paid hours per week
    Decimal paid_weeks;      // weeks paid per year (including paid vacation)

    // Schedule.
    Decimal paid_vacation_weeks;  // paid weeks in which you do not work (no commute, no hours)
    Decimal days_per_week;        // days actually worked per working week
    Decimal unpaid_minutes_per_day;  // unpaid breaks, changing, waiting for shifts

    // Getting there (one way).
    Decimal commute_distance;      // km or mi
    Decimal commute_minutes;       // door to door
    Decimal cost_per_distance;     // all-in vehicle cost per km/mi: fuel, upkeep, tyres, depreciation, insurance share
    Decimal daily_parking_and_tolls;
    Decimal daily_transit_fare;

    // Other money.
    Decimal work_costs_annual;       // uniforms, tools, phone, licences you pay for
    Decimal benefits_monthly_value;  // employer-paid health/dental/etc., per month
    Decimal pension_match_percent;   // employer pension/RRSP match as % of gross (0-100)
    Decimal other_benefits_annual;   // anything else with a cash value
};

struct RealHourlyResult {
    Decimal weeks_worked;
    Decimal days_worked;

    Decimal hours_worked;       // paid hours actually worked
    Decimal hours_paid;         // including paid vacation
    Decimal commute_hours;
    Decimal unpaid_hours;
    Decimal total_hours;        // everything the job consumes

    Decimal commute_distance_total;
    Decimal vehicle_cost;
    Decimal parking_and_tolls;
    Decimal transit_cost;
    Decimal commute_cost;       // sum of the three above
    Decimal work_costs;
    Decimal benefits_value;     // health etc. + pension match + other
    Decimal pension_match_value;

    Decimal value_annual;       // net + benefits - commute cost - work costs
    Decimal take_home_per_hour_paid;    // net / hours_paid
    Decimal take_home_per_hour_worked;  // net / hours_worked
    Decimal real_hourly;                // value / total_hours
    Decimal hidden_cost_share;          // (headline take-home/h - real hourly) / headline; negative when benefits outweigh costs
};

// Returns false and sets `error` for invalid inputs (negative numbers, more vacation weeks
// than paid weeks, more than 7 days a week, pension match over 100%).
bool compute_real_hourly(const RealHourlyInput& in, RealHourlyResult& out, std::string& error);

}  // namespace jobeval
