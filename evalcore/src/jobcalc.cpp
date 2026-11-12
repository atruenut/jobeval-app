#include "evalcore/jobcalc.hpp"

namespace jobeval {

bool evaluate_job(const std::shared_ptr<const Bundle>& bundle, const JobInputs& in, JobOutcome& out,
                  std::vector<TraceEntry>& trace, std::string& error) {
    const Decimal hundred = Decimal::from_int(100);
    if (in.pay.is_negative() || in.bonus_annual.is_negative() || in.shift_premium_per_hour.is_negative() ||
        in.company_multiplier.is_negative() || in.company_threshold_hours.is_negative() || in.signing_bonus.is_negative() ||
        in.expected_years.is_negative()) {
        error = "pay, bonus, premium and overtime values cannot be negative";
        return false;
    }
    if (in.shift_premium_percent.is_negative() || in.shift_premium_percent > hundred) {
        error = "shift premium share must be between 0 and 100%";
        return false;
    }
    JobOutcome o;
    const Decimal hours_paid = in.hours_per_week * in.paid_weeks;

    TakeHomeInput t;
    t.hours_per_week = in.hours_per_week;
    t.weeks_per_year = in.paid_weeks;
    t.as_of = in.as_of;
    t.extra = in.extra;
    if (in.basis == PayBasis::Salary) {
        o.hourly_wage = hours_paid.is_zero() ? Decimal() : in.pay / hours_paid;
        t.has_base_pay_override = true;
        t.base_pay_annual = in.pay;
        t.use_statutory_overtime = false;  // salary is a flat sum; overtime does not apply
    } else {
        o.hourly_wage = in.pay;
        t.use_statutory_overtime = in.overtime != OvertimeMode::None;
        if (in.overtime == OvertimeMode::Company && !in.company_multiplier.is_zero()) {
            Decimal threshold = in.company_threshold_hours;
            if (in.use_legal_threshold) {
                // Resolve the region's threshold by asking for it through a legal-only run.
                TakeHomeInput probe = t;
                probe.hourly_wage = o.hourly_wage;
                TakeHomeResult pr;
                std::vector<TraceEntry> ignored;
                std::string perr;
                if (compute_take_home(bundle, probe, pr, ignored, perr) && !pr.legal_overtime_threshold.is_zero())
                    threshold = pr.legal_overtime_threshold;
            }
            t.company_overtime.push_back({threshold, o.hourly_wage * in.company_multiplier});
        }
    }
    t.hourly_wage = o.hourly_wage;
    t.other_pay_annual = in.bonus_annual + in.shift_premium_per_hour * in.shift_premium_percent / hundred * hours_paid;
    if (!in.signing_bonus.is_zero() && !in.expected_years.is_zero()) t.other_pay_annual = t.other_pay_annual + in.signing_bonus / in.expected_years;

    if (!compute_take_home(bundle, t, o.take_home, trace, error)) return false;

    RealHourlyInput r;
    r.net_annual = o.take_home.net_annual;
    r.gross_annual = o.take_home.gross_annual;
    r.hours_per_week = in.hours_per_week;
    r.paid_weeks = in.paid_weeks;
    r.paid_vacation_weeks = in.paid_vacation_weeks;
    r.paid_days_off = in.paid_days_off;
    r.days_per_week = in.days_per_week;
    r.remote_days_per_week = in.remote_days_per_week;
    r.unpaid_minutes_per_day = in.unpaid_minutes_per_day;
    r.commute_distance = in.commute_distance;
    r.commute_minutes = in.commute_minutes;
    r.cost_per_distance = in.cost_per_distance;
    r.daily_parking_and_tolls = in.daily_parking_and_tolls;
    r.daily_transit_fare = in.daily_transit_fare;
    r.work_costs_annual = in.work_costs_annual;
    r.benefits_monthly_value = in.benefits_monthly_value;
    r.benefit_premiums_monthly = in.benefit_premiums_monthly;
    r.pension_match_percent = in.pension_match_percent;
    r.other_benefits_annual = in.other_benefits_annual;
    if (!compute_real_hourly(r, o.real, error)) return false;
    out = std::move(o);
    return true;
}

bool solve_pay_for_real_hourly(const std::shared_ptr<const Bundle>& bundle, const JobInputs& base, const Decimal& target,
                               const Decimal& pay_ceiling, Decimal& pay_needed, std::string& error) {
    auto real_at = [&](const Decimal& pay, Decimal& real) {
        JobInputs j = base;
        j.pay = pay;
        JobOutcome o;
        std::vector<TraceEntry> trace;
        return evaluate_job(bundle, j, o, trace, error) ? (real = o.real.real_hourly, true) : false;
    };
    Decimal lo, hi = pay_ceiling, r;
    if (!real_at(hi, r)) return false;
    if (r < target) {
        error = "the target is not reachable within the pay ceiling";
        return false;
    }
    if (!real_at(lo, r)) return false;
    if (r >= target) {
        pay_needed = lo;
        return true;
    }
    const Decimal half = Decimal::parse("0.5");
    const Decimal tolerance = Decimal::parse("0.001");
    for (int i = 0; i < 80 && (hi - lo) > tolerance; ++i) {
        const Decimal mid = (lo + hi) * half;
        if (!real_at(mid, r)) return false;
        if (r < target) lo = mid;
        else hi = mid;
    }
    pay_needed = hi;
    return true;
}

}  // namespace jobeval
