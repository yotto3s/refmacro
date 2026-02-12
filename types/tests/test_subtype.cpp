#include <gtest/gtest.h>

#include <reftype/pretty.hpp>
#include <reftype/subtype.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using reftype::get_arrow_input;
using reftype::get_arrow_output;
using reftype::get_refined_base;
using reftype::get_refined_pred;
using reftype::is_arrow;
using reftype::is_base;
using reftype::is_refined;
using reftype::is_subtype;
using reftype::join;
using reftype::tarr;
using reftype::TBool;
using reftype::TInt;
using reftype::TReal;
using reftype::tref;
using reftype::types_equal;

using E = Expression<128>;

// --- AST classifiers ---

TEST(SubtypeHelpers, IsBase) {
    static_assert(is_base(TInt));
    static_assert(is_base(TBool));
    static_assert(is_base(TReal));
    static_assert(!is_base(tref(TInt, E::var("#v") > E::lit(0))));
    static_assert(!is_base(tarr("x", TInt, TBool)));
}

TEST(SubtypeHelpers, IsRefined) {
    static_assert(is_refined(tref(TInt, E::var("#v") > E::lit(0))));
    static_assert(!is_refined(TInt));
    static_assert(!is_refined(tarr("x", TInt, TBool)));
}

TEST(SubtypeHelpers, IsArrow) {
    static_assert(is_arrow(tarr("x", TInt, TBool)));
    static_assert(!is_arrow(TInt));
    static_assert(!is_arrow(tref(TInt, E::var("#v") > E::lit(0))));
}

// --- AST accessors ---

TEST(SubtypeHelpers, GetRefinedBase) {
    constexpr auto t = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(types_equal(get_refined_base(t), TInt));
}

TEST(SubtypeHelpers, GetRefinedPred) {
    constexpr auto pred = E::var("#v") > E::lit(0);
    constexpr auto t = tref(TInt, pred);
    // The predicate's root tag should be "gt"
    constexpr auto p = get_refined_pred(t);
    static_assert(refmacro::str_eq(p.ast.nodes[p.id].tag, "gt"));
}

TEST(SubtypeHelpers, GetArrowInputOutput) {
    constexpr auto t = tarr("x", TInt, TBool);
    static_assert(types_equal(get_arrow_input(t), TInt));
    static_assert(types_equal(get_arrow_output(t), TBool));
}

// --- Structural equality ---

TEST(TypesEqual, SameBaseType) {
    static_assert(types_equal(TInt, TInt));
    static_assert(types_equal(TBool, TBool));
    static_assert(types_equal(TReal, TReal));
}

TEST(TypesEqual, DifferentBaseType) {
    static_assert(!types_equal(TInt, TBool));
    static_assert(!types_equal(TInt, TReal));
    static_assert(!types_equal(TBool, TReal));
}

TEST(TypesEqual, SameRefinement) {
    constexpr auto a = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto b = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(types_equal(a, b));
}

TEST(TypesEqual, DifferentPredicate) {
    constexpr auto a = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto b = tref(TInt, E::var("#v") > E::lit(5));
    static_assert(!types_equal(a, b));
}

TEST(TypesEqual, SameArrow) {
    constexpr auto a = tarr("x", TInt, TBool);
    constexpr auto b = tarr("x", TInt, TBool);
    static_assert(types_equal(a, b));
}

TEST(TypesEqual, DifferentArrow) {
    constexpr auto a = tarr("x", TInt, TBool);
    constexpr auto b = tarr("x", TInt, TReal);
    static_assert(!types_equal(a, b));
}

// --- Base widening ---

TEST(Subtype, BaseWidening) {
    static_assert(is_subtype(TBool, TInt));
    static_assert(is_subtype(TInt, TReal));
    static_assert(is_subtype(TBool, TReal));
}

TEST(Subtype, BaseWideningFails) {
    static_assert(!is_subtype(TInt, TBool));
    static_assert(!is_subtype(TReal, TInt));
    static_assert(!is_subtype(TReal, TBool));
}

// --- Reflexivity ---

TEST(Subtype, Reflexivity) {
    static_assert(is_subtype(TInt, TInt));
    static_assert(is_subtype(TBool, TBool));
    static_assert(is_subtype(TReal, TReal));
}

TEST(Subtype, ReflexivityRefined) {
    constexpr auto t = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(is_subtype(t, t));
}

TEST(Subtype, ReflexivityArrow) {
    constexpr auto t = tarr("x", TInt, TBool);
    static_assert(is_subtype(t, t));
}

