#include <gtest/gtest.h>
#include <refmacro/math.hpp>
#include <reftype/fm/solver.hpp>

using namespace reftype::fm;
using Expression = refmacro::Expression<>;

// ============================================================
// is_unsat / is_sat on InequalitySystem
// ============================================================

TEST(IsUnsat, EmptySystem) {
    constexpr InequalitySystem<> sys{};
    static_assert(!is_unsat(sys));
    static_assert(is_sat(sys));
}

TEST(IsUnsat, SatisfiableSystem) {
    // x >= 0 && x <= 5
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 5.0));
    }();
    static_assert(!is_unsat(sys));
    static_assert(is_sat(sys));
}

TEST(IsUnsat, UnsatisfiableSystem) {
    // x >= 5 && x <= 3
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, -5.0))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 3.0));
    }();
    static_assert(is_unsat(sys));
    static_assert(!is_sat(sys));
}

TEST(IsUnsat, ConstantContradiction) {
    // -1 >= 0 → UNSAT
    constexpr auto sys = InequalitySystem<>{}.add(
        LinearInequality::make({}, -1.0));
    static_assert(is_unsat(sys));
}

TEST(IsUnsat, StrictBoundsUNSAT) {
    // x > 0 && x < 0
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 0.0, true));
    }();
    static_assert(is_unsat(sys));
}

// ============================================================
// is_unsat / is_sat on ParseResult (DNF)
// ============================================================

TEST(IsUnsatDNF, SingleSatClause) {
    // One clause: x >= 0 → SAT
    constexpr auto result = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        s = s.add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0));
        return single_clause(s);
    }();
    static_assert(!is_unsat(result));
    static_assert(is_sat(result));
}

TEST(IsUnsatDNF, SingleUnsatClause) {
    // One clause: x > 0 && x < 0 → UNSAT
    constexpr auto result = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        s = s.add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true))
              .add(LinearInequality::make({LinearTerm{x, -1.0}}, 0.0, true));
        return single_clause(s);
    }();
    static_assert(is_unsat(result));
    static_assert(!is_sat(result));
}

TEST(IsUnsatDNF, OneSatOneUnsat) {
    // (x > 0 && x < 0) || (x >= 0) → SAT (second clause is satisfiable)
    constexpr auto result = [] {
        InequalitySystem<> s1{};
        int x1 = s1.vars.find_or_add("x");
        s1 = s1.add(LinearInequality::make({LinearTerm{x1, 1.0}}, 0.0, true))
               .add(LinearInequality::make({LinearTerm{x1, -1.0}}, 0.0, true));
        InequalitySystem<> s2{};
        int x2 = s2.vars.find_or_add("x");
        s2 = s2.add(LinearInequality::make({LinearTerm{x2, 1.0}}, 0.0));
        ParseResult<> r{};
        r.clauses[0] = s1;
        r.clauses[1] = s2;
        r.clause_count = 2;
        return r;
    }();
    static_assert(!is_unsat(result));
    static_assert(is_sat(result));
}

TEST(IsUnsatDNF, AllClausesUnsat) {
    // (x > 5 && x < 3) || (x > 10 && x < 8) → UNSAT (both clauses UNSAT)
    constexpr auto result = [] {
        InequalitySystem<> s1{};
        int x1 = s1.vars.find_or_add("x");
        s1 = s1.add(LinearInequality::make({LinearTerm{x1, 1.0}}, -5.0, true))
               .add(LinearInequality::make({LinearTerm{x1, -1.0}}, 3.0, true));
        InequalitySystem<> s2{};
        int x2 = s2.vars.find_or_add("x");
        s2 = s2.add(LinearInequality::make({LinearTerm{x2, 1.0}}, -10.0, true))
               .add(LinearInequality::make({LinearTerm{x2, -1.0}}, 8.0, true));
        ParseResult<> r{};
        r.clauses[0] = s1;
        r.clauses[1] = s2;
        r.clause_count = 2;
        return r;
    }();
    static_assert(is_unsat(result));
    static_assert(!is_sat(result));
}

// ============================================================
// is_valid_implication — Expression level
// ============================================================

TEST(IsValidImplication, StrongerPremise) {
    // (x > 0 && x < 10) => (x >= 0) — true
    static constexpr auto P =
        (Expression::var("x") > 0.0) && (Expression::var("x") < 10.0);
    static constexpr auto Q = Expression::var("x") >= 0.0;
    static_assert(is_valid_implication(P, Q));
}

TEST(IsValidImplication, WeakerConclusion) {
    // (x > 0) => (x > 5) — false (x = 1 is counterexample)
    static constexpr auto P = Expression::var("x") > 0.0;
    static constexpr auto Q = Expression::var("x") > 5.0;
    static_assert(!is_valid_implication(P, Q));
}

TEST(IsValidImplication, EqualityImpliesBound) {
    // (x == 3) => (x >= 0) — true
    static constexpr auto P = Expression::var("x") == 3.0;
    static constexpr auto Q = Expression::var("x") >= 0.0;
    static_assert(is_valid_implication(P, Q));
}

TEST(IsValidImplication, BoundedRange) {
    // (x >= 0 && x <= 5) => (x >= 0) — true
    static constexpr auto P =
        (Expression::var("x") >= 0.0) && (Expression::var("x") <= 5.0);
    static constexpr auto Q = Expression::var("x") >= 0.0;
    static_assert(is_valid_implication(P, Q));
}

TEST(IsValidImplication, TwoVariables) {
    // (x >= 0 && y >= 0 && x + y <= 10) => (x <= 10) — true
    static constexpr auto x = Expression::var("x");
    static constexpr auto y = Expression::var("y");
    static constexpr auto P = (x >= 0.0) && (y >= 0.0) && (x + y <= 10.0);
    static constexpr auto Q = x <= 10.0;
    static_assert(is_valid_implication(P, Q));
}

