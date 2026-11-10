// Exact fixed-point decimal used for every monetary and rate calculation.
//
// Values are stored as a signed 128-bit integer scaled by 1e9 (nine decimal
// places). |value| must stay below 1e9 so that a product of two values fits
// in 128 bits before rescaling. Binary floating point is never used for
// money: 0.1 + 0.2 == 0.3 here.
#pragma once

#include <stdexcept>
#include <string>

namespace jobeval {

struct EvalError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class RoundMode { HalfUp, HalfEven, Down, Up, Floor, Ceil };

// Accepts: half_up, half_even, down, up, floor, ceil.
bool parse_round_mode(const std::string& name, RoundMode& out);

__extension__ typedef __int128 Int128;

class Decimal {
public:
    using Raw = Int128;
    static constexpr int kScale = 9;
    static constexpr Raw kOne = 1000000000;
    static constexpr Raw kLimit = kOne * 1000000000;  // |raw| < kLimit

    Decimal() = default;

    static Decimal from_int(long long n);
    static Decimal from_raw(Raw raw);
    // Strict parse: [+-]digits[.digits][e[+-]digits]. More than nine
    // significant decimal places is an error rather than a silent rounding.
    static Decimal parse(const std::string& text);
    static bool try_parse(const std::string& text, Decimal& out, std::string* error = nullptr);

    Raw raw() const { return raw_; }
    bool is_zero() const { return raw_ == 0; }
    bool is_negative() const { return raw_ < 0; }

    // Canonical text, trailing zeros trimmed ("2766.75", "0", "-0.0595").
    std::string to_string() const;
    // Rounded half away from zero to `places` (0..9) decimals, always showing them.
    std::string to_fixed(int places) const;
    double to_double() const;  // display/plotting only, never for money maths

    Decimal operator-() const;
    Decimal abs() const;
    Decimal floor() const;
    Decimal ceil() const;
    // Round to a multiple of `step` (> 0), e.g. step 0.01 for cents.
    Decimal round_to(const Decimal& step, RoundMode mode) const;

    friend Decimal operator+(const Decimal& a, const Decimal& b);
    friend Decimal operator-(const Decimal& a, const Decimal& b);
    friend Decimal operator*(const Decimal& a, const Decimal& b);  // rounds half away from zero
    friend Decimal operator/(const Decimal& a, const Decimal& b);  // throws on /0

    friend bool operator==(const Decimal& a, const Decimal& b) { return a.raw_ == b.raw_; }
    friend bool operator!=(const Decimal& a, const Decimal& b) { return a.raw_ != b.raw_; }
    friend bool operator<(const Decimal& a, const Decimal& b) { return a.raw_ < b.raw_; }
    friend bool operator<=(const Decimal& a, const Decimal& b) { return a.raw_ <= b.raw_; }
    friend bool operator>(const Decimal& a, const Decimal& b) { return a.raw_ > b.raw_; }
    friend bool operator>=(const Decimal& a, const Decimal& b) { return a.raw_ >= b.raw_; }

private:
    Raw raw_ = 0;
};

inline Decimal min(const Decimal& a, const Decimal& b) { return b < a ? b : a; }
inline Decimal max(const Decimal& a, const Decimal& b) { return a < b ? b : a; }

}  // namespace jobeval
