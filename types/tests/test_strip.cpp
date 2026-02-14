#include <gtest/gtest.h>

#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <reftype/strip.hpp>
#include <reftype/subtype.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using reftype::ann;
using reftype::pos_int;
using reftype::strip_types;
using reftype::TBool;
using reftype::TInt;
using reftype::tref;
using reftype::typed_compile;
using reftype::typed_full_compile;
using reftype::types_equal;

using E = Expression<128>;

// ============================================================
// strip_types: basic stripping
// ============================================================

TEST(StripTypes, LiteralAnnotation) {
    // ann(lit(5), TInt) → lit(5)
    static constexpr auto input = ann(E::lit(5), TInt);
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected = E::lit(5);
    static_assert(types_equal(stripped, expected));
}

TEST(StripTypes, VariableAnnotation) {
    // ann(var("x"), TInt) → var("x")
    static constexpr auto input = ann(E::var("x"), TInt);
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected = E::var("x");
    static_assert(types_equal(stripped, expected));
}

TEST(StripTypes, RefinedAnnotation) {
    // ann(lit(5), {#v : Int | #v > 0}) → lit(5)
    static constexpr auto input = ann(E::lit(5), pos_int());
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected = E::lit(5);
    static_assert(types_equal(stripped, expected));
}

TEST(StripTypes, ArithmeticPreserved) {
    // ann(var("x") + lit(1), TInt) → var("x") + lit(1)
    static constexpr auto input = ann(E::var("x") + E::lit(1), TInt);
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected = E::var("x") + E::lit(1);
    static_assert(types_equal(stripped, expected));
}

TEST(StripTypes, NestedAnnotations) {
    // ann(ann(lit(5), TInt) + lit(1), TInt) → lit(5) + lit(1)
    static constexpr auto inner_ann = ann(E::lit(5), TInt);
    static constexpr auto input = ann(inner_ann + E::lit(1), TInt);
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected = E::lit(5) + E::lit(1);
    static_assert(types_equal(stripped, expected));
}

TEST(StripTypes, DeepNestedAnnotations) {
    // ann(ann(ann(lit(3), TInt), TInt), TInt) → lit(3)
    static constexpr auto input = ann(ann(ann(E::lit(3), TInt), TInt), TInt);
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected = E::lit(3);
    static_assert(types_equal(stripped, expected));
}

TEST(StripTypes, UnannotatedExpression) {
    // No annotations — expression passes through unchanged
    static constexpr auto input = E::var("x") + E::lit(1);
    static constexpr auto stripped = strip_types(input);
    static_assert(types_equal(stripped, input));
}

TEST(StripTypes, LiteralOnly) {
    // Just a literal, no annotation
    static constexpr auto input = E::lit(42);
    static constexpr auto stripped = strip_types(input);
    static_assert(types_equal(stripped, input));
}

TEST(StripTypes, PreservesComparison) {
    // ann(x > lit(0), TBool) → x > lit(0)
    static constexpr auto input = ann(E::var("x") > E::lit(0), TBool);
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected = E::var("x") > E::lit(0);
    static_assert(types_equal(stripped, expected));
}

TEST(StripTypes, PreservesLogical) {
    // ann(p && q, TBool) → p && q
    static constexpr auto input =
        ann(E::var("p") && E::var("q"), TBool);
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected = E::var("p") && E::var("q");
    static_assert(types_equal(stripped, expected));
}

TEST(StripTypes, PreservesConditional) {
    // cond(p, lit(1), lit(2)) — no annotations, passes through
    static constexpr auto input =
        refmacro::make_node<128>("cond", E::var("p"), E::lit(1), E::lit(2));
    static constexpr auto stripped = strip_types(input);
    static_assert(types_equal(stripped, input));
}

TEST(StripTypes, PreservesNeg) {
    // neg(x) → neg(x)
    static constexpr auto input = -E::var("x");
    static constexpr auto stripped = strip_types(input);
    static_assert(types_equal(stripped, input));
}

TEST(StripTypes, PreservesLetBinding) {
    // let x = lit(5) in x + lit(1) — no annotations
    static constexpr auto input =
        refmacro::let_<128>("x", E::lit(5), E::var("x") + E::lit(1));
    static constexpr auto stripped = strip_types(input);
    static_assert(types_equal(stripped, input));
}

TEST(StripTypes, AnnotatedLetBinding) {
    // ann(let x = lit(5) in x + lit(1), TInt) → let x = lit(5) in x + lit(1)
    static constexpr auto let_expr =
        refmacro::let_<128>("x", E::lit(5), E::var("x") + E::lit(1));
    static constexpr auto input = ann(let_expr, TInt);
    static constexpr auto stripped = strip_types(input);
    static_assert(types_equal(stripped, let_expr));
}

