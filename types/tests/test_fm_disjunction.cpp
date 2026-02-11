#include <gtest/gtest.h>
#include <refmacro/math.hpp>
#include <reftype/fm/solver.hpp>

using namespace reftype::fm;
using Expression = refmacro::Expression<>;

// ============================================================
// negate_inequality
// ============================================================

TEST(NegateInequality, NonStrict) {
    // x >= 3  →  (1*x + (-3) >= 0)
    // negated →  (-1*x + 3 > 0)  i.e. x < 3
    constexpr auto ineq = LinearInequality::make({LinearTerm{0, 1.0}}, -3.0);
    constexpr auto neg = negate_inequality(ineq);
    static_assert(neg.terms[0].coeff == -1.0);
    static_assert(neg.constant == 3.0);
    static_assert(neg.strict == true);
}

TEST(NegateInequality, Strict) {
    // x > 3  →  (1*x + (-3) > 0)
    // negated →  (-1*x + 3 >= 0)  i.e. x <= 3
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, 1.0}}, -3.0, true);
    constexpr auto neg = negate_inequality(ineq);
    static_assert(neg.terms[0].coeff == -1.0);
    static_assert(neg.constant == 3.0);
    static_assert(neg.strict == false);
}

// ============================================================
// clause_implies
// ============================================================

TEST(ClauseImplies, StrongerImpliesWeaker) {
    // (x >= 1 && x <= 3) => (x >= 0 && x <= 5)
    constexpr auto result = [] {
        InequalitySystem<> a{};
        int x = a.vars.find_or_add("x");
        a = a.add(LinearInequality::make({LinearTerm{x, 1.0}}, -1.0))  // x >= 1
              .add(LinearInequality::make({LinearTerm{x, -1.0}}, 3.0)); // x <= 3

        InequalitySystem<> b{};
        int xb = b.vars.find_or_add("x");
        b = b.add(LinearInequality::make({LinearTerm{xb, 1.0}}, 0.0))  // x >= 0
              .add(LinearInequality::make({LinearTerm{xb, -1.0}}, 5.0)); // x <= 5

        return clause_implies(a, b);
    }();
    static_assert(result == true);
}

TEST(ClauseImplies, WeakerDoesNotImplyStronger) {
    // (x >= 0 && x <= 5) does NOT imply (x >= 1 && x <= 3)
    constexpr auto result = [] {
        InequalitySystem<> a{};
        int x = a.vars.find_or_add("x");
        a = a.add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))   // x >= 0
              .add(LinearInequality::make({LinearTerm{x, -1.0}}, 5.0)); // x <= 5

        InequalitySystem<> b{};
        int xb = b.vars.find_or_add("x");
        b = b.add(LinearInequality::make({LinearTerm{xb, 1.0}}, -1.0))  // x >= 1
              .add(LinearInequality::make({LinearTerm{xb, -1.0}}, 3.0)); // x <= 3

        return clause_implies(a, b);
    }();
    static_assert(result == false);
}

TEST(ClauseImplies, IdenticalClauses) {
    // (x >= 0) implies (x >= 0) — trivially
    constexpr auto result = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        s = s.add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0));
        return clause_implies(s, s);
    }();
    static_assert(result == true);
}

TEST(ClauseImplies, TwoVariables) {
    // (x >= 0 && y >= 0 && x + y <= 5) => (x <= 5)
    constexpr auto result = [] {
        InequalitySystem<> a{};
        int x = a.vars.find_or_add("x");
        int y = a.vars.find_or_add("y");
        a = a.add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))
              .add(LinearInequality::make({LinearTerm{y, 1.0}}, 0.0))
              .add(LinearInequality::make({LinearTerm{x, -1.0}, LinearTerm{y, -1.0}}, 5.0));

        InequalitySystem<> b{};
        int xb = b.vars.find_or_add("x");
        b.vars.find_or_add("y"); // consistent VarInfo
        b = b.add(LinearInequality::make({LinearTerm{xb, -1.0}}, 5.0)); // x <= 5

        return clause_implies(a, b);
    }();
    static_assert(result == true);
}