// --- Refined <: Refined (same base) ---

TEST(Subtype, RefinedImplicationPass) {
    // {#v > 0 && #v < 10} <: {#v >= 0}
    constexpr auto sub =
        tref(TInt, E::var("#v") > E::lit(0) && E::var("#v") < E::lit(10));
    constexpr auto super = tref(TInt, E::var("#v") >= E::lit(0));
    static_assert(is_subtype(sub, super));
}

TEST(Subtype, RefinedImplicationFail) {
    // {#v > 0} </: {#v > 5}
    constexpr auto sub = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto super = tref(TInt, E::var("#v") > E::lit(5));
    static_assert(!is_subtype(sub, super));
}

TEST(Subtype, RefinedEqualityImplication) {
    // {#v == 5} <: {#v > 0}
    constexpr auto sub = tref(TInt, E::var("#v") == E::lit(5));
    constexpr auto super = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(is_subtype(sub, super));
}

TEST(Subtype, RefinedRange) {
    // {#v >= 1 && #v <= 5} <: {#v > 0 && #v < 10}
    constexpr auto sub =
        tref(TInt, E::var("#v") >= E::lit(1) && E::var("#v") <= E::lit(5));
    constexpr auto super =
        tref(TInt, E::var("#v") > E::lit(0) && E::var("#v") < E::lit(10));
    static_assert(is_subtype(sub, super));
}

// --- Refined <: Unrefined ---

TEST(Subtype, RefinedToUnrefined) {
    // {#v : Int | #v > 0} <: Int
    constexpr auto sub = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(is_subtype(sub, TInt));
}

TEST(Subtype, RefinedToWiderUnrefined) {
    // {#v : Int | #v > 0} <: Real
    constexpr auto sub = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(is_subtype(sub, TReal));
}

TEST(Subtype, RefinedToIncompatibleUnrefined) {
    // {#v : Real | #v > 0} </: Int
    constexpr auto sub = tref(TReal, E::var("#v") > E::lit(0));
    static_assert(!is_subtype(sub, TInt));
}

// --- Unrefined <: Refined ---

TEST(Subtype, UnrefinedToRefinedFails) {
    // Int </: {#v : Int | #v > 0} (not all ints are positive)
    constexpr auto super = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(!is_subtype(TInt, super));
}

// --- Refinement with base widening ---

TEST(Subtype, RefinedBaseWidening) {
    // {#v : Int | #v > 0} <: {#v : Real | #v > 0}
    constexpr auto sub = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto super = tref(TReal, E::var("#v") > E::lit(0));
    static_assert(is_subtype(sub, super));
}

TEST(Subtype, RefinedBaseWideningFails) {
    // {#v : Real | #v > 0} </: {#v : Int | #v > 0}
    constexpr auto sub = tref(TReal, E::var("#v") > E::lit(0));
    constexpr auto super = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(!is_subtype(sub, super));
}

// --- Arrow subtyping ---

TEST(Subtype, ArrowCovariant) {
    // (x : Int) -> Nat <: (x : Int) -> Int (covariant output)
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr auto sub = tarr("x", TInt, nat);
    constexpr auto super = tarr("x", TInt, TInt);
    static_assert(is_subtype(sub, super));
}

TEST(Subtype, ArrowCovariantFails) {
    // (x : Int) -> Int </: (x : Int) -> Nat
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr auto sub = tarr("x", TInt, TInt);
    constexpr auto super = tarr("x", TInt, nat);
    static_assert(!is_subtype(sub, super));
}

TEST(Subtype, ArrowContravariant) {
    // (x : Nat) -> Int <: (x : Int) -> Int
    // Wait — contravariant means (x : wider_input) -> ... <: (x : narrower_input) -> ...
    // Actually: (A2 → B1) <: (A1 → B2) iff A1 <: A2 and B1 <: B2
    // So: (x : Int) -> Int <: (x : Nat) -> Int iff Nat <: Int (yes) and Int <: Int (yes)
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr auto sub = tarr("x", TInt, TInt);
    constexpr auto super = tarr("x", nat, TInt);
    static_assert(is_subtype(sub, super));
}

TEST(Subtype, ArrowContravariantFails) {
    // (x : Nat) -> Int </: (x : Int) -> Int
    // Requires Int <: Nat, which is false
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr auto sub = tarr("x", nat, TInt);
    constexpr auto super = tarr("x", TInt, TInt);
    static_assert(!is_subtype(sub, super));
}

// --- Incompatible types ---

