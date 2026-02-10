#ifndef REFTYPE_FM_ROUNDING_HPP
#define REFTYPE_FM_ROUNDING_HPP

#include <reftype/fm/types.hpp>

namespace reftype::fm {

consteval double ceil_val(double x) {
    double truncated = static_cast<double>(static_cast<long long>(x));
    if (x > truncated) return truncated + 1.0;
    return truncated;
}

consteval double floor_val(double x) {
    double truncated = static_cast<double>(static_cast<long long>(x));
    if (x < truncated) return truncated - 1.0;
    return truncated;
}

// Assumes values are small enough to fit in long long without precision loss.
// Safe for refinement-type constants (small integers, simple fractions).
consteval bool is_integer_val(double x) {
    return x == static_cast<double>(static_cast<long long>(x));
}

// Round a bound inequality's constant for integer variable elimination.
// is_lower: true for lower bound (coeff > 0), false for upper bound (coeff < 0).
// Only modifies constant and strict flag; coefficients unchanged.
//
// Assumes LHS is integer-valued (guaranteed by the no-mixing constraint).
// Does not normalize by the target variable's coefficient — e.g., 2x >= 5
// is not tightened to 2x >= 6 (which would require divisibility reasoning).
// This is sound but incomplete: FM + rounding may report SAT for systems
// that are UNSAT due to coefficient divisibility (e.g., 2x = 5).
consteval LinearInequality round_integer_bound(
    LinearInequality ineq, bool is_lower) {
    if (is_lower) {
        // LHS + constant >= 0 means LHS >= -constant
        double bound = -ineq.constant;
        if (ineq.strict && is_integer_val(bound))
            ineq.constant = -(bound + 1.0);  // x > 2 → x >= 3
        else
            ineq.constant = -ceil_val(bound); // x >= 2.5 → x >= 3
    } else {
        // -LHS + constant >= 0 means LHS <= constant
        if (ineq.strict && is_integer_val(ineq.constant))
            ineq.constant -= 1.0;             // x < 3 → x <= 2
        else
            ineq.constant = floor_val(ineq.constant); // x <= 3.5 → x <= 3
    }
    ineq.strict = false;
    return ineq;
}

} // namespace reftype::fm

#endif // REFTYPE_FM_ROUNDING_HPP