// ============================================================
// remove_unsat_clauses
// ============================================================

TEST(RemoveUnsatClauses, KeepsSatClauses) {
    // (x >= 0) || (x <= 0) — both SAT, keep both
    constexpr auto result = [] {
        InequalitySystem<> s1{};
        int x1 = s1.vars.find_or_add("x");
        s1 = s1.add(LinearInequality::make({LinearTerm{x1, 1.0}}, 0.0));

        InequalitySystem<> s2{};
        int x2 = s2.vars.find_or_add("x");
        s2 = s2.add(LinearInequality::make({LinearTerm{x2, -1.0}}, 0.0));

        ParseResult<> r{};
        r.clauses[0] = s1;
        r.clauses[1] = s2;
        r.clause_count = 2;
        return remove_unsat_clauses(r);
    }();
    static_assert(result.clause_count == 2);
}

TEST(RemoveUnsatClauses, RemovesUnsatKeepsSat) {
    // (x > 0 && x < 0) || (x >= 0) — first UNSAT, remove it
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
        return remove_unsat_clauses(r);
    }();
    static_assert(result.clause_count == 1);
    // The remaining clause is the SAT one (x >= 0)
    static_assert(!fm_is_unsat(result.clauses[0]));
}

TEST(RemoveUnsatClauses, AllUnsatYieldsEmpty) {
    // (x > 0 && x < 0) || (x > 5 && x < 3) — both UNSAT
    constexpr auto result = [] {
        InequalitySystem<> s1{};
        int x1 = s1.vars.find_or_add("x");
        s1 = s1.add(LinearInequality::make({LinearTerm{x1, 1.0}}, 0.0, true))
               .add(LinearInequality::make({LinearTerm{x1, -1.0}}, 0.0, true));

        InequalitySystem<> s2{};
        int x2 = s2.vars.find_or_add("x");
        s2 = s2.add(LinearInequality::make({LinearTerm{x2, 1.0}}, -5.0, true))
               .add(LinearInequality::make({LinearTerm{x2, -1.0}}, 3.0, true));

        ParseResult<> r{};
        r.clauses[0] = s1;
        r.clauses[1] = s2;
        r.clause_count = 2;
        return remove_unsat_clauses(r);
    }();
    static_assert(result.clause_count == 0);
}

// ============================================================
// remove_subsumed_clauses
// ============================================================

TEST(RemoveSubsumedClauses, NarrowSubsumedByBroad) {
    // (x > 0 && x < 10) || (x > 0 && x < 5)
    // Second: (0,5) ⊂ first: (0,10) → second subsumed, dropped
    constexpr auto result = [] {
        InequalitySystem<> broad{};
        int x = broad.vars.find_or_add("x");
        broad = broad.add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true))
                     .add(LinearInequality::make({LinearTerm{x, -1.0}}, 10.0, true));

        InequalitySystem<> narrow{};
        int xn = narrow.vars.find_or_add("x");
        narrow = narrow.add(LinearInequality::make({LinearTerm{xn, 1.0}}, 0.0, true))
                       .add(LinearInequality::make({LinearTerm{xn, -1.0}}, 5.0, true));

        ParseResult<> r{};
        r.clauses[0] = broad;
        r.clauses[1] = narrow;
        r.clause_count = 2;
        return remove_subsumed_clauses(r);
    }();
    static_assert(result.clause_count == 1);
}

