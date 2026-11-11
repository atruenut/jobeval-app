#include "evalcore/takehome.hpp"

#include <algorithm>

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
        if (in.hourly_wage.is_negative() || in.hours_per_week.is_negative() || in.weeks_per_year.is_negative() ||
            in.other_pay_annual.is_negative() || in.base_pay_annual.is_negative()) {
            error = "wage, hours, weeks and pay cannot be negative";
            return false;
        }
        TakeHomeResult r;
        r.hours_per_year = in.hours_per_week * in.weeks_per_year;

        EvalInputs ei;
        ei.date = in.as_of;
        ei.values = in.extra;
        ei.values["wage"] = Value::num(in.hourly_wage);

        // Weekly pay: every hour earns the highest overtime rate whose threshold it is beyond
        // (never less than the straight-time wage); tiers come from the law (pack) and from
        // company policy.
        std::vector<OvertimeTier> tiers = in.company_overtime;
        std::vector<TraceEntry> ot_trace;
        auto rate_slot = bundle->slots.find("overtime_rate");
        auto thr_slot = bundle->slots.find("overtime_threshold_hours");
        if (rate_slot != bundle->slots.end() && thr_slot != bundle->slots.end() && rate_slot->second.size() == 1 &&
            thr_slot->second.size() == 1) {
            EvalInputs probe = ei;
            probe.values["gross_annual"] = Value::num(Decimal());
            Evaluator ot(bundle, probe);
            r.legal_overtime_threshold = ot.rule(thr_slot->second[0]).as_num();
            r.legal_overtime_rate = ot.rule(rate_slot->second[0]).as_num();
            ot_trace = ot.trace();
            if (in.use_statutory_overtime) tiers.push_back({r.legal_overtime_threshold, r.legal_overtime_rate});
        }
        Decimal weekly = in.hourly_wage * in.hours_per_week;
        {
            std::vector<Decimal> cuts;  // distinct thresholds inside (0, hours)
            for (const auto& t : tiers)
                if (t.threshold_hours > Decimal() && t.threshold_hours < in.hours_per_week) cuts.push_back(t.threshold_hours);
            std::sort(cuts.begin(), cuts.end());
            cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
            Decimal pay;
            Decimal lo;
            std::vector<Decimal> bounds = cuts;
            bounds.push_back(in.hours_per_week);
            for (const Decimal& hi : bounds) {
                Decimal rate = in.hourly_wage;
                for (const auto& t : tiers)
                    if (t.threshold_hours <= lo) rate = max(rate, t.hourly_rate);
                pay = pay + rate * (hi - lo);
                lo = hi;
            }
            if (!cuts.empty()) {
                weekly = pay;
                r.overtime_applied = true;
                r.overtime_hours_per_week = in.hours_per_week - cuts.front();
                const Decimal straight = in.hourly_wage * cuts.front();
                const Decimal ot_week = pay - straight;
                r.overtime_pay_annual = ot_week * in.weeks_per_year;
                r.overtime_rate = ot_week / r.overtime_hours_per_week;
            }
        }
        r.other_pay_annual = in.other_pay_annual;
        r.gross_annual = (in.has_base_pay_override ? in.base_pay_annual : weekly * in.weeks_per_year) + in.other_pay_annual;
        ei.values["gross_annual"] = Value::num(r.gross_annual);
        Evaluator ev(bundle, ei);

        slot_lines(ev, *bundle, "income_tax", r.income_tax_lines, r.income_tax);
        slot_lines(ev, *bundle, "contributions", r.contribution_lines, r.contributions);
        r.net_annual = r.gross_annual - r.income_tax - r.contributions;
        r.net_monthly = r.net_annual / Decimal::from_int(12);
        if (!r.hours_per_year.is_zero()) r.net_hourly = r.net_annual / r.hours_per_year;
        out = std::move(r);
        trace = ot_trace;
        for (const auto& t : ev.trace()) trace.push_back(t);
        return true;
    } catch (const EvalError& e) {
        error = e.what();
        return false;
    }
}

}  // namespace jobeval