TEST(IsValidImplication, TwoVariablesFalse) {
    // (x >= 0 && y >= 0) => (x + y <= 10) — false
    static constexpr auto x = Expression::var("x");
    static constexpr auto y = Expression::var("y");
    static constexpr auto P = (x >= 0.0) && (y >= 0.0);
    static constexpr auto Q = x + y <= 10.0;
    static_assert(!is_valid_implication(P, Q));
}

TEST(IsValidImplication, IdenticalFormulas) {
    // (x > 0) => (x > 0) — true (trivial)
    static constexpr auto P = Expression::var("x") > 0.0;
    static_assert(is_valid_implication(P, P));
}

// ============================================================
// is_valid — Expression level
// ============================================================

TEST(IsValid, TautologyViaDemorgan) {
    // (x >= 0 || x < 0) — tautology
    // !formula → De Morgan → x < 0 && x >= 0 → single UNSAT clause
    static constexpr auto formula =
        (Expression::var("x") >= 0.0) || (Expression::var("x") < 0.0);
    static_assert(is_valid(formula));
}

TEST(IsValid, NotAlwaysTrue) {
    // (x > 0) — not always true (x = -1 is counterexample)
    static constexpr auto formula = Expression::var("x") > 0.0;
    static_assert(!is_valid(formula));
}

TEST(IsValid, TautologyLe) {
    // (x <= 5 || x > 5) — tautology
    static constexpr auto formula =
        (Expression::var("x") <= 5.0) || (Expression::var("x") > 5.0);
    static_assert(is_valid(formula));
}

TEST(IsValid, ContradictionNegated) {
    // !(x > 0 && x < 0) — always true (negation of contradiction)
    static constexpr auto formula =
        !((Expression::var("x") > 0.0) && (Expression::var("x") < 0.0));
    static_assert(is_valid(formula));
}

// ============================================================
// VarInfo overloads — real-valued variables
// ============================================================

TEST(IsValidImplicationReal, StrictOpenInterval) {
    // For reals: (x > 0 && x < 1) => (x >= 0) — true
    static constexpr auto P =
        (Expression::var("x") > 0.0) && (Expression::var("x") < 1.0);
    static constexpr auto Q = Expression::var("x") >= 0.0;
    constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("x", false);
        return v;
    }();
    static_assert(is_valid_implication(P, Q, vars));
}

TEST(IsValidImplicationReal, RealVsIntegerDifference) {
    // x > 0 && x < 1:
    //   For integers: UNSAT (no integer in (0,1))
    //   For reals: SAT (e.g., x = 0.5)
    static constexpr auto formula =
        (Expression::var("x") > 0.0) && (Expression::var("x") < 1.0);

    // Integer: the formula itself is UNSAT, so any implication from it is vacuously true
    constexpr auto parsed_int = parse_to_system(formula);
    static_assert(is_unsat(parsed_int));

    // Real: the formula is SAT
    constexpr auto parsed_real = [] {
        VarInfo<> v{};
        v.find_or_add("x", false);
        return parse_to_system(formula, v);
    }();
    static_assert(is_sat(parsed_real));
}

TEST(IsValidReal, TautologyReal) {
    // (x >= 0 || x < 0) — tautology for reals too
    static constexpr auto formula =
        (Expression::var("x") >= 0.0) || (Expression::var("x") < 0.0);
    constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("x", false);
        return v;
    }();
    static_assert(is_valid(formula, vars));
}

TEST(IsValidReal, NotValidForReals) {
    // (x > 0) — not valid for reals either
    static constexpr auto formula = Expression::var("x") > 0.0;
    constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("x", false);
        return v;
    }();
    static_assert(!is_valid(formula, vars));
}

// ============================================================
// End-to-end: parsed disjunction through solver
// ============================================================

TEST(SolverEndToEnd, DisjunctionSat) {
    // (x > 5) || (x < -5) → SAT (two satisfiable clauses)
    static constexpr auto formula =
        (Expression::var("x") > 5.0) || (Expression::var("x") < Expression::lit(-5.0));
    constexpr auto result = parse_to_system(formula);
    static_assert(result.clause_count == 2);
    static_assert(is_sat(result));
}

TEST(SolverEndToEnd, DisjunctionAllUnsatViaImplication) {
    // is_valid_implication where negating conclusion creates multiple clauses
    // P = (x >= 1 && x <= 3), Q = (x >= 0 && x <= 5)
    // P && !Q = P && (x < 0 || x > 5)
    //         = (x >= 1 && x <= 3 && x < 0) || (x >= 1 && x <= 3 && x > 5)
    //         Both clauses UNSAT → implication valid
    static constexpr auto x = Expression::var("x");
    static constexpr auto P = (x >= 1.0) && (x <= 3.0);
    static constexpr auto Q = (x >= 0.0) && (x <= 5.0);
    static_assert(is_valid_implication(P, Q));
}

TEST(SolverEndToEnd, DisjunctionNotAllUnsatViaImplication) {
    // P = (x >= 0 && x <= 10), Q = (x >= 1 && x <= 5)
    // P && !Q = P && (x < 1 || x > 5)
    //         = (x >= 0 && x <= 10 && x < 1) || (x >= 0 && x <= 10 && x > 5)
    //         First clause SAT (x = 0), second clause SAT (x = 6) → not valid
    static constexpr auto x = Expression::var("x");
    static constexpr auto P = (x >= 0.0) && (x <= 10.0);
    static constexpr auto Q = (x >= 1.0) && (x <= 5.0);
    static_assert(!is_valid_implication(P, Q));
}
