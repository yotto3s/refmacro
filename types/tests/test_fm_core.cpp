#include <gtest/gtest.h>
#include <reftype/fm/eliminate.hpp>

using namespace reftype::fm;

// --- has_contradiction ---

TEST(HasContradiction, EmptySystem) {
    constexpr InequalitySystem<> sys{};
    static_assert(!has_contradiction(sys));
}

TEST(HasContradiction, PositiveConstant) {
    // 5 >= 0 → no contradiction
    constexpr auto sys = InequalitySystem<>{}.add(
        LinearInequality::make({}, 5.0));
    static_assert(!has_contradiction(sys));
}

TEST(HasContradiction, ZeroConstantNonStrict) {
    // 0 >= 0 → no contradiction
    constexpr auto sys = InequalitySystem<>{}.add(
        LinearInequality::make({}, 0.0));
    static_assert(!has_contradiction(sys));
}

TEST(HasContradiction, ZeroConstantStrict) {
    // 0 > 0 → contradiction
    constexpr auto sys = InequalitySystem<>{}.add(
        LinearInequality::make({}, 0.0, true));
    static_assert(has_contradiction(sys));
}

TEST(HasContradiction, NegativeConstantNonStrict) {
    // -1 >= 0 → contradiction
    constexpr auto sys = InequalitySystem<>{}.add(
        LinearInequality::make({}, -1.0));
    static_assert(has_contradiction(sys));
}

TEST(HasContradiction, NegativeConstantStrict) {
    // -1 > 0 → contradiction
    constexpr auto sys = InequalitySystem<>{}.add(
        LinearInequality::make({}, -1.0, true));
    static_assert(has_contradiction(sys));
}

TEST(HasContradiction, MixedSomeContradiction) {
    // 5 >= 0 (ok) and -1 >= 0 (contradiction) → contradiction
    constexpr auto sys = InequalitySystem<>{}
        .add(LinearInequality::make({}, 5.0))
        .add(LinearInequality::make({}, -1.0));
    static_assert(has_contradiction(sys));
}

// --- eliminate_variable: single variable ---

TEST(EliminateVariable, SingleVarSAT) {
    // x >= 0 && x <= 5  (i.e., x >= 0 && -x + 5 >= 0)
    // Eliminate x → 0 + 5 >= 0 → 5 >= 0 → SAT
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))    // x >= 0
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 5.0));  // -x + 5 >= 0
    }();
    constexpr auto result = eliminate_variable(sys, 0);
    static_assert(result.count == 1);
    static_assert(!has_contradiction(result));
}

TEST(EliminateVariable, SingleVarUNSAT) {
    // x >= 5 && x <= 3  (i.e., x - 5 >= 0 && -x + 3 >= 0)
    // Eliminate x → -5 + 3 >= 0 → -2 >= 0 → UNSAT
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, -5.0))   // x - 5 >= 0
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 3.0));  // -x + 3 >= 0
    }();
    constexpr auto result = eliminate_variable(sys, 0);
    static_assert(result.count == 1);
    static_assert(has_contradiction(result));
}

TEST(EliminateVariable, StrictBoundsUNSAT) {
    // x > 0 && x < 0  (i.e., x > 0 && -x > 0)
    // Eliminate x → 0 > 0 → UNSAT
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true))   // x > 0
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 0.0, true)); // -x > 0
    }();
    constexpr auto result = eliminate_variable(sys, 0);
    static_assert(has_contradiction(result));
}

TEST(EliminateVariable, MixedStrictness) {
    // x > 0 && x <= 0  (i.e., x > 0 && -x >= 0)
    // Eliminate x → 0 > 0 (strict because lower is strict) → UNSAT
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true))    // x > 0
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 0.0, false)); // -x >= 0
    }();
    constexpr auto result = eliminate_variable(sys, 0);
    static_assert(has_contradiction(result));
}

TEST(EliminateVariable, NonStrictTouching) {
    // x >= 0 && x <= 0  (i.e., x >= 0 && -x >= 0)
    // Eliminate x → 0 >= 0 → SAT (x = 0)
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))    // x >= 0
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 0.0));  // -x >= 0
    }();
    constexpr auto result = eliminate_variable(sys, 0);
    static_assert(!has_contradiction(result));
}

TEST(EliminateVariable, UnrelatedPassThrough) {
    // x >= 0, y >= 3 — eliminate x → y >= 3 passes through
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        int y = s.vars.find_or_add("y");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))    // x >= 0
            .add(LinearInequality::make({LinearTerm{y, 1.0}}, -3.0));  // y - 3 >= 0
    }();
    constexpr auto result = eliminate_variable(sys, 0);
    // Only the y >= 3 inequality passes through (x >= 0 is a lower bound with no upper)
    static_assert(result.count == 1);
    static_assert(result.ineqs[0].terms[0].var_id == 1);
    static_assert(result.ineqs[0].constant == -3.0);
}

