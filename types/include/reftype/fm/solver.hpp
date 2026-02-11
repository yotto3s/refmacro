#ifndef REFTYPE_FM_SOLVER_HPP
#define REFTYPE_FM_SOLVER_HPP

#include <refmacro/expr.hpp>
#include <reftype/fm/disjunction.hpp>
#include <reftype/fm/eliminate.hpp>
#include <reftype/fm/parser.hpp>

namespace reftype::fm {

// --- System-level (single conjunction) ---

// Check if a system of inequalities has no solution.
// Thin wrapper over fm_is_unsat — eliminate_variable already
// integrates integer rounding internally.
template <std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval bool is_unsat(const InequalitySystem<MaxIneqs, MaxVars>& sys) {
    return fm_is_unsat(sys);
}

template <std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval bool is_sat(const InequalitySystem<MaxIneqs, MaxVars>& sys) {
    return !is_unsat(sys);
}

// --- DNF-level (disjunction of conjunctions) ---

// A DNF formula is UNSAT iff ALL clauses are UNSAT.
// Empty DNF (zero clauses) is vacuously UNSAT (false || ... = false).
template <std::size_t MaxClauses = 8, std::size_t MaxIneqs = 64,
          std::size_t MaxVars = 16>
consteval bool
is_unsat(const ParseResult<MaxClauses, MaxIneqs, MaxVars>& result) {
    for (std::size_t i = 0; i < result.clause_count; ++i)
        if (!fm_is_unsat(result.clauses[i]))
            return false;
    return true;
}

template <std::size_t MaxClauses = 8, std::size_t MaxIneqs = 64,
          std::size_t MaxVars = 16>
consteval bool
is_sat(const ParseResult<MaxClauses, MaxIneqs, MaxVars>& result) {
    return !is_unsat(result);
}

// --- Expression-level: implication and validity ---

// Check if P => Q is valid.
// When Q is conjunctive, uses clause-by-clause checking:
// (C_1 || ... || C_n) => Q iff clause_implies(C_i, Q) for all i.
// This avoids DNF explosion from negating Q (Phase 6f optimization).
// Falls back to brute-force (P && !Q is UNSAT) when Q is disjunctive.
template <std::size_t Cap, std::size_t MaxClauses = 8,
          std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval bool
is_valid_implication(const refmacro::Expression<Cap>& premise,
                     const refmacro::Expression<Cap>& conclusion) {
    VarInfo<MaxVars> vars{};
    auto p_dnf = parse_to_system<Cap, MaxClauses, MaxIneqs>(premise, vars);
    auto q_dnf = parse_to_system<Cap, MaxClauses, MaxIneqs>(conclusion, vars);
    // Propagate final VarInfo to all P clauses (Q clauses already have it
    // since vars accumulated through both parse calls in order).
    for (std::size_t i = 0; i < p_dnf.clause_count; ++i)
        p_dnf.clauses[i].vars = vars;

    if (q_dnf.is_conjunctive()) {
        for (std::size_t i = 0; i < p_dnf.clause_count; ++i)
            if (!clause_implies(p_dnf.clauses[i], q_dnf.system()))
                return false;
        return true;
    }

    // Q is disjunctive: fall back to brute-force
    auto combined = premise && !conclusion;
    auto result = parse_to_system(combined);
    return is_unsat(result);
}

// Overload with caller-supplied VarInfo for real-valued variables.
// Takes VarInfo by value intentionally: parse_to_system mutates it to
// register discovered variables, and we must not alter the caller's copy.
template <std::size_t Cap, std::size_t MaxClauses = 8,
          std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval bool
is_valid_implication(const refmacro::Expression<Cap>& premise,
                     const refmacro::Expression<Cap>& conclusion,
                     VarInfo<MaxVars> vars) {
    auto p_dnf = parse_to_system<Cap, MaxClauses, MaxIneqs>(premise, vars);
    auto q_dnf = parse_to_system<Cap, MaxClauses, MaxIneqs>(conclusion, vars);
    for (std::size_t i = 0; i < p_dnf.clause_count; ++i)
        p_dnf.clauses[i].vars = vars;

    if (q_dnf.is_conjunctive()) {
        for (std::size_t i = 0; i < p_dnf.clause_count; ++i)
            if (!clause_implies(p_dnf.clauses[i], q_dnf.system()))
                return false;
        return true;
    }

    auto combined = premise && !conclusion;
    auto result = parse_to_system(combined, vars);
    return is_unsat(result);
}

// Check if a formula is always true (!formula is UNSAT).
template <std::size_t Cap>
consteval bool is_valid(const refmacro::Expression<Cap>& formula) {
    auto negated = !formula;
    auto result = parse_to_system(negated);
    return is_unsat(result);
}

// Overload with caller-supplied VarInfo for real-valued variables.
template <std::size_t Cap, std::size_t MaxVars = 16>
consteval bool is_valid(const refmacro::Expression<Cap>& formula,
                        VarInfo<MaxVars> vars) {
    auto negated = !formula;
    auto result = parse_to_system(negated, vars);
    return is_unsat(result);
}

} // namespace reftype::fm

#endif // REFTYPE_FM_SOLVER_HPP