TEST(RemoveSubsumedClauses, DisjointKeptBoth) {
    // (x > 0 && x < 5) || (x > 10 && x < 20)
    // Neither subsumes the other → keep both
    constexpr auto result = [] {
        InequalitySystem<> s1{};
        int x1 = s1.vars.find_or_add("x");
        s1 = s1.add(LinearInequality::make({LinearTerm{x1, 1.0}}, 0.0, true))
               .add(LinearInequality::make({LinearTerm{x1, -1.0}}, 5.0, true));

        InequalitySystem<> s2{};
        int x2 = s2.vars.find_or_add("x");
        s2 = s2.add(LinearInequality::make({LinearTerm{x2, 1.0}}, -10.0, true))
               .add(LinearInequality::make({LinearTerm{x2, -1.0}}, 20.0, true));

        ParseResult<> r{};
        r.clauses[0] = s1;
        r.clauses[1] = s2;
        r.clause_count = 2;
        return remove_subsumed_clauses(r);
    }();
    static_assert(result.clause_count == 2);
}

TEST(RemoveSubsumedClauses, ThreeClausesChainedSubsumption) {
    // (x >= 0 && x <= 10) || (x >= 1 && x <= 9) || (x >= 2 && x <= 8)
    // Third ⊂ Second ⊂ First → only first survives
    constexpr auto result = [] {
        InequalitySystem<> s1{};
        int x1 = s1.vars.find_or_add("x");
        s1 = s1.add(LinearInequality::make({LinearTerm{x1, 1.0}}, 0.0))
               .add(LinearInequality::make({LinearTerm{x1, -1.0}}, 10.0));

        InequalitySystem<> s2{};
        int x2 = s2.vars.find_or_add("x");
        s2 = s2.add(LinearInequality::make({LinearTerm{x2, 1.0}}, -1.0))
               .add(LinearInequality::make({LinearTerm{x2, -1.0}}, 9.0));

        InequalitySystem<> s3{};
        int x3 = s3.vars.find_or_add("x");
        s3 = s3.add(LinearInequality::make({LinearTerm{x3, 1.0}}, -2.0))
               .add(LinearInequality::make({LinearTerm{x3, -1.0}}, 8.0));

        ParseResult<> r{};
        r.clauses[0] = s1;
        r.clauses[1] = s2;
        r.clauses[2] = s3;
        r.clause_count = 3;
        return remove_subsumed_clauses(r);
    }();
    static_assert(result.clause_count == 1);
}

// ============================================================
// simplify_dnf
// ============================================================

TEST(SimplifyDnf, RemovesUnsatAndSubsumed) {
    // (x > 0 && x < 0) || (x >= 0 && x <= 10) || (x >= 1 && x <= 5)
    // First: UNSAT → removed. Third ⊂ Second → third removed.
    // Result: 1 clause (x >= 0 && x <= 10)
    constexpr auto result = [] {
        InequalitySystem<> unsat{};
        int xu = unsat.vars.find_or_add("x");
        unsat = unsat.add(LinearInequality::make({LinearTerm{xu, 1.0}}, 0.0, true))
                     .add(LinearInequality::make({LinearTerm{xu, -1.0}}, 0.0, true));

        InequalitySystem<> broad{};
        int xb = broad.vars.find_or_add("x");
        broad = broad.add(LinearInequality::make({LinearTerm{xb, 1.0}}, 0.0))
                     .add(LinearInequality::make({LinearTerm{xb, -1.0}}, 10.0));

        InequalitySystem<> narrow{};
        int xn = narrow.vars.find_or_add("x");
        narrow = narrow.add(LinearInequality::make({LinearTerm{xn, 1.0}}, -1.0))
                       .add(LinearInequality::make({LinearTerm{xn, -1.0}}, 5.0));

        ParseResult<> r{};
        r.clauses[0] = unsat;
        r.clauses[1] = broad;
        r.clauses[2] = narrow;
        r.clause_count = 3;
        return simplify_dnf(r);
    }();
    static_assert(result.clause_count == 1);
    static_assert(is_sat(result));
}

TEST(SimplifyDnf, SingleClauseUnchanged) {
    constexpr auto result = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        s = s.add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0));
        return simplify_dnf(single_clause(s));
    }();
    static_assert(result.clause_count == 1);
}

