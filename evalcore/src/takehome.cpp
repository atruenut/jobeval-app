#include "evalcore/takehome.hpp"

namespace jobeval {

namespace {

bool slot_lines(Evaluator& ev, const Bundle& b, const char* slot, std::vector<SlotLine>& lines, Decimal& total) {
    total = Decimal();
    lines.clear();
    auto it = b.slots.find(slot);
    if (it == b.slots.end()) return true;  // pack has nothing for this slot
    for (const auto& rid : it->second) {
        SlotLine l;
        l.id = rid;
        const Rule& r = b.rules.at(rid);
        l.label = r.label.empty() ? rid : r.label;
        l.amount = ev.rule(rid).as_num();
        total = total + l.amount;
        lines.push_back(std::move(l));
    }
    return true;
}

}  // namespace

bool compute_take_home(const std::shared_ptr<const Bundle>& bundle, const TakeHomeInput& in, TakeHomeResult& out,
                       std::vector<TraceEntry>& trace, std::string& error) {
    try {
        if (in.hourly_wage.is_negative() || in.hours_per_week.is_negative() || in.weeks_per_year.is_negative()) {
            error = "wage, hours and weeks cannot be negative";
            return false;
        }
        TakeHomeResult r;
        r.hours_per_year = in.hours_per_week * in.weeks_per_year;
        r.gross_annual = in.hourly_wage * r.hours_per_year;

        EvalInputs ei;
        ei.date = in.as_of;
        ei.values = in.extra;
        ei.values["gross_annual"] = Value::num(r.gross_annual);
        ei.values["wage"] = Value::num(in.hourly_wage);
        Evaluator ev(bundle, ei);

        slot_lines(ev, *bundle, "income_tax", r.income_tax_lines, r.income_tax);
        slot_lines(ev, *bundle, "contributions", r.contribution_lines, r.contributions);
        r.net_annual = r.gross_annual - r.income_tax - r.contributions;
        r.net_monthly = r.net_annual / Decimal::from_int(12);
        if (!r.hours_per_year.is_zero()) r.net_hourly = r.net_annual / r.hours_per_year;
        out = std::move(r);
        trace = ev.trace();
        return true;
    } catch (const EvalError& e) {
        error = e.what();
        return false;
    }
}

}  // namespace jobeval
