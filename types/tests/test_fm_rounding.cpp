#include <gtest/gtest.h>
#include <utility>

#include <reftype/fm/eliminate.hpp>
#include <reftype/fm/rounding.hpp>
#include <reftype/fm/types.hpp>

using namespace reftype::fm;

// --- ceil_val ---

TEST(CeilVal, PositiveInteger) { static_assert(ceil_val(3.0) == 3.0); }
TEST(CeilVal, NegativeInteger) { static_assert(ceil_val(-3.0) == -3.0); }
TEST(CeilVal, Zero) { static_assert(ceil_val(0.0) == 0.0); }
TEST(CeilVal, PositiveFraction) { static_assert(ceil_val(2.3) == 3.0); }
TEST(CeilVal, NegativeFraction) { static_assert(ceil_val(-2.3) == -2.0); }
TEST(CeilVal, JustAboveInteger) { static_assert(ceil_val(2.001) == 3.0); }
TEST(CeilVal, JustBelowInteger) { static_assert(ceil_val(2.999) == 3.0); }

// --- floor_val ---

TEST(FloorVal, PositiveInteger) { static_assert(floor_val(3.0) == 3.0); }
TEST(FloorVal, NegativeInteger) { static_assert(floor_val(-3.0) == -3.0); }
TEST(FloorVal, Zero) { static_assert(floor_val(0.0) == 0.0); }
TEST(FloorVal, PositiveFraction) { static_assert(floor_val(2.7) == 2.0); }
TEST(FloorVal, NegativeFraction) { static_assert(floor_val(-2.7) == -3.0); }
TEST(FloorVal, JustAboveInteger) { static_assert(floor_val(3.001) == 3.0); }
TEST(FloorVal, JustBelowInteger) { static_assert(floor_val(2.999) == 2.0); }

// --- is_integer_val ---

TEST(IsIntegerVal, Integer) { static_assert(is_integer_val(5.0)); }
TEST(IsIntegerVal, Fraction) { static_assert(!is_integer_val(5.5)); }
TEST(IsIntegerVal, Zero) { static_assert(is_integer_val(0.0)); }
TEST(IsIntegerVal, NegativeInteger) { static_assert(is_integer_val(-4.0)); }
TEST(IsIntegerVal, NegativeFraction) { static_assert(!is_integer_val(-4.1)); }

// --- round_integer_bound ---

// Lower bound: x >= -constant (non-strict)
// x >= 2.5 → x >= 3 → constant = -3
TEST(RoundIntegerBound, LowerNonStrictFraction) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, 1.0}}, -2.5, false);
    constexpr auto rounded = round_integer_bound(ineq, true, 1.0);
    static_assert(rounded.constant == -3.0);
    static_assert(rounded.strict == false);
}

// Lower bound strict integer: x > 2 → x >= 3 → constant = -3
TEST(RoundIntegerBound, LowerStrictInteger) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, 1.0}}, -2.0, true);
    constexpr auto rounded = round_integer_bound(ineq, true, 1.0);
    static_assert(rounded.constant == -3.0);
    static_assert(rounded.strict == false);
}

// Lower bound non-strict integer: x >= 2 → unchanged constant
TEST(RoundIntegerBound, LowerNonStrictInteger) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, 1.0}}, -2.0, false);
    constexpr auto rounded = round_integer_bound(ineq, true, 1.0);
    static_assert(rounded.constant == -2.0);
    static_assert(rounded.strict == false);
}

// Upper bound: LHS <= constant (non-strict)
// x <= 3.5 → x <= 3 → constant = 3
TEST(RoundIntegerBound, UpperNonStrictFraction) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, -1.0}}, 3.5, false);
    constexpr auto rounded = round_integer_bound(ineq, false, 1.0);
    static_assert(rounded.constant == 3.0);
    static_assert(rounded.strict == false);
}

// Upper bound strict integer: x < 3 → x <= 2 → constant = 2
TEST(RoundIntegerBound, UpperStrictInteger) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, -1.0}}, 3.0, true);
    constexpr auto rounded = round_integer_bound(ineq, false, 1.0);
    static_assert(rounded.constant == 2.0);
    static_assert(rounded.strict == false);
}

