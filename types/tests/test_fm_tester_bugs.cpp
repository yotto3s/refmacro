#include <gtest/gtest.h>
#include <utility>

#include <reftype/fm/eliminate.hpp>
#include <reftype/fm/rounding.hpp>
#include <reftype/fm/solver.hpp>
#include <reftype/fm/types.hpp>

using namespace reftype::fm;
using Expression = refmacro::Expression<>;

// ============================================================
// F2: Multi-variable integer rounding with integer coefficients
//     where the sum of remaining terms is integer — rounding with
//     coeff=1.0 IS correct in this case (confirming CR5).
//
//     This tests that the coeff=1 fallback works for the common
//     case where all coefficients are integer.
// ============================================================

TEST(TesterBugs_F2, MultiVarIntCoeffRoundingCorrect) {
    // System: 2x + 3y - 7.5 >= 0  AND  -2x - 3y + 8.5 >= 0
    // i.e., 2x + 3y in [7.5, 8.5]. Since 2x+3y is always integer,
    // the only solution is 2x + 3y = 8. e.g., x=1, y=2 → 2+6=8 ✓
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        int y = sys.vars.find_or_add("y", true);
        // 2x + 3y - 7.5 >= 0
        sys = sys.add(LinearInequality::make(
            {LinearTerm{x, 2.0}, LinearTerm{y, 3.0}}, -7.5, false));
        // -2x - 3y + 8.5 >= 0
        sys = sys.add(LinearInequality::make(
            {LinearTerm{x, -2.0}, LinearTerm{y, -3.0}}, 8.5, false));
        return fm_is_unsat(sys);
    }();
    // x=1, y=2 → 2+6=8, 8 in [7.5, 8.5] → SAT
    static_assert(!unsat, "F2: multi-var integer coeff rounding should be correct (SAT)");
}

// ============================================================
// F3: Multi-variable integer rounding with integer coefficients
//     correctly detects UNSAT when sum gap is <1.
//
//     2x + 3y > 8  AND  2x + 3y < 9, integers.
//     2x+3y is always integer, so no integer in open interval (8,9).
// ============================================================

TEST(TesterBugs_F3, MultiVarIntCoeffTightenDetectsUNSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        int y = sys.vars.find_or_add("y", true);
        // 2x + 3y - 8 > 0
        sys = sys.add(LinearInequality::make(
            {LinearTerm{x, 2.0}, LinearTerm{y, 3.0}}, -8.0, true));
        // -2x - 3y + 9 > 0
        sys = sys.add(LinearInequality::make(
            {LinearTerm{x, -2.0}, LinearTerm{y, -3.0}}, 9.0, true));
        return fm_is_unsat(sys);
    }();
    // No integer satisfies 8 < 2x+3y < 9 → UNSAT
    static_assert(unsat, "F3: multi-var integer coeff tightening detects UNSAT");
}

// ============================================================
// F4: Conjoin cross-product 2x2 produces 4 clauses (CR4).
//
//     (A || B) && (C || D) → AC, AD, BC, BD
// ============================================================

TEST(TesterBugs_F4, ConjoinCrossProduct2x2) {
    constexpr auto result = [] consteval {
        // Left: (x > 0) || (x < -1) → 2 clauses
        InequalitySystem<> a{};
        int xa = a.vars.find_or_add("x");
        a = a.add(LinearInequality::make({LinearTerm{xa, 1.0}}, 0.0, true));

        InequalitySystem<> b{};
        int xb = b.vars.find_or_add("x");
        b = b.add(LinearInequality::make({LinearTerm{xb, -1.0}}, -1.0, true));

        ParseResult<> left{};
        left.clauses[0] = a;
        left.clauses[1] = b;
        left.clause_count = 2;

        // Right: (y > 0) || (y < -1) → 2 clauses
        InequalitySystem<> c{};
        c.vars.find_or_add("x"); // keep consistent var ordering
        int yc = c.vars.find_or_add("y");
        c = c.add(LinearInequality::make({LinearTerm{yc, 1.0}}, 0.0, true));

        InequalitySystem<> d{};
        d.vars.find_or_add("x");
        int yd = d.vars.find_or_add("y");
        d = d.add(LinearInequality::make({LinearTerm{yd, -1.0}}, -1.0, true));

        ParseResult<> right{};
        right.clauses[0] = c;
        right.clauses[1] = d;
        right.clause_count = 2;

        return conjoin(left, right);
    }();
    // 2×2 cross product should yield 4 clauses
    static_assert(result.clause_count == 4, "F4: conjoin 2x2 should produce 4 clauses");
    // Each clause has 2 inequalities (one from left, one from right)
    static_assert(result.clauses[0].count == 2);
    static_assert(result.clauses[1].count == 2);
    static_assert(result.clauses[2].count == 2);
    static_assert(result.clauses[3].count == 2);
}

// ============================================================
// F6: round_integer_bound single-variable with non-unit coeff.
//
//     3*x - 7 >= 0 (lower, coeff=3).
//     x >= 7/3 ≈ 2.333, rounds to x >= 3 → 3*3 - 9 = 0, constant = -9.
// ============================================================

TEST(TesterBugs_F6, SingleVarNonUnitLowerFraction) {
    // 3x - 7 >= 0 → x >= 7/3 ≈ 2.333 → rounds to x >= 3 → 3x - 9 >= 0
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, 3.0}}, -7.0, false);
    constexpr auto rounded = round_integer_bound(ineq, true, 3.0);
    static_assert(rounded.constant == -9.0,
        "F6: 3x >= 7 → x >= 3 → 3x >= 9 → constant = -9");
    static_assert(rounded.strict == false);
}

// ============================================================
// F7: round_integer_bound single-variable upper with non-unit coeff.
//
//     -3*x + 7 >= 0 → x <= 7/3 ≈ 2.333 → x <= 2 → -3*2 + 6 = 0
//     constant = 6.
// ============================================================

TEST(TesterBugs_F7, SingleVarNonUnitUpperFraction) {
    // -3x + 7 >= 0 → x <= 7/3 ≈ 2.333 → x <= 2 → -3x + 6 >= 0
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, -3.0}}, 7.0, false);
    constexpr auto rounded = round_integer_bound(ineq, false, 3.0);
    static_assert(rounded.constant == 6.0,
        "F7: -3x + 7 >= 0 → x <= 2 → -3x + 6 >= 0");
    static_assert(rounded.strict == false);
}

// ============================================================
// F8: End-to-end non-unit coefficient elimination with integer
//     rounding that relies on normalization.
//
//     3x >= 7 AND 3x <= 8, x integer.
//     x >= 7/3 ≈ 2.333 → x >= 3, and x <= 8/3 ≈ 2.666 → x <= 2.
//     3 > 2 → UNSAT.
// ============================================================

TEST(TesterBugs_F8, NonUnitCoeffDivisibilityGap) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        // 3x - 7 >= 0 (x >= 7/3)
        sys = sys.add(LinearInequality::make({LinearTerm{x, 3.0}}, -7.0, false));
        // -3x + 8 >= 0 (x <= 8/3)
        sys = sys.add(LinearInequality::make({LinearTerm{x, -3.0}}, 8.0, false));
        return fm_is_unsat(sys);
    }();
    // No integer in [7/3, 8/3] ≈ [2.333, 2.666] → UNSAT
    static_assert(unsat, "F8: 3x in [7,8] with no integer solution → UNSAT");
}
