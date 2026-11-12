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
    o.taxable_annual = o.weekly_benefit * Decimal::from_int(52) + in.other_taxable_monthly * twelve;

    TakeHomeInput t;
    t.as_of = in.as_of;
    t.extra = extra;
    t.use_statutory_overtime = false;
    t.other_income_annual = o.taxable_annual;
    if (!compute_take_home(bundle, t, o.take_home, trace, error)) return false;

    o.net_annual = o.take_home.net_annual + in.tax_free_monthly * twelve;
    o.net_monthly = o.net_annual / twelve;
    o.activity_hours_year = in.activity_hours_per_week * Decimal::from_int(52);
    if (in.monthly_expenses > o.net_monthly) {
        o.monthly_shortfall = in.monthly_expenses - o.net_monthly;
        o.runway_unlimited = false;
        o.runway_months = o.monthly_shortfall.is_zero() ? Decimal() : in.savings / o.monthly_shortfall;
    }
    out = std::move(o);
    return true;
}

}  // namespace jobeval
