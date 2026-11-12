#include "evalcore/notworking.hpp"

namespace jobeval {

bool evaluate_not_working(const std::shared_ptr<const Bundle>& bundle, const NotWorkingInputs& in, NotWorkingOutcome& out,
                          std::vector<TraceEntry>& trace, std::string& error) {
    const Decimal* nonneg[] = {&in.prior_earnings_annual, &in.benefit_weeks, &in.other_taxable_monthly, &in.tax_free_monthly,
                               &in.monthly_expenses, &in.savings, &in.activity_hours_per_week};
    for (const Decimal* d : nonneg) {
        if (d->is_negative()) {
            error = "values cannot be negative";
            return false;
        }
    }
    const Decimal twelve = Decimal::from_int(12);
    NotWorkingOutcome o;

    std::map<std::string, Value> extra = in.extra;
    extra["prior_earnings_annual"] = Value::num(in.prior_earnings_annual);

    // The benefit is a pack rule; ask the pack with the prior earnings supplied.
    auto slot = bundle->slots.find("unemployment_weekly_benefit");
    o.pack_has_benefit = slot != bundle->slots.end() && slot->second.size() == 1;
    if (in.use_unemployment_benefit && o.pack_has_benefit) {
        try {
            EvalInputs ei;
            ei.date = in.as_of;
            ei.values = extra;
            ei.values["gross_annual"] = Value::num(Decimal());
            ei.values["other_income"] = Value::num(Decimal());
            ei.values["wage"] = Value::num(Decimal());
            Evaluator ev(bundle, ei);
            o.weekly_benefit = ev.rule(slot->second[0]).as_num();
        } catch (const EvalError& e) {
            error = e.what();
            return false;
        }
    }
    const Decimal fifty_two = Decimal::from_int(52);
    const bool limited = in.use_unemployment_benefit && !o.weekly_benefit.is_zero() && !in.benefit_weeks.is_zero();
    // Year one: only the weeks actually paid count toward tax.
    const Decimal paid_weeks_y1 = limited && in.benefit_weeks < fifty_two ? in.benefit_weeks : fifty_two;
    o.taxable_annual = o.weekly_benefit * paid_weeks_y1 + in.other_taxable_monthly * twelve;

    auto take_home_for = [&](const Decimal& taxable, TakeHomeResult& th) {
        TakeHomeInput t;
        t.as_of = in.as_of;
        t.extra = extra;
        t.use_statutory_overtime = false;
        t.other_income_annual = taxable;
        return compute_take_home(bundle, t, th, trace, error);
    };
    if (!take_home_for(o.taxable_annual, o.take_home)) return false;
    o.net_annual = o.take_home.net_annual + in.tax_free_monthly * twelve;
    o.net_monthly = o.net_annual / twelve;

    // Phases for the runway.
    o.net_monthly_during = o.net_monthly;
    o.net_monthly_after = o.net_monthly;
    if (limited) {
        o.benefit_months = in.benefit_weeks * twelve / fifty_two;
        TakeHomeResult during, after;
        std::vector<TraceEntry> keep = trace;
        if (!take_home_for(o.weekly_benefit * fifty_two + in.other_taxable_monthly * twelve, during)) return false;
        if (!take_home_for(in.other_taxable_monthly * twelve, after)) return false;
        trace = keep;  // the explanation shows the year-one calculation
        o.net_monthly_during = during.net_annual / twelve + in.tax_free_monthly;
        o.net_monthly_after = after.net_annual / twelve + in.tax_free_monthly;
    } else {
        // No limit (or no benefit): the same income all the time. Year one equals the steady state.
        TakeHomeResult steady;
        std::vector<TraceEntry> keep = trace;
        if (!take_home_for(o.weekly_benefit * fifty_two + in.other_taxable_monthly * twelve, steady)) return false;
        trace = keep;
        o.net_monthly_during = o.net_monthly_after = steady.net_annual / twelve + in.tax_free_monthly;
    }
    o.activity_hours_year = in.activity_hours_per_week * fifty_two;

    const Decimal b1 = o.net_monthly_during - in.monthly_expenses;
    const Decimal b2 = o.net_monthly_after - in.monthly_expenses;
    if (b1.is_negative()) o.monthly_shortfall = -b1;
    if (b2.is_negative()) o.monthly_shortfall_after = -b2;
    {
        Decimal savings = in.savings;
        Decimal elapsed;
        o.runway_unlimited = true;
        if (limited) {
            if (b1.is_negative()) {
                const Decimal t = savings / (-b1);
                if (t <= o.benefit_months) {
                    o.runway_unlimited = false;
                    o.runway_months = t;
                } else {
                    savings = savings + b1 * o.benefit_months;
                    elapsed = o.benefit_months;
                }
            } else {
                savings = savings + b1 * o.benefit_months;
                elapsed = o.benefit_months;
            }
            if (o.runway_unlimited && b2.is_negative()) {
                o.runway_unlimited = false;
                o.runway_months = elapsed + savings / (-b2);
            }
        } else if (b1.is_negative()) {
            o.runway_unlimited = false;
            o.runway_months = savings / (-b1);
        }
    }
    out = std::move(o);
    return true;
}

}  // namespace jobeval