TEST(Subtype, ArrowVsBase) {
    static_assert(!is_subtype(tarr("x", TInt, TInt), TInt));
    static_assert(!is_subtype(TInt, tarr("x", TInt, TInt)));
}

TEST(Subtype, ArrowVsRefined) {
    constexpr auto ref = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(!is_subtype(tarr("x", TInt, TInt), ref));
    static_assert(!is_subtype(ref, tarr("x", TInt, TInt)));
}

// --- Join ---

TEST(Join, SameType) {
    static_assert(types_equal(join(TInt, TInt), TInt));
    static_assert(types_equal(join(TBool, TBool), TBool));
    static_assert(types_equal(join(TReal, TReal), TReal));
}

TEST(Join, BaseWidening) {
    static_assert(types_equal(join(TInt, TReal), TReal));
    static_assert(types_equal(join(TReal, TInt), TReal));
    static_assert(types_equal(join(TBool, TInt), TInt));
    static_assert(types_equal(join(TInt, TBool), TInt));
    static_assert(types_equal(join(TBool, TReal), TReal));
    static_assert(types_equal(join(TReal, TBool), TReal));
}

TEST(Join, RefinedDisjunction) {
    // join({#v > 0}, {#v < 0}) = {#v : Int | #v > 0 || #v < 0}
    constexpr auto t1 = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto t2 = tref(TInt, E::var("#v") < E::lit(0));
    constexpr auto result = join(t1, t2);
    static_assert(is_refined(result));
    static_assert(types_equal(get_refined_base(result), TInt));
    // Predicate root should be "lor" (disjunction)
    constexpr auto pred = get_refined_pred(result);
    static_assert(refmacro::str_eq(pred.ast.nodes[pred.id].tag, "lor"));
}

TEST(Join, RefinedDropsRefinement) {
    // join(TInt, {#v > 0}) = TInt
    constexpr auto ref = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(types_equal(join(TInt, ref), TInt));
    static_assert(types_equal(join(ref, TInt), TInt));
}

TEST(Join, RefinedWithBaseWidening) {
    // join({#v:Int|#v>0}, {#v:Real|#v<0}) = {#v:Real|...}
    constexpr auto t1 = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto t2 = tref(TReal, E::var("#v") < E::lit(0));
    constexpr auto result = join(t1, t2);
    static_assert(is_refined(result));
    static_assert(types_equal(get_refined_base(result), TReal));
}

TEST(Join, RefinedAndWiderBase) {
    // join(TBool, {#v:Int|#v>0}) = Int (drop refinement, widen)
    constexpr auto ref = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(types_equal(join(TBool, ref), TInt));
    static_assert(types_equal(join(ref, TBool), TInt));
}

// --- Join: Bool refinements ---

TEST(Join, BoolRefinedDisjunction) {
    // join({#v:Bool|#v==0}, {#v:Bool|#v==1}) = {#v:Bool|...}
    constexpr auto t1 = tref(TBool, E::var("#v") == E::lit(0));
    constexpr auto t2 = tref(TBool, E::var("#v") == E::lit(1));
    constexpr auto result = join(t1, t2);
    static_assert(is_refined(result));
    static_assert(types_equal(get_refined_base(result), TBool));
}

// --- Join used by subtype (soundness check) ---

TEST(Join, ResultIsSupertypeOfBoth) {
    constexpr auto t1 = tref(TInt, E::var("#v") > E::lit(0));
    constexpr auto t2 = tref(TInt, E::var("#v") < E::lit(0));
    constexpr auto lub = join(t1, t2);
    // Both should be subtypes of the join
    static_assert(is_subtype(t1, lub));
    static_assert(is_subtype(t2, lub));
}

// --- Cross-base refinement subtyping (bug fix: VarInfo from super's base) ---

TEST(Subtype, IntToRealRefinedHalfOpen) {
    // Int <: {#v : Real | #v > 0.5} should be false
    // (not all ints satisfy #v > 0.5; e.g. 0)
    // With correct VarInfo (#v as real), FM treats #v > 0.5 correctly
    constexpr auto super = tref(TReal, E::var("#v") > E::lit(0.5));
    static_assert(!is_subtype(TInt, super));
}

TEST(Subtype, BoolToIntRefined) {
    // Bool <: {#v : Int | #v >= 0} should be false
    // (all ints >= 0? No, so Q is not valid for all integers)
    constexpr auto super = tref(TInt, E::var("#v") >= E::lit(0));
    static_assert(!is_subtype(TBool, super));
}
