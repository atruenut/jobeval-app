#include "evalcore/scoring.hpp"

namespace jobeval {

bool weighted_fit(const std::map<std::string, Decimal>& scores, const std::map<std::string, Decimal>& weights, Decimal& out) {
    Decimal sum, total;
    for (const auto& kv : weights) {
        auto it = scores.find(kv.first);
        if (it == scores.end() || kv.second <= Decimal()) continue;
        sum = sum + it->second * kv.second;
        total = total + kv.second;
    }
    out = Decimal();
    if (total.is_zero()) return false;
    out = sum / total * Decimal::from_int(10);
    return true;
}

Decimal scale_score(const Decimal& value, const Decimal& worst, const Decimal& best) {
    const Decimal ten = Decimal::from_int(10);
    if (worst == best) return ten;
    Decimal s = (value - worst) / (best - worst) * ten;
    if (s < Decimal()) s = Decimal();
    if (s > ten) s = ten;
    return s;
}

}  // namespace jobeval