TEST(SimplifyDnf, EmptyDnfUnchanged) {
    constexpr ParseResult<> empty{};
    constexpr auto result = simplify_dnf(empty);
    static_assert(result.clause_count == 0);
}

// ============================================================
// Correctness equivalence: clause-by-clause vs brute-force
// ============================================================

TEST(ImplicationEquivalence, ConjunctiveConclusion) {
    // Both approaches agree: (x >= 1 && x <= 3) => (x >= 0 && x <= 5)
    static constexpr auto x = Expression::var("x");
    static constexpr auto P = (x >= 1.0) && (x <= 3.0);
    static constexpr auto Q = (x >= 0.0) && (x <= 5.0);
    // Clause-by-clause (integrated in solver)
    static_assert(is_valid_implication(P, Q));
    // Manual brute-force check
    constexpr auto combined = P && !Q;
    static_assert(is_unsat(parse_to_system(combined)));
}

TEST(ImplicationEquivalence, DisjunctivePremise) {
    // (x > 5 || x < -5) => (x > 0 || x < 0)
    // Each clause of premise implies the conclusion
    static constexpr auto x = Expression::var("x");
    static constexpr auto P = (x > 5.0) || (x < Expression::lit(-5.0));
    static constexpr auto Q = (x > 0.0) || (x < 0.0);
    static_assert(is_valid_implication(P, Q));
    // Brute-force
    constexpr auto combined = P && !Q;
    static_assert(is_unsat(parse_to_system(combined)));
}

TEST(ImplicationEquivalence, FalseImplication) {
    // (x >= 0) does NOT imply (x >= 5)
    static constexpr auto x = Expression::var("x");
    static constexpr auto P = x >= 0.0;
    static constexpr auto Q = x >= 5.0;
    static_assert(!is_valid_implication(P, Q));
    // Brute-force
    constexpr auto combined = P && !Q;
    static_assert(!is_unsat(parse_to_system(combined)));
}

TEST(ImplicationEquivalence, MultiVarConjunctive) {
    // (x >= 1 && x <= 3 && y >= 1 && y <= 3) => (x >= 0 && y >= 0)
    static constexpr auto x = Expression::var("x");
    static constexpr auto y = Expression::var("y");
    static constexpr auto P =
        (x >= 1.0) && (x <= 3.0) && (y >= 1.0) && (y <= 3.0);
    static constexpr auto Q = (x >= 0.0) && (y >= 0.0);
    static_assert(is_valid_implication(P, Q));
    // Brute-force
    constexpr auto combined = P && !Q;
    static_assert(is_unsat(parse_to_system(combined)));
}

// ============================================================
// Edge cases
// ============================================================

TEST(ImplicationEquivalence, EmptyPremise) {
    // true => (x >= 0) is NOT valid — x could be negative.
    // An empty conjunction (no constraints) means "true".
    // But Expression-level we can't easily represent "true", so test
    // a tautological premise: (x >= 0 || x < 0) => (x >= 5)
    static constexpr auto x = Expression::var("x");
    static constexpr auto P = (x >= 0.0) || (x < 0.0);
    static constexpr auto Q = x >= 5.0;
    static_assert(!is_valid_implication(P, Q));
}

TEST(ImplicationEquivalence, UnsatPremise) {
    // (x > 0 && x < 0) => anything — vacuously true
    static constexpr auto x = Expression::var("x");
    static constexpr auto P = (x > 0.0) && (x < 0.0);
    static constexpr auto Q = x >= 100.0;
    static_assert(is_valid_implication(P, Q));
}

TEST(ImplicationEquivalence, RealValuedVariables) {
    // For reals: (x > 0 && x < 1) => (x >= 0) — true
    static constexpr auto x = Expression::var("x");
    static constexpr auto P = (x > 0.0) && (x < 1.0);
    static constexpr auto Q = x >= 0.0;
    constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("x", false);
        return v;
    }();
    static_assert(is_valid_implication(P, Q, vars));
}