TEST(EliminateVariable, NonUnitCoefficients) {
    // 2x >= 6 && -3x >= -15  (i.e., 2x - 6 >= 0 && -3x + 15 >= 0)
    // Means: x >= 3 && x <= 5
    // Combine: lower_coeff=2, upper_abs=3
    // Result: (-6)*3 + 15*2 = -18 + 30 = 12 >= 0 → SAT
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 2.0}}, -6.0))    // 2x - 6 >= 0
            .add(LinearInequality::make({LinearTerm{x, -3.0}}, 15.0));  // -3x + 15 >= 0
    }();
    constexpr auto result = eliminate_variable(sys, 0);
    static_assert(result.count == 1);
    static_assert(result.ineqs[0].constant == 12.0);
    static_assert(!has_contradiction(result));
}

// --- eliminate_variable: two variables ---

TEST(EliminateVariable, TwoVarsSAT) {
    // x >= 0, y >= 0, x + y <= 10  → SAT
    // Represented: x >= 0, y >= 0, -x - y + 10 >= 0
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        int y = s.vars.find_or_add("y");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))
            .add(LinearInequality::make({LinearTerm{y, 1.0}}, 0.0))
            .add(LinearInequality::make({LinearTerm{x, -1.0}, LinearTerm{y, -1.0}}, 10.0));
    }();
    // Eliminate x first, then y via fm_is_unsat — system is satisfiable
    static_assert(!fm_is_unsat(sys));
}

TEST(EliminateVariable, TwoVarsUNSAT) {
    // x >= y && y >= x + 1  (i.e., x - y >= 0 && -x + y - 1 >= 0)
    // Eliminate y: lower={ineq1: y coeff +1}, upper={ineq0: y coeff -1}
    // Combined: (-x - 1)*1 + (x)*1 = -1 >= 0 → UNSAT
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        int y = s.vars.find_or_add("y");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}, LinearTerm{y, -1.0}}, 0.0))
            .add(LinearInequality::make({LinearTerm{x, -1.0}, LinearTerm{y, 1.0}}, -1.0));
    }();
    static_assert(fm_is_unsat(sys));
}

// --- fm_is_unsat ---

TEST(FmIsUnsat, EmptySystem) {
    constexpr InequalitySystem<> sys{};
    static_assert(!fm_is_unsat(sys));
}

TEST(FmIsUnsat, SingleInequalitySAT) {
    // x >= 0 → SAT (no upper bound, no contradiction)
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s.add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0));
    }();
    static_assert(!fm_is_unsat(sys));
}

TEST(FmIsUnsat, ConstantContradiction) {
    // -1 >= 0 → UNSAT (no variables, just a bad constant)
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        s.vars.find_or_add("x"); // var exists but unused
        return s.add(LinearInequality::make({}, -1.0));
    }();
    static_assert(fm_is_unsat(sys));
}

TEST(FmIsUnsat, ThreeVarsSAT) {
    // x >= 0, y >= 0, z >= 0, x + y + z <= 100 → SAT
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        int y = s.vars.find_or_add("y");
        int z = s.vars.find_or_add("z");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))
            .add(LinearInequality::make({LinearTerm{y, 1.0}}, 0.0))
            .add(LinearInequality::make({LinearTerm{z, 1.0}}, 0.0))
            .add(LinearInequality::make(
                {LinearTerm{x, -1.0}, LinearTerm{y, -1.0}, LinearTerm{z, -1.0}}, 100.0));
    }();
    static_assert(!fm_is_unsat(sys));
}

TEST(FmIsUnsat, ThreeVarsUNSAT) {
    // x >= 10, y >= 10, z >= 10, x + y + z <= 20 → UNSAT (need >= 30 but cap at 20)
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        int y = s.vars.find_or_add("y");
        int z = s.vars.find_or_add("z");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, -10.0))
            .add(LinearInequality::make({LinearTerm{y, 1.0}}, -10.0))
            .add(LinearInequality::make({LinearTerm{z, 1.0}}, -10.0))
            .add(LinearInequality::make(
                {LinearTerm{x, -1.0}, LinearTerm{y, -1.0}, LinearTerm{z, -1.0}}, 20.0));
    }();
    static_assert(fm_is_unsat(sys));
}

TEST(FmIsUnsat, MultipleBoundsOnSameVar) {
    // x >= 0, x >= 1, x <= 5, x <= 3 → SAT (1 <= x <= 3)
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, -1.0))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 5.0))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 3.0));
    }();
    static_assert(!fm_is_unsat(sys));
}

TEST(EliminateVariable, UpperBoundsOnlyPassThrough) {
    // y <= 5, y <= 3 — eliminate x (not present) → both pass through unchanged
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        s.vars.find_or_add("x");
        int y = s.vars.find_or_add("y");
        return s
            .add(LinearInequality::make({LinearTerm{y, -1.0}}, 5.0))   // -y + 5 >= 0
            .add(LinearInequality::make({LinearTerm{y, -1.0}}, 3.0));  // -y + 3 >= 0
    }();
    constexpr auto result = eliminate_variable(sys, 0);
    // x has no bounds, so both inequalities are unrelated and pass through
    static_assert(result.count == 2);
    static_assert(!fm_is_unsat(sys));
}

TEST(FmIsUnsat, StrictOpenInterval) {
    // x > 0 && x < 1 → SAT for reals (0 < x < 1)
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true))    // x > 0
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 1.0, true));  // -x + 1 > 0
    }();
    static_assert(!fm_is_unsat(sys));
}
