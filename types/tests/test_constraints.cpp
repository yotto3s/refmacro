#include <gtest/gtest.h>
#include <reftype/constraints.hpp>
#include <reftype/types.hpp>
#include <refmacro/math.hpp>

using namespace reftype;
using refmacro::str_eq;

// --- Basic construction ---

TEST(ConstraintSet, EmptySet) {
    constexpr ConstraintSet cs{};
    static_assert(cs.count == 0);
}

// --- Add single constraint ---

TEST(ConstraintSet, AddSingle) {
    constexpr auto formula = Expression<128>::var("#v") > Expression<128>::lit(0);
    constexpr auto cs = ConstraintSet<>{}.add(formula, "value must be positive");
    static_assert(cs.count == 1);
    static_assert(str_eq(cs.constraints[0].origin, "value must be positive"));

    // Verify formula is a valid expression (root node is "gt")
    constexpr auto f = cs.constraints[0].formula;
    static_assert(str_eq(f.ast.nodes[f.id].tag, "gt"));
}

// --- Add multiple constraints ---

TEST(ConstraintSet, AddMultiple) {
    constexpr auto cs = ConstraintSet<>{}
        .add(Expression<128>::var("#v") > Expression<128>::lit(0), "positive")
        .add(Expression<128>::var("#v") < Expression<128>::lit(100), "bounded");
    static_assert(cs.count == 2);

    static_assert(str_eq(cs.constraints[0].origin, "positive"));
    static_assert(str_eq(cs.constraints[1].origin, "bounded"));
}

// --- Merge ---

TEST(ConstraintSet, MergeTwoSets) {
    constexpr auto cs1 = ConstraintSet<>{}
        .add(Expression<128>::var("x") > Expression<128>::lit(0), "x positive");
    constexpr auto cs2 = ConstraintSet<>{}
        .add(Expression<128>::var("y") > Expression<128>::lit(0), "y positive")
        .add(Expression<128>::var("y") < Expression<128>::lit(10), "y bounded");

    constexpr auto merged = cs1.merge(cs2);
    static_assert(merged.count == 3);
    static_assert(str_eq(merged.constraints[0].origin, "x positive"));
    static_assert(str_eq(merged.constraints[1].origin, "y positive"));
    static_assert(str_eq(merged.constraints[2].origin, "y bounded"));
}

TEST(ConstraintSet, MergeWithEmpty) {
    constexpr auto cs = ConstraintSet<>{}
        .add(Expression<128>::var("x") > Expression<128>::lit(0), "x positive");
    constexpr ConstraintSet<> empty{};

    constexpr auto merged1 = cs.merge(empty);
    static_assert(merged1.count == 1);

    constexpr auto merged2 = empty.merge(cs);
    static_assert(merged2.count == 1);
}

// --- Immutability ---

TEST(ConstraintSet, AddReturnsNewSet) {
    constexpr ConstraintSet<> cs0{};
    constexpr auto cs1 = cs0.add(Expression<128>::var("x") > Expression<128>::lit(0), "pos");
    constexpr auto cs2 = cs1.add(Expression<128>::var("y") > Expression<128>::lit(0), "pos2");

    static_assert(cs0.count == 0);
    static_assert(cs1.count == 1);
    static_assert(cs2.count == 2);
}

// --- Complex formula expressions ---

TEST(ConstraintSet, ConjunctionFormula) {
    // (x > 0) && (x < 100)
    constexpr auto formula =
        (Expression<128>::var("x") > Expression<128>::lit(0)) &&
        (Expression<128>::var("x") < Expression<128>::lit(100));
    constexpr auto cs = ConstraintSet<>{}.add(formula, "x in range");
    static_assert(cs.count == 1);

    constexpr auto f = cs.constraints[0].formula;
    static_assert(str_eq(f.ast.nodes[f.id].tag, "land"));
}

TEST(ConstraintSet, ArithmeticFormula) {
    // x + y > 0
    constexpr auto formula =
        (Expression<128>::var("x") + Expression<128>::var("y")) > Expression<128>::lit(0);
    constexpr auto cs = ConstraintSet<>{}.add(formula, "sum positive");
    static_assert(cs.count == 1);

    constexpr auto f = cs.constraints[0].formula;
    static_assert(str_eq(f.ast.nodes[f.id].tag, "gt"));
}

// --- Origin string preservation ---

TEST(ConstraintSet, LongOriginTruncated) {
    // Origin buffer is char[64], verify long strings don't overflow
    constexpr auto cs = ConstraintSet<>{}.add(
        Expression<128>::var("x") > Expression<128>::lit(0),
        "this is a very long origin string that should be truncated at 63 characters plus null");
    static_assert(cs.count == 1);
    // String should be truncated but not crash
    static_assert(cs.constraints[0].origin[63] == '\0');
}

// --- Capacity overflow ---

TEST(ConstraintSet, CapacityOverflowPrevented) {
    // Use small MaxConstraints to verify overflow guard
    constexpr auto cs = ConstraintSet<128, 2>{}
        .add(Expression<128>::var("x") > Expression<128>::lit(0), "c1")
        .add(Expression<128>::var("y") > Expression<128>::lit(0), "c2");
    static_assert(cs.count == 2);
    // Third add would throw "ConstraintSet capacity exceeded" in consteval.
    static_assert(str_eq(cs.constraints[0].origin, "c1"));
    static_assert(str_eq(cs.constraints[1].origin, "c2"));
}

// --- Structural type (NTTP compatible) ---

TEST(ConstraintSet, StructuralType) {
    constexpr auto cs = ConstraintSet<>{}.add(
        Expression<128>::var("x") > Expression<128>::lit(0), "pos");
    static_assert(cs.count == 1);
    // If this compiles, ConstraintSet is structural
    []<auto C>() {
        static_assert(C.count == 1);
    }.template operator()<cs>();
}