// Upper bound non-strict integer: x <= 3 → unchanged constant
TEST(RoundIntegerBound, UpperNonStrictInteger) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, -1.0}}, 3.0, false);
    constexpr auto rounded = round_integer_bound(ineq, false, 1.0);
    static_assert(rounded.constant == 3.0);
    static_assert(rounded.strict == false);
}

// --- VarInfo no-mixing ---

TEST(VarInfoNoMixing, TwoIntegers) {
    static constexpr auto vars = [] consteval {
        VarInfo<> v{};
        v.find_or_add("x", true);
        v.find_or_add("y", true);
        return v;
    }();
    static_assert(vars.count == 2);
    static_assert(vars.is_integer[0] == true);
    static_assert(vars.is_integer[1] == true);
}

TEST(VarInfoNoMixing, TwoReals) {
    static constexpr auto vars = [] consteval {
        VarInfo<> v{};
        v.find_or_add("x", false);
        v.find_or_add("y", false);
        return v;
    }();
    static_assert(vars.count == 2);
    static_assert(vars.is_integer[0] == false);
    static_assert(vars.is_integer[1] == false);
}

// Duplicate find_or_add with matching type succeeds
TEST(VarInfoNoMixing, DuplicateSameType) {
    static constexpr auto result = [] consteval {
        VarInfo<> v{};
        int first = v.find_or_add("x", true);
        int second = v.find_or_add("x", true); // same type — returns existing
        return std::pair{v.count, std::pair{first, second}};
    }();
    static_assert(result.first == 1);
    static_assert(result.second.first == 0);
    static_assert(result.second.second == 0);
}

// --- Integer-aware FM elimination ---

// x > 2.5 && x < 3.5, x integer → SAT (x=3)
TEST(FMIntegerElim, FractionBoundsSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, -2.5, true))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 3.5, true));
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// x > 2 && x < 3, x integer → UNSAT
TEST(FMIntegerElim, StrictIntegerBoundsUNSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, -2.0, true))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 3.0, true));
        return fm_is_unsat(sys);
    }();
    static_assert(unsat);
}

// x >= 0 && x <= 0, x integer → SAT (x=0)
TEST(FMIntegerElim, ZeroBoundSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, false))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 0.0, false));
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// x > 0 && x < 1, x integer → UNSAT
TEST(FMIntegerElim, NoIntegerBetween01) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 1.0, true));
        return fm_is_unsat(sys);
    }();
    static_assert(unsat);
}

// x > -0.5 && x < 0.5, x integer → SAT (x=0)
TEST(FMIntegerElim, FractionAroundZeroSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.5, true))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 0.5, true));
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// Real variable: x > 0 && x < 1 → SAT (no rounding)
TEST(FMIntegerElim, RealVariableSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", false);
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 1.0, true));
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// --- Negative-bound integer tests ---

// x > -3 && x < -1, x integer → SAT (x=-2)
TEST(FMIntegerElim, NegativeBoundsSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        // x > -3 → x + 3 > 0
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 3.0, true))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, -1.0, true));
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// x > -2 && x < -1, x integer → UNSAT (no integer between -2 and -1)
TEST(FMIntegerElim, NegativeBoundsUNSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        // x > -2 → x + 2 > 0
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 2.0, true))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, -1.0, true));
        return fm_is_unsat(sys);
    }();
    static_assert(unsat);
}

// --- Multi-variable integer tests ---

// x + y > 4, x + y < 6, both integer → SAT (x+y = 5)
TEST(FMIntegerMultiVar, SumBetween4And6SAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        int y = sys.vars.find_or_add("y", true);
        sys = sys
            .add(LinearInequality::make(
                {LinearTerm{x, 1.0}, LinearTerm{y, 1.0}}, -4.0, true))
            .add(LinearInequality::make(
                {LinearTerm{x, -1.0}, LinearTerm{y, -1.0}}, 6.0, true));
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// x + y > 4, x + y < 5, both integer → UNSAT
TEST(FMIntegerMultiVar, SumBetween4And5UNSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        int y = sys.vars.find_or_add("y", true);
        sys = sys
            .add(LinearInequality::make(
                {LinearTerm{x, 1.0}, LinearTerm{y, 1.0}}, -4.0, true))
            .add(LinearInequality::make(
                {LinearTerm{x, -1.0}, LinearTerm{y, -1.0}}, 5.0, true));
        return fm_is_unsat(sys);
    }();
    static_assert(unsat);
}

