#include "evalcore/realhourly.hpp"

namespace jobeval {

bool compute_real_hourly(const RealHourlyInput& in, RealHourlyResult& out, std::string& error) {
    const Decimal* nonneg[] = {&in.hours_per_week, &in.paid_weeks, &in.paid_vacation_weeks, &in.paid_days_off, &in.benefit_premiums_monthly, &in.days_per_week, &in.remote_days_per_week,
                               &in.unpaid_minutes_per_day, &in.commute_distance, &in.commute_minutes,
                               &in.cost_per_distance, &in.daily_parking_and_tolls, &in.daily_transit_fare,
                               &in.work_costs_annual, &in.benefits_monthly_value, &in.pension_match_percent,
                               &in.other_benefits_annual};
    for (const Decimal* d : nonneg) {
        if (d->is_negative()) {
            error = "values cannot be negative";
            return false;
        }
    }
    if (in.paid_vacation_weeks > in.paid_weeks) {
        error = "paid vacation weeks cannot exceed paid weeks per year";
        return false;
    }
    if (in.remote_days_per_week > in.days_per_week) {
        error = "remote days cannot exceed days worked per week";
        return false;
    }
    if (in.days_per_week > Decimal::from_int(7)) {
        error = "days per week cannot exceed 7";
        return false;
    }
    if (!in.days_per_week.is_zero() && in.hours_per_week > in.days_per_week * Decimal::from_int(24)) {
        error = "hours per week cannot exceed 24 hours on each day worked";
        return false;
    }
    if (in.pension_match_percent > Decimal::from_int(100)) {
        error = "pension match cannot exceed 100%";
        return false;
    }

    const Decimal sixty = Decimal::from_int(60);
    const Decimal two = Decimal::from_int(2);
    const Decimal twelve = Decimal::from_int(12);
    RealHourlyResult r;

    r.weeks_worked = in.paid_weeks - in.paid_vacation_weeks;
    const Decimal scheduled_days = in.days_per_week * r.weeks_worked;
    if (in.paid_days_off > scheduled_days) {
        error = "paid days off exceed the days you are scheduled to work";
        return false;
    }
    r.days_worked = scheduled_days - in.paid_days_off;
    r.hours_paid = in.hours_per_week * in.paid_weeks;
    r.hours_worked = in.hours_per_week * r.weeks_worked;
    if (!in.days_per_week.is_zero()) r.hours_worked = r.hours_worked - in.paid_days_off * in.hours_per_week / in.days_per_week;
    r.commute_days = in.days_per_week.is_zero() ? Decimal() : r.days_worked * (in.days_per_week - in.remote_days_per_week) / in.days_per_week;
    r.commute_hours = two * in.commute_minutes * r.commute_days / sixty;
    r.unpaid_hours = in.unpaid_minutes_per_day * r.days_worked / sixty;
    r.total_hours = r.hours_worked + r.commute_hours + r.unpaid_hours;

    r.commute_distance_total = two * in.commute_distance * r.commute_days;
    r.vehicle_cost = r.commute_distance_total * in.cost_per_distance;
    r.parking_and_tolls = in.daily_parking_and_tolls * r.commute_days;
    r.transit_cost = in.daily_transit_fare * r.commute_days;
    r.commute_cost = r.vehicle_cost + r.parking_and_tolls + r.transit_cost;
    r.work_costs = in.work_costs_annual;
    r.benefit_premiums = in.benefit_premiums_monthly * twelve;
    r.pension_match_value = in.gross_annual * in.pension_match_percent / Decimal::from_int(100);
    r.benefits_value = in.benefits_monthly_value * twelve + r.pension_match_value + in.other_benefits_annual;

    r.value_annual = in.net_annual + r.benefits_value - r.commute_cost - r.work_costs - r.benefit_premiums;
    if (!r.hours_paid.is_zero()) r.take_home_per_hour_paid = in.net_annual / r.hours_paid;
    if (!r.hours_worked.is_zero()) r.take_home_per_hour_worked = in.net_annual / r.hours_worked;
    if (!r.total_hours.is_zero()) r.real_hourly = r.value_annual / r.total_hours;
    if (!r.take_home_per_hour_paid.is_zero() && r.take_home_per_hour_paid > Decimal()) {
        r.hidden_cost_share = (r.take_home_per_hour_paid - r.real_hourly) / r.take_home_per_hour_paid;
    }
    out = r;
    return true;
}

}  // namespace jobeval
