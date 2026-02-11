#ifndef REFTYPE_FM_DISJUNCTION_HPP
#define REFTYPE_FM_DISJUNCTION_HPP

#include <reftype/fm/eliminate.hpp>
#include <reftype/fm/parser.hpp>

namespace reftype::fm {

// Negate a single inequality.
// (sum + c >= 0) becomes (-sum - c > 0)
// (sum + c > 0)  becomes (-sum - c >= 0)
consteval LinearInequality negate_inequality(LinearInequality ineq) {
    for (std::size_t i = 0; i < ineq.term_count; ++i)
        ineq.terms[i].coeff = -ineq.terms[i].coeff;
    ineq.constant = -ineq.constant;
    ineq.strict = !ineq.strict;
    return ineq;
}

// Check if clause A implies clause B.
// A => B iff for every inequality b_i in B, A ∧ ¬b_i is UNSAT.
// This avoids DNF explosion: instead of negating B as a whole
// (which produces a disjunction), we test each inequality individually.
template <std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval bool clause_implies(const InequalitySystem<MaxIneqs, MaxVars>& a,
                              const InequalitySystem<MaxIneqs, MaxVars>& b) {
    // Guard: var_ids must refer to the same variables in both systems.
    const auto& smaller = (a.vars.count <= b.vars.count) ? a.vars : b.vars;
    const auto& larger = (a.vars.count <= b.vars.count) ? b.vars : a.vars;
    for (std::size_t i = 0; i < smaller.count; ++i) {
        auto found = larger.find(smaller.names[i]);
        if (!found.has_value() || found.value() != static_cast<int>(i))
            throw "clause_implies: incompatible variable orderings";
    }

    for (std::size_t i = 0; i < b.count; ++i) {
        auto test = a.add(negate_inequality(b.ineqs[i]));
        if (!fm_is_unsat(test))
            return false;
    }
    return true;
}

// Remove trivially UNSAT clauses from a DNF.
// In a disjunction, UNSAT clauses contribute nothing (false || X = X).
template <std::size_t MaxClauses = 8, std::size_t MaxIneqs = 64,
          std::size_t MaxVars = 16>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
remove_unsat_clauses(const ParseResult<MaxClauses, MaxIneqs, MaxVars>& result) {
    ParseResult<MaxClauses, MaxIneqs, MaxVars> r{};
    for (std::size_t i = 0; i < result.clause_count; ++i) {
        if (!fm_is_unsat(result.clauses[i])) {
            r.clauses[r.clause_count++] = result.clauses[i];
        }
    }
    return r;
}

// Remove subsumed clauses from a DNF.
// If clause_implies(C_i, C_j) (C_i's solutions ⊆ C_j's solutions),
// then C_i is redundant in the disjunction and can be dropped.
//
// Quadratic in the number of clauses — acceptable since MaxClauses
// is typically small (≤ 8).
template <std::size_t MaxClauses = 8, std::size_t MaxIneqs = 64,
          std::size_t MaxVars = 16>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
remove_subsumed_clauses(
    const ParseResult<MaxClauses, MaxIneqs, MaxVars>& result) {
    bool subsumed[MaxClauses]{};

    for (std::size_t i = 0; i < result.clause_count; ++i) {
        if (subsumed[i])
            continue;
        for (std::size_t j = 0; j < result.clause_count; ++j) {
            if (i == j || subsumed[j])
                continue;
            // If C_j implies C_i, then C_j ⊆ C_i → drop C_j
            if (clause_implies(result.clauses[j], result.clauses[i]))
                subsumed[j] = true;
        }
    }

    ParseResult<MaxClauses, MaxIneqs, MaxVars> r{};
    for (std::size_t i = 0; i < result.clause_count; ++i) {
        if (!subsumed[i])
            r.clauses[r.clause_count++] = result.clauses[i];
    }
    return r;
}

// Simplify a DNF: remove UNSAT clauses, then remove subsumed clauses.
template <std::size_t MaxClauses = 8, std::size_t MaxIneqs = 64,
          std::size_t MaxVars = 16>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
simplify_dnf(const ParseResult<MaxClauses, MaxIneqs, MaxVars>& result) {
    auto cleaned = remove_unsat_clauses(result);
    return remove_subsumed_clauses(cleaned);
}

} // namespace reftype::fm

#endif // REFTYPE_FM_DISJUNCTION_HPP