TEST(StripTypes, AnnotationInsideArithmetic) {
    // ann(lit(5), TInt) + ann(lit(3), TInt) → lit(5) + lit(3)
    static constexpr auto input = ann(E::lit(5), TInt) + ann(E::lit(3), TInt);
    static constexpr auto stripped = strip_types(input);
    static constexpr auto expected = E::lit(5) + E::lit(3);
    static_assert(types_equal(stripped, expected));
}

TEST(StripTypes, AnnotatedLambda) {
    // ann(lambda("x", x+1), tarr("x", TInt, TInt)) → lambda("x", x+1)
    static constexpr auto body = E::var("x") + E::lit(1);
    static constexpr auto lam = refmacro::lambda<128>("x", body);
    static constexpr auto arrow_type =
        reftype::tarr("x", TInt, TInt);
    static constexpr auto input = ann(lam, arrow_type);
    static constexpr auto stripped = strip_types(input);
    static_assert(types_equal(stripped, lam));
}

// ============================================================
// typed_compile: type check + strip + compile
// ============================================================

TEST(TypedCompile, LiteralConstant) {
    // ann(lit(5), TInt) → compiles to constant 5
    static constexpr auto e = ann(E::lit(5), TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 5);
}

TEST(TypedCompile, LiteralArithmetic) {
    // ann(lit(2) + lit(3), TInt) → compiles to constant 5
    static constexpr auto e = ann(E::lit(2) + E::lit(3), TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 5);
}

TEST(TypedCompile, NestedAnnotationCompile) {
    // ann(ann(lit(5), TInt) + lit(1), TInt) → compiles to 6
    static constexpr auto e =
        ann(ann(E::lit(5), TInt) + E::lit(1), TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 6);
}

TEST(TypedCompile, VariableExpression) {
    // ann(var("x") + lit(1), TInt) with env {x: Int} → f(3) == 4
    static constexpr auto e = ann(E::var("x") + E::lit(1), TInt);
    // typed_compile with math macros, providing env for type checking
    constexpr auto result = reftype::type_check(e, reftype::TypeEnv<128>{}.bind("x", TInt));
    static_assert(result.valid);
    static constexpr auto stripped = strip_types(e);
    constexpr auto f = refmacro::compile<stripped, refmacro::MAdd>();
    static_assert(f(3) == 4);
    static_assert(f(10) == 11);
}

TEST(TypedCompile, LetBindingCompile) {
    // let x = 5 in x + 1 → compiles to 6
    static constexpr auto let_expr =
        refmacro::let_<128>("x", E::lit(5), E::var("x") + E::lit(1));
    static constexpr auto e = ann(let_expr, TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 6);
}

TEST(TypedCompile, RefinedAnnotationCompile) {
    // ann(lit(5), {#v : Int | #v > 0}) → compiles to 5
    static constexpr auto e = ann(E::lit(5), pos_int());
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 5);
}

// ============================================================
// Pipeline: same results as untyped compile
// ============================================================

TEST(Pipeline, MatchesUntypedCompile) {
    // Compare typed vs untyped pipeline for the same expression
    // Expression: lit(3) + lit(4)
    static constexpr auto untyped = E::lit(3) + E::lit(4);
    static constexpr auto typed = ann(E::lit(3) + E::lit(4), TInt);

    constexpr auto f_untyped = refmacro::compile<untyped, refmacro::MAdd>();
    constexpr auto f_typed = typed_full_compile<typed>();

    static_assert(f_untyped() == f_typed());
    static_assert(f_typed() == 7);
}

TEST(Pipeline, VariableMatchesUntyped) {
    // var("x") * var("y") — typed vs untyped should produce same results
    static constexpr auto untyped = E::var("x") * E::var("y");
    static constexpr auto typed = ann(E::var("x") * E::var("y"), TInt);

    // For the typed version, we need to strip manually since it has free vars
    static constexpr auto stripped = strip_types(typed);

    constexpr auto f_untyped = refmacro::compile<untyped, refmacro::MMul>();
    constexpr auto f_typed = refmacro::compile<stripped, refmacro::MMul>();

    static_assert(f_untyped(3, 4) == f_typed(3, 4));
    static_assert(f_typed(5, 6) == 30);
}

TEST(Pipeline, ConditionalCompile) {
    // cond(p, lit(1), lit(2)) annotated as TInt
    static constexpr auto cond_expr =
        refmacro::make_node<128>("cond", E::var("p"), E::lit(1), E::lit(2));
    static constexpr auto typed = ann(cond_expr, TInt);

    // Verify type check with env
    constexpr auto result = reftype::type_check(
        typed, reftype::TypeEnv<128>{}.bind("p", TBool));
    static_assert(result.valid);

    // Strip and compile
    static constexpr auto stripped = strip_types(typed);
    constexpr auto f = refmacro::compile<stripped, refmacro::MCond>();

    // p=true → 1, p=false → 2
    static_assert(f(true) == 1);
    static_assert(f(false) == 2);
}
