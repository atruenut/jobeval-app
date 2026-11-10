#include "evalcore/decimal.hpp"

#include <algorithm>
#include <cctype>

namespace jobeval {

namespace {

using Raw = Decimal::Raw;

inline Raw habs(Raw x) { return x < 0 ? -x : x; }

Decimal checked(Raw r) {
    if (habs(r) >= Decimal::kLimit) {
        throw EvalError("number out of range (absolute value must be below 1e9)");
    }
    return Decimal::from_raw(r);
}

std::string magnitude_to_string(Raw v) {
    if (v == 0) return "0";
    std::string out;
    while (v > 0) {
        out.push_back(static_cast<char>('0' + static_cast<int>(v % 10)));
        v /= 10;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

// n / d (d > 0) rounded to an integer according to `mode`.
Raw round_div(Raw n, Raw d, RoundMode mode) {
    const bool neg = n < 0;
    const Raw m = habs(n);
    Raw q = m / d;
    const Raw r = m % d;
    bool inc = false;
    switch (mode) {
        case RoundMode::HalfUp:   inc = (r * 2 >= d); break;
        case RoundMode::HalfEven: inc = (r * 2 > d) || (r * 2 == d && (q % 2 == 1)); break;
        case RoundMode::Down:     inc = false; break;
        case RoundMode::Up:       inc = r > 0; break;
        case RoundMode::Floor:    inc = neg && r > 0; break;
        case RoundMode::Ceil:     inc = !neg && r > 0; break;
    }
    if (inc) ++q;
    return neg ? -q : q;
}

Raw pow10(int n) {
    Raw v = 1;
    for (int i = 0; i < n; ++i) v *= 10;
    return v;
}

}  // namespace

bool parse_round_mode(const std::string& name, RoundMode& out) {
    if (name == "half_up") out = RoundMode::HalfUp;
    else if (name == "half_even") out = RoundMode::HalfEven;
    else if (name == "down") out = RoundMode::Down;
    else if (name == "up") out = RoundMode::Up;
    else if (name == "floor") out = RoundMode::Floor;
    else if (name == "ceil") out = RoundMode::Ceil;
    else return false;
    return true;
}

Decimal Decimal::from_int(long long n) { return checked(static_cast<Raw>(n) * kOne); }

Decimal Decimal::from_raw(Raw raw) {
    Decimal d;
    d.raw_ = raw;
    return d;
}

bool Decimal::try_parse(const std::string& text, Decimal& out, std::string* error) {
    auto fail = [&](const std::string& msg) {
        if (error) *error = msg + ": '" + text + "'";
        return false;
    };
    size_t i = 0;
    const size_t n = text.size();
    bool neg = false;
    if (i < n && (text[i] == '+' || text[i] == '-')) {
        neg = text[i] == '-';
        ++i;
    }
    std::string digits;
    int frac_len = 0;
    bool any = false;
    while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) {
        digits.push_back(text[i++]);
        any = true;
    }
    if (i < n && text[i] == '.') {
        ++i;
        while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) {
            digits.push_back(text[i++]);
            ++frac_len;
            any = true;
        }
    }
    if (!any) return fail("not a number");
    int exp10 = 0;
    if (i < n && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        bool eneg = false;
        if (i < n && (text[i] == '+' || text[i] == '-')) {
            eneg = text[i] == '-';
            ++i;
        }
        if (i >= n || !std::isdigit(static_cast<unsigned char>(text[i]))) return fail("bad exponent");
        long e = 0;
        while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) {
            e = e * 10 + (text[i++] - '0');
            if (e > 1000) return fail("exponent too large");
        }
        exp10 = static_cast<int>(eneg ? -e : e);
    }
    if (i != n) return fail("trailing characters in number");

    // value = digits * 10^(exp10 - frac_len); raw = value * 1e9
    int shift = exp10 - frac_len + kScale;
    while (shift < 0 && !digits.empty() && digits.back() == '0') {
        digits.pop_back();
        ++shift;
    }
    if (shift < 0) {
        bool all_zero = std::all_of(digits.begin(), digits.end(), [](char c) { return c == '0'; });
        if (!all_zero) return fail("more than 9 decimal places");
        shift = 0;
        digits = "0";
    }
    Raw v = 0;
    for (char c : digits) {
        v = v * 10 + (c - '0');
        if (v >= kLimit) return fail("number out of range");
    }
    for (int k = 0; k < shift; ++k) {
        v *= 10;
        if (v >= kLimit) return fail("number out of range");
    }
    out = from_raw(neg ? -v : v);
    return true;
}

Decimal Decimal::parse(const std::string& text) {
    Decimal d;
    std::string err;
    if (!try_parse(text, d, &err)) throw EvalError(err);
    return d;
}

std::string Decimal::to_string() const {
    const Raw m = habs(raw_);
    std::string ip = magnitude_to_string(m / kOne);
    Raw frac = m % kOne;
    std::string out = (raw_ < 0 ? "-" : "") + ip;
    if (frac != 0) {
        std::string f = magnitude_to_string(frac);
        f.insert(f.begin(), static_cast<size_t>(kScale) - f.size(), '0');
        while (!f.empty() && f.back() == '0') f.pop_back();
        out += "." + f;
    }
    if (out == "-0") out = "0";
    return out;
}

std::string Decimal::to_fixed(int places) const {
    if (places < 0) places = 0;
    if (places > kScale) places = kScale;
    const Raw step = pow10(kScale - places);
    const Raw r = round_div(raw_, step, RoundMode::HalfUp) * step;
    const Raw m = habs(r);
    std::string out = magnitude_to_string(m / kOne);
    if (places > 0) {
        std::string f = magnitude_to_string(m % kOne);
        f.insert(f.begin(), static_cast<size_t>(kScale) - f.size(), '0');
        out += "." + f.substr(0, static_cast<size_t>(places));
    }
    if (r < 0) out.insert(out.begin(), '-');
    return out;
}

double Decimal::to_double() const { return static_cast<double>(raw_) / 1e9; }

Decimal Decimal::operator-() const { return from_raw(-raw_); }
Decimal Decimal::abs() const { return from_raw(habs(raw_)); }
Decimal Decimal::floor() const { return checked(round_div(raw_, kOne, RoundMode::Floor) * kOne); }
Decimal Decimal::ceil() const { return checked(round_div(raw_, kOne, RoundMode::Ceil) * kOne); }

Decimal Decimal::round_to(const Decimal& step, RoundMode mode) const {
    if (step.raw_ <= 0) throw EvalError("rounding step must be positive");
    return checked(round_div(raw_, step.raw_, mode) * step.raw_);
}

Decimal operator+(const Decimal& a, const Decimal& b) { return checked(a.raw_ + b.raw_); }
Decimal operator-(const Decimal& a, const Decimal& b) { return checked(a.raw_ - b.raw_); }

Decimal operator*(const Decimal& a, const Decimal& b) {
    return checked(round_div(a.raw_ * b.raw_, Decimal::kOne, RoundMode::HalfUp));
}

Decimal operator/(const Decimal& a, const Decimal& b) {
    if (b.raw_ == 0) throw EvalError("division by zero");
    Raw n = a.raw_ * Decimal::kOne;
    Raw d = b.raw_;
    if (d < 0) {
        n = -n;
        d = -d;
    }
    return checked(round_div(n, d, RoundMode::HalfUp));
}

}  // namespace jobeval
