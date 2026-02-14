#ifndef REFTYPE_FM_ELIMINATE_HPP
#define REFTYPE_FM_ELIMINATE_HPP

#include <reftype/fm/rounding.hpp>
#include <reftype/fm/types.hpp>

namespace reftype::fm {

// Combine two inequalities to eliminate a variable.
// Given lower bound (var has positive coeff) and upper bound (var has negative
// coeff), produce a new inequality without that variable.
consteval LinearInequality combine_bounds(const LinearInequality& lower,
                                          double lower_coeff,
                                          const LinearInequality& upper,
                                          double upper_abs_coeff, int var_id) {

    // After normalization: lower gives  x >= (-other_lower_terms -
    // lower.constant)
    //                      upper gives  x <= ( other_upper_terms +
    //                      upper.constant)
    // Combined: multiply lower by upper_abs_coeff, upper by lower_coeff, then
    // add. This cancels the target variable and preserves the direction (>= 0).

    LinearInequality result{};
    result.strict = lower.strict || upper.strict;

    // Scale and merge terms from both inequalities, skipping the eliminated
    // variable We collect terms, combining coefficients for the same var_id.

    // First, add scaled terms from lower
    for (std::size_t i = 0; i < lower.term_count; ++i) {
        if (lower.terms[i].var_id == var_id)
            continue;
        double scaled = lower.terms[i].coeff * upper_abs_coeff;
        // Check if this var already exists in result
        bool found = false;
        for (std::size_t j = 0; j < result.term_count; ++j) {
            if (result.terms[j].var_id == lower.terms[i].var_id) {
                result.terms[j].coeff += scaled;
                found = true;
                break;
            }
        }
        if (!found) {
            if (result.term_count >= MaxTermsPerIneq)
                throw "combine_bounds: too many terms";
            result.terms[result.term_count++] = {lower.terms[i].var_id, scaled};
        }
    }

    // Then, add scaled terms from upper
    for (std::size_t i = 0; i < upper.term_count; ++i) {
        if (upper.terms[i].var_id == var_id)
            continue;
        double scaled = upper.terms[i].coeff * lower_coeff;
        bool found = false;
        for (std::size_t j = 0; j < result.term_count; ++j) {
            if (result.terms[j].var_id == upper.terms[i].var_id) {
                result.terms[j].coeff += scaled;
                found = true;
                break;
            }
        }
        if (!found) {
            if (result.term_count >= MaxTermsPerIneq)
                throw "combine_bounds: too many terms";
            result.terms[result.term_count++] = {upper.terms[i].var_id, scaled};
        }
    }

    result.constant =
        lower.constant * upper_abs_coeff + upper.constant * lower_coeff;

    // Prune zero-coefficient terms to avoid wasting capacity across elimination
    // rounds
    std::size_t write = 0;
    for (std::size_t i = 0; i < result.term_count; ++i) {
        if (result.terms[i].coeff != 0.0)
            result.terms[write++] = result.terms[i];
    }
    result.term_count = write;

    return result;
}

// Eliminate a single variable from the system.
// Partitions inequalities into lower bounds, upper bounds, and unrelated,
// then combines each (lower, upper) pair to produce a new system without
// var_id.
template <std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval InequalitySystem<MaxIneqs, MaxVars>
eliminate_variable(InequalitySystem<MaxIneqs, MaxVars> sys, int var_id) {

    if (var_id < 0 || static_cast<std::size_t>(var_id) >= sys.vars.count)
        throw "eliminate_variable: var_id out of range";

    InequalitySystem<MaxIneqs, MaxVars> result{};
    result.vars = sys.vars;

    // Partition: find indices and coefficients for lower/upper/unrelated
    std::size_t lower_idx[MaxIneqs]{};
    double lower_coeff[MaxIneqs]{};
    std::size_t lower_count = 0;

    std::size_t upper_idx[MaxIneqs]{};
    double upper_abs_coeff[MaxIneqs]{};
    std::size_t upper_count = 0;

    for (std::size_t i = 0; i < sys.count; ++i) {
        double coeff = 0.0;
        for (std::size_t t = 0; t < sys.ineqs[i].term_count; ++t) {
            if (sys.ineqs[i].terms[t].var_id == var_id) {
                coeff = sys.ineqs[i].terms[t].coeff;
                break;
            }
        }

        if (coeff > 0.0) {
            lower_idx[lower_count] = i;
            lower_coeff[lower_count] = coeff;
            ++lower_count;
        } else if (coeff < 0.0) {
            upper_idx[upper_count] = i;
            upper_abs_coeff[upper_count] = -coeff;
            ++upper_count;
        } else {
            // Unrelated — copy directly
            result = result.add(sys.ineqs[i]);
        }
    }

    // Integer rounding: tighten bounds before combining
    if (sys.vars.is_integer[static_cast<std::size_t>(var_id)]) {
        for (std::size_t li = 0; li < lower_count; ++li)
            sys.ineqs[lower_idx[li]] = round_integer_bound(
                sys.ineqs[lower_idx[li]], true, lower_coeff[li]);
        for (std::size_t ui = 0; ui < upper_count; ++ui)
            sys.ineqs[upper_idx[ui]] = round_integer_bound(
                sys.ineqs[upper_idx[ui]], false, upper_abs_coeff[ui]);
    }

    // Combine each (lower, upper) pair
    for (std::size_t li = 0; li < lower_count; ++li) {
        for (std::size_t ui = 0; ui < upper_count; ++ui) {
            auto combined = combine_bounds(
                sys.ineqs[lower_idx[li]], lower_coeff[li],
                sys.ineqs[upper_idx[ui]], upper_abs_coeff[ui], var_id);
            result = result.add(combined);
        }
    }

    return result;
}

// Check a constant-only system for contradictions.
// After all variables are eliminated, only constant inequalities remain.
// A contradiction is: constant < 0 (for >=) or constant <= 0 (for >).
template <std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval bool
has_contradiction(const InequalitySystem<MaxIneqs, MaxVars>& sys) {
    for (std::size_t i = 0; i < sys.count; ++i) {
        const auto& ineq = sys.ineqs[i];
        if (ineq.term_count != 0)
            throw "has_contradiction: system still has variable terms";
        if (ineq.strict && ineq.constant <= 0.0)
            return true;
        if (!ineq.strict && ineq.constant < 0.0)
            return true;
    }
    return false;
}

// Eliminate all variables and check for contradiction.
// Returns true if the system is unsatisfiable.
template <std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval bool fm_is_unsat(InequalitySystem<MaxIneqs, MaxVars> sys) {
    for (std::size_t v = 0; v < sys.vars.count; ++v)
        sys = eliminate_variable(sys, static_cast<int>(v));
    return has_contradiction(sys);
}

} // namespace reftype::fm

#endif // REFTYPE_FM_ELIMINATE_HPP
