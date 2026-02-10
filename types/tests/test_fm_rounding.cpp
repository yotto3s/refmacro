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
    constexpr auto rounded = round_integer_bound(ineq, true);
    static_assert(rounded.constant == -3.0);
    static_assert(rounded.strict == false);
}

// Lower bound strict integer: x > 2 → x >= 3 → constant = -3
TEST(RoundIntegerBound, LowerStrictInteger) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, 1.0}}, -2.0, true);
    constexpr auto rounded = round_integer_bound(ineq, true);
    static_assert(rounded.constant == -3.0);
    static_assert(rounded.strict == false);
}

// Lower bound non-strict integer: x >= 2 → unchanged constant
TEST(RoundIntegerBound, LowerNonStrictInteger) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, 1.0}}, -2.0, false);
    constexpr auto rounded = round_integer_bound(ineq, true);
    static_assert(rounded.constant == -2.0);
    static_assert(rounded.strict == false);
}

// Upper bound: LHS <= constant (non-strict)
// x <= 3.5 → x <= 3 → constant = 3
TEST(RoundIntegerBound, UpperNonStrictFraction) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, -1.0}}, 3.5, false);
    constexpr auto rounded = round_integer_bound(ineq, false);
    static_assert(rounded.constant == 3.0);
    static_assert(rounded.strict == false);
}

// Upper bound strict integer: x < 3 → x <= 2 → constant = 2
TEST(RoundIntegerBound, UpperStrictInteger) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, -1.0}}, 3.0, true);
    constexpr auto rounded = round_integer_bound(ineq, false);
    static_assert(rounded.constant == 2.0);
    static_assert(rounded.strict == false);
}

// Upper bound non-strict integer: x <= 3 → unchanged constant
TEST(RoundIntegerBound, UpperNonStrictInteger) {
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, -1.0}}, 3.0, false);
    constexpr auto rounded = round_integer_bound(ineq, false);
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
        // x > 2.5 → 1*x - 2.5 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, 1.0}}, -2.5, true);
        // x < 3.5 → -1*x + 3.5 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, -1.0}}, 3.5, true);
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// x > 2 && x < 3, x integer → UNSAT
TEST(FMIntegerElim, StrictIntegerBoundsUNSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        // x > 2 → x - 2 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, 1.0}}, -2.0, true);
        // x < 3 → -x + 3 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, -1.0}}, 3.0, true);
        return fm_is_unsat(sys);
    }();
    static_assert(unsat);
}

// x >= 0 && x <= 0, x integer → SAT (x=0)
TEST(FMIntegerElim, ZeroBoundSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        // x >= 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, false);
        // x <= 0 → -x >= 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, -1.0}}, 0.0, false);
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// x > 0 && x < 1, x integer → UNSAT
TEST(FMIntegerElim, NoIntegerBetween01) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        // x > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true);
        // x < 1 → -x + 1 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, -1.0}}, 1.0, true);
        return fm_is_unsat(sys);
    }();
    static_assert(unsat);
}

// x > -0.5 && x < 0.5, x integer → SAT (x=0)
TEST(FMIntegerElim, FractionAroundZeroSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        // x > -0.5 → x + 0.5 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, 1.0}}, 0.5, true);
        // x < 0.5 → -x + 0.5 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, -1.0}}, 0.5, true);
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// Real variable: x > 0 && x < 1 → SAT (no rounding)
TEST(FMIntegerElim, RealVariableSAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", false);
        // x > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, 1.0}}, 0.0, true);
        // x < 1 → -x + 1 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, -1.0}}, 1.0, true);
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// --- Multi-variable integer tests ---

// x + y > 4, x + y < 6, both integer → SAT (x+y = 5)
TEST(FMIntegerMultiVar, SumBetween4And6SAT) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        int y = sys.vars.find_or_add("y", true);
        // x + y > 4 → x + y - 4 > 0
        sys.ineqs[sys.count++] = LinearInequality::make(
            {LinearTerm{x, 1.0}, LinearTerm{y, 1.0}}, -4.0, true);
        // x + y < 6 → -x - y + 6 > 0
        sys.ineqs[sys.count++] = LinearInequality::make(
            {LinearTerm{x, -1.0}, LinearTerm{y, -1.0}}, 6.0, true);
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
        // x + y > 4 → x + y - 4 > 0
        sys.ineqs[sys.count++] = LinearInequality::make(
            {LinearTerm{x, 1.0}, LinearTerm{y, 1.0}}, -4.0, true);
        // x + y < 5 → -x - y + 5 > 0
        sys.ineqs[sys.count++] = LinearInequality::make(
            {LinearTerm{x, -1.0}, LinearTerm{y, -1.0}}, 5.0, true);
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
        // x >= y → x - y >= 0
        sys.ineqs[sys.count++] = LinearInequality::make(
            {LinearTerm{x, 1.0}, LinearTerm{y, -1.0}}, 0.0, false);
        // y >= x + 1 → y - x - 1 >= 0 → -x + y - 1 >= 0
        sys.ineqs[sys.count++] = LinearInequality::make(
            {LinearTerm{x, -1.0}, LinearTerm{y, 1.0}}, -1.0, false);
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
        // 2x - 5 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, 2.0}}, -5.0, true);
        // -2x + 7 > 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, -2.0}}, 7.0, true);
        return fm_is_unsat(sys);
    }();
    static_assert(!unsat);
}

// 3x >= 1 && 3x <= 2, x integer → SAT reports SAT (known limitation:
// rounding doesn't normalize by coefficient, so divisibility constraints
// like "3x must be divisible by 3" are not captured)
TEST(FMElimNonUnit, DivisibilityLimitation) {
    constexpr bool unsat = [] consteval {
        InequalitySystem<> sys{};
        int x = sys.vars.find_or_add("x", true);
        // 3x - 1 >= 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, 3.0}}, -1.0, false);
        // -3x + 2 >= 0
        sys.ineqs[sys.count++] =
            LinearInequality::make({LinearTerm{x, -3.0}}, 2.0, false);
        return fm_is_unsat(sys);
    }();
    // Ideally UNSAT (no integer x where 1 <= 3x <= 2), but FM rounding
    // doesn't capture coefficient divisibility — this is a known limitation.
    static_assert(!unsat);
}