// x >= y, y >= x + 1, both integer → UNSAT
TEST(FMIntegerMultiVar, XGeqYAndYGeqXPlus1UNSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        int y = sys.vars.find_or_add("y", true);
        sys = sys
            .add(LinearInequality::make(
                {LinearTerm{x, 1.0}, LinearTerm{y, -1.0}}, 0.0, false))
            .add(LinearInequality::make(
                {LinearTerm{x, -1.0}, LinearTerm{y, 1.0}}, -1.0, false));
        return fm_is_unsat(sys);
    }();
    static_assert(unsat);
}

// --- Non-unit coefficient tests ---

// 2x > 5 && 2x < 7, x integer → SAT (x=3, since 2*3=6 is in (5,7))
TEST(FMElimNonUnit, NonUnitCoeffSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 2.0}}, -5.0, true))
            .add(LinearInequality::make({LinearTerm{x, -2.0}}, 7.0, true));
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// 3x >= 1 && 3x <= 2, x integer → UNSAT (no integer x where 1 <= 3x <= 2)
// Rounding normalizes by coefficient: 3x >= 1 → x >= 1/3 → x >= 1 → 3x >= 3,
// and 3x <= 2 → x <= 2/3 → x <= 0 → 3x <= 0. Combined: 3 + 0 <= 0 → UNSAT.
TEST(FMElimNonUnit, DivisibilityDetected) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 3.0}}, -1.0, false))
            .add(LinearInequality::make({LinearTerm{x, -3.0}}, 2.0, false));
        return fm_is_unsat(sys);
    }();
    static_assert(unsat);
}

// --- Non-unit coefficient normalization tests ---

// 2x - 3 >= 0 && -2x + 3 >= 0, x integer → UNSAT (x = 1.5, no integer)
// Without normalization this would report SAT; with normalization:
// lower: x >= 1.5 → x >= 2 → 2x - 4 >= 0
// upper: x <= 1.5 → x <= 1 → -2x + 2 >= 0
// combined: -4 + 2 = -2, -2 >= 0 → UNSAT
TEST(FMElimNonUnit, NonUnitHalfIntegerUNSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        sys = sys
            .add(LinearInequality::make({LinearTerm{x, 2.0}}, -3.0, false))
            .add(LinearInequality::make({LinearTerm{x, -2.0}}, 3.0, false));
        return fm_is_unsat(sys);
    }();
    static_assert(unsat);
}

// 2x - 3 > 0, x integer → tightens to x >= 2 (2x - 4 >= 0)
TEST(RoundIntegerBound, NonUnitLowerStrictFraction) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, 2.0}}, -3.0, true);
    constexpr auto rounded = round_integer_bound(ineq, true, 2.0);
    static_assert(rounded.constant == -4.0);
    static_assert(rounded.strict == false);
}

// -2x + 3 >= 0, x integer → tightens to x <= 1 (-2x + 2 >= 0)
TEST(RoundIntegerBound, NonUnitUpperNonStrictFraction) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, -2.0}}, 3.0, false);
    constexpr auto rounded = round_integer_bound(ineq, false, 2.0);
    static_assert(rounded.constant == 2.0);
    static_assert(rounded.strict == false);
}

// Multi-variable: 2x + 3y - 3 >= 0, x integer — must NOT normalize by coeff
// (normalizing would be unsound: x=0,y=1 satisfies original but not tightened)
TEST(FMElimNonUnit, MultiVarNoNormalization) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        int y = sys.vars.find_or_add("y", true);
        // 2x + 3y - 3 >= 0 && -x + 0 >= 0 && x - 0 >= 0 && -y + 1 >= 0 && y - 1 >= 0
        // Forces x=0, y=1, and 2(0)+3(1)-3 = 0 >= 0 → SAT
        sys = sys
            .add(LinearInequality::make(
                {LinearTerm{x, 2.0}, LinearTerm{y, 3.0}}, -3.0, false))
            .add(LinearInequality::make({LinearTerm{x, -1.0}}, 0.0, false))
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, false))
            .add(LinearInequality::make({LinearTerm{y, -1.0}}, 1.0, false))
            .add(LinearInequality::make({LinearTerm{y, 1.0}}, -1.0, false));
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}
