#ifndef REFTYPE_FM_ROUNDING_HPP
#define REFTYPE_FM_ROUNDING_HPP

#include <limits>
#include <reftype/fm/types.hpp>

namespace reftype::fm {

consteval void check_ll_range(double x, const char* fn) {
    if (x != x)
        throw fn; // NaN: all comparisons false, would bypass range check
    constexpr auto lo =
        static_cast<double>(std::numeric_limits<long long>::min());
    constexpr auto hi =
        static_cast<double>(std::numeric_limits<long long>::max());
    if (x < lo || x > hi)
        throw fn;
}

consteval double ceil_val(double x) {
    check_ll_range(x, "ceil_val: input out of range for long long");
    double truncated = static_cast<double>(static_cast<long long>(x));
    if (x > truncated)
        return truncated + 1.0;
    return truncated;
}

consteval double floor_val(double x) {
    check_ll_range(x, "floor_val: input out of range for long long");
    double truncated = static_cast<double>(static_cast<long long>(x));
    if (x < truncated)
        return truncated - 1.0;
    return truncated;
}

consteval bool is_integer_val(double x) {
    check_ll_range(x, "is_integer_val: input out of range for long long");
    return x == static_cast<double>(static_cast<long long>(x));
}

// Round a bound inequality's constant for integer variable elimination.
// is_lower: true for lower bound (coeff > 0), false for upper bound (coeff <
// 0). target_coeff: absolute value of the target variable's coefficient. Only
// modifies constant and strict flag; coefficients unchanged.
//
// For single-variable inequalities (term_count == 1), normalizes by the
// target variable's coefficient before rounding: e.g., 2x >= 3 becomes
// x >= 1.5, rounded to x >= 2, giving 2x >= 4.
//
// For multi-variable inequalities, normalization by the target coefficient
// would be unsound (the bound depends on other variables whose combined
// value may not be a multiple of the coefficient). In this case, the
// constant is rounded as if the effective coefficient is 1.
consteval LinearInequality
round_integer_bound(LinearInequality ineq, bool is_lower, double target_coeff) {
    // For multi-variable inequalities, rounding with coeff=1 is only
    // sound when every coefficient is integer (so the weighted sum of
    // integer variables is itself integer).  If any coefficient is
    // non-integer (e.g. 0.5 from a division), the sum can be
    // fractional and rounding would over-tighten, causing false UNSAT.
    if (ineq.term_count > 1) {
        for (std::size_t i = 0; i < ineq.term_count; ++i)
            if (!is_integer_val(ineq.terms[i].coeff))
                return ineq; // skip rounding — not safe
    }

    // Only normalize by the target coefficient for single-variable
    // inequalities. For multi-variable, the other terms make the
    // effective bound variable-dependent, so normalizing is unsound.
    double coeff = (ineq.term_count == 1) ? target_coeff : 1.0;

    if (is_lower) {
        // a*x + constant >= 0 means x >= -constant/a
        double bound = -ineq.constant / coeff;
        if (ineq.strict && is_integer_val(bound))
            ineq.constant = -(bound + 1.0) * coeff; // x > 2 → x >= 3
        else
            ineq.constant = -ceil_val(bound) * coeff; // x >= 2.5 → x >= 3
    } else {
        // -a*x + constant >= 0 means x <= constant/a
        double bound = ineq.constant / coeff;
        if (ineq.strict && is_integer_val(bound))
            ineq.constant = (bound - 1.0) * coeff; // x < 3 → x <= 2
        else
            ineq.constant = floor_val(bound) * coeff; // x <= 3.5 → x <= 3
    }
    ineq.strict = false;
    return ineq;
}

} // namespace reftype::fm

#endif // REFTYPE_FM_ROUNDING_HPP
