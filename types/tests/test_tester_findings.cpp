// Bug-finding reproduction tests for Fragment 1: Core + Type System
// Created by review-tester-1 as part of code review
//
// These tests target edge cases and potential issues found during
// adversarial analysis of the type system implementation.

#include <gtest/gtest.h>

#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <reftype/check.hpp>
#include <reftype/pretty.hpp>
#include <reftype/strip.hpp>
#include <reftype/subtype.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using reftype::ann;
using reftype::BaseKind;
using reftype::get_base_kind;
using reftype::is_subtype;
using reftype::join;
using reftype::pos_int;
using reftype::strip_types;
using reftype::tarr;
using reftype::TBool;
using reftype::TInt;
using reftype::TReal;
using reftype::tref;
using reftype::type_check;
using reftype::TypeEnv;
using reftype::types_equal;

using E = Expression<128>;

// ============================================================
// F2: is_subtype(Bool, {#v:Int|Q}) false-negative when Q
//     holds for {0,1} but not all integers
// ============================================================

// Bool <: {#v : Int | #v == 0 || #v == 1} should ideally be true:
// all boolean values (0 and 1) satisfy (#v == 0 || #v == 1).
// However, the implementation checks Q over all integers (super's domain),
// not over booleans (sub's domain), so it returns false.
//
// This test documents the known limitation. If the implementation is fixed
// in the future to use the sub's domain, this test should be updated.
TEST(TesterFindings_F2, BoolSubtypeIntRefinedFalseNegative) {
    // Predicate: #v == 0 || #v == 1 — true for all booleans
    constexpr auto super =
        tref(TInt, E::var("#v") == E::lit(0) || E::var("#v") == E::lit(1));

    // Current behavior: returns false (checking Q over all integers)
    // Expected (mathematically): true (all bools satisfy Q)
    // This static_assert documents the CURRENT behavior:
    static_assert(!is_subtype(TBool, super),
                  "F2: Bool <: {#v:Int|#v==0||#v==1} returns false "
                  "(known limitation: checks Q over integers, not booleans)");
}

// Stronger example: Bool <: {#v : Int | #v >= 0 && #v <= 1}
// This should be true (0 and 1 both satisfy 0 <= #v <= 1)
// but the implementation returns false.
TEST(TesterFindings_F2, BoolSubtypeIntBoundedRangeFalseNegative) {
    constexpr auto super =
        tref(TInt, E::var("#v") >= E::lit(0) && E::var("#v") <= E::lit(1));
    static_assert(!is_subtype(TBool, super),
                  "F2: Bool <: {#v:Int|0<=#v<=1} returns false "
                  "(checking Q over all integers)");
}

// F1: Int + Bool rejection is covered by compile_fail_bool_arith.cpp.

// ============================================================
// Gap: join(TBool, {#v:Int|#v>0}) behavior
// ============================================================

// join(TBool, {#v:Int|#v>0}) should produce Int (base + refined → wider base)
TEST(TesterFindings_Gap, JoinBoolAndRefinedInt) {
    constexpr auto ref = tref(TInt, E::var("#v") > E::lit(0));
    // join(base, refined) → wider_base(base, refined_base)
    // wider_base(TBool, TInt) → TInt
    constexpr auto result = join(TBool, ref);
    static_assert(types_equal(result, TInt),
                  "join(Bool, {#v:Int|#v>0}) should be Int");
}

TEST(TesterFindings_Gap, JoinRefinedIntAndBool) {
    constexpr auto ref = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto result = join(ref, TBool);
    static_assert(types_equal(result, TInt),
                  "join({#v:Int|#v>0}, Bool) should be Int");
}

// join({#v:Bool|pred}, {#v:Int|pred}) — refined + refined, different bases
TEST(TesterFindings_Gap, JoinRefinedBoolAndRefinedInt) {
    constexpr auto t1 = tref(TBool, E::var("#v") == E::lit(0));
    constexpr auto t2 = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto result = join(t1, t2);
    // wider_base(TBool, TInt) = TInt → {#v:Int | pred1 || pred2}
    static_assert(reftype::is_refined(result));
    static_assert(types_equal(reftype::get_refined_base(result), TInt));
}

// ============================================================
// Gap: strip_types preserves progn with annotations inside
// ============================================================

TEST(TesterFindings_Gap, StripTypesPrognWithAnnotations) {
    // progn(ann(lit(1), TInt), ann(lit(2), TInt)) → progn(lit(1), lit(2))
    static constexpr auto input = refmacro::make_node<128>(
        "progn", ann(E::lit(1), TInt), ann(E::lit(2), TInt));
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected =
        refmacro::make_node<128>("progn", E::lit(1), E::lit(2));
    static_assert(types_equal(stripped, expected),
                  "strip_types should remove annotations inside progn");
}

// ============================================================
// Gap: synth for negation with refined operand
// ============================================================

TEST(TesterFindings_Gap, NegRefinedInt) {
    // neg(x) where x : {#v : Int | #v > 0} → result type should be Int
    constexpr auto nat = tref(TInt, E::var("#v") > E::lit(0));
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", nat);
    constexpr auto neg = refmacro::make_node<128>("neg", E::var("x"));
    constexpr auto r = type_check(neg, env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt),
                  "neg on refined Int should produce Int");
}

// ============================================================
// Gap: synth for comparison with refined operands
// ============================================================

TEST(TesterFindings_Gap, ComparisonRefinedOperands) {
    // x > y where x : {#v >= 0}, y : {#v >= 0} → Bool
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", nat).bind("y", nat);
    constexpr auto r = type_check(E::var("x") > E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool),
                  "comparison of refined operands should produce Bool");
}

// ============================================================
// Gap: join reflexivity for refined types
// ============================================================

TEST(TesterFindings_Gap, JoinReflexiveRefined) {
    constexpr auto t = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto result = join(t, t);
    static_assert(types_equal(result, t),
                  "join(t, t) should return t for refined types");
}

// ============================================================
// Gap: is_subtype reflexivity for arrow with refined types
// ============================================================

TEST(TesterFindings_Gap, SubtypeReflexiveRefinedArrow) {
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr auto t = tarr("x", nat, nat);
    static_assert(
        is_subtype(t, t),
        "arrow type with refined components should be reflexive subtype");
}

// ============================================================
// Gap: type_check for conditional with refined branches
// ============================================================

TEST(TesterFindings_Gap, CondRefinedBranches) {
    // cond(p, x:{#v>0}, y:{#v<0}) → join = {#v:Int | #v>0 || #v<0}
    constexpr auto pos = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto neg = tref(TInt, E::var("#v") < E::lit(0));
    constexpr TypeEnv<128> env =
        TypeEnv<128>{}.bind("p", TBool).bind("x", pos).bind("y", neg);
    constexpr auto e =
        refmacro::make_node<128>("cond", E::var("p"), E::var("x"), E::var("y"));
    constexpr auto r = type_check(e, env);
    static_assert(r.valid);
    // Result should be refined (disjunction of predicates)
    static_assert(reftype::is_refined(r.type),
                  "cond with refined branches should produce refined result");
}
