// Weighted decision scoring: combine criteria scored 0-10 with importance weights into one
// 0-100 "fit" figure. Pure arithmetic; criteria a job has no score for are left out of both
// sums, so a missing criterion neither helps nor hurts.
#pragma once

#include <map>
#include <string>

#include "evalcore/decimal.hpp"

namespace jobeval {

// `scores`: criterion id -> 0..10. `weights`: criterion id -> importance >= 0.
// Returns false (and leaves `out` at 0) when no criterion has both a score and a positive weight.
bool weighted_fit(const std::map<std::string, Decimal>& scores, const std::map<std::string, Decimal>& weights, Decimal& out);

// Linear 0..10 score of `value` between `worst` (0) and `best` (10), clamped. Works whichever of
// the two is larger, so it serves "higher is better" and "lower is better" criteria alike.
// If worst == best every value scores 10.
Decimal scale_score(const Decimal& value, const Decimal& worst, const Decimal& best);

}  // namespace jobeval
