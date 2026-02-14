#include <gtest/gtest.h>

#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <reftype/refinement.hpp>

using refmacro::Expression;
using reftype::ann;
using reftype::pos_int;
using reftype::tarr;
using reftype::TBool;
using reftype::TInt;
using reftype::TReal;
using reftype::tref;
using reftype::type_check;
using reftype::typed_full_compile;
using reftype::TypeEnv;

using E = Expression<128>;

// ============================================================
// End-to-end: type check → strip → compile → run
// ============================================================

// --- Literal pipeline ---

TEST(Integration, LiteralToFunction) {
    // ann(lit(42), TInt) → compiled function returns 42
    static constexpr auto e = ann(E::lit(42), TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 42);
    EXPECT_EQ(f(), 42);
}

// --- Arithmetic pipeline ---

TEST(Integration, ArithmeticExpression) {
    // ann(lit(3) + lit(4) * lit(2), TInt) → 3 + 4*2 = 11
    static constexpr auto e = ann(E::lit(3) + E::lit(4) * E::lit(2), TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 11);
    EXPECT_EQ(f(), 11);
}

TEST(Integration, ArithmeticWithVariables) {
    // ann(x + y * lit(2), TInt) with env {x:Int, y:Int}
    static constexpr auto e = ann(E::var("x") + E::var("y") * E::lit(2), TInt);
    static constexpr auto env = TypeEnv<128>{}.bind("x", TInt).bind("y", TInt);
    constexpr auto f = reftype::typed_full_compile<e, env>();
    static_assert(f(3, 4) == 11);  // 3 + 4*2
    static_assert(f(10, 5) == 20); // 10 + 5*2
    EXPECT_EQ(f(1, 1), 3);
}

// --- Conditional pipeline ---

TEST(Integration, ConditionalExpression) {
    // cond(p, lit(1), lit(0)) annotated as TInt
    static constexpr auto cond_expr =
        refmacro::make_node<128>("cond", E::var("p"), E::lit(1), E::lit(0));
    static constexpr auto e = ann(cond_expr, TInt);
    static constexpr auto env = TypeEnv<128>{}.bind("p", TBool);
    constexpr auto f = reftype::typed_full_compile<e, env>();
    static_assert(f(true) == 1);
    static_assert(f(false) == 0);
    EXPECT_EQ(f(true), 1);
    EXPECT_EQ(f(false), 0);
}

// --- Let-binding pipeline ---

TEST(Integration, LetBinding) {
    // let x = 5 in x + x → 10
    static constexpr auto let_expr =
        refmacro::let_<128>("x", E::lit(5), E::var("x") + E::var("x"));
    static constexpr auto e = ann(let_expr, TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 10);
    EXPECT_EQ(f(), 10);
}

TEST(Integration, NestedLetBindings) {
    // let x = 3 in let y = 4 in x + y → 7
    static constexpr auto e =
        ann(refmacro::let_<128>(
                "x", E::lit(3),
                refmacro::let_<128>("y", E::lit(4), E::var("x") + E::var("y"))),
            TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 7);
    EXPECT_EQ(f(), 7);
}

// --- Lambda pipeline ---

TEST(Integration, AnnotatedLambdaApply) {
    // ann(apply(lambda("x", x+1), lit(5)), TInt) → 6
    // Direct apply(lambda, val) pattern — compiles as a let binding
    static constexpr auto lam =
        refmacro::lambda<128>("x", E::var("x") + E::lit(1));
    static constexpr auto e = ann(refmacro::apply<128>(lam, E::lit(5)), TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 6);
    EXPECT_EQ(f(), 6);
}

// --- Refinement type pipeline ---

TEST(Integration, RefinementAnnotationValid) {
    // ann(lit(5), {#v : Int | #v > 0}) — valid, compiles to 5
    static constexpr auto e = ann(E::lit(5), pos_int());
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 5);
    EXPECT_EQ(f(), 5);
}

TEST(Integration, RefinementAnnotationInvalid) {
    // ann(lit(0), {#v : Int | #v > 0}) — invalid (0 not > 0)
    static constexpr auto e = ann(E::lit(0), pos_int());
    constexpr auto r = type_check(e);
    static_assert(!r.valid);
}

TEST(Integration, RefinementAnnotationNegativeInvalid) {
    // ann(lit(-1), {#v : Int | #v > 0}) — invalid (-1 not > 0)
    static constexpr auto e = ann(E::lit(-1), pos_int());
    constexpr auto r = type_check(e);
    static_assert(!r.valid);
}

// --- Comparison pipeline ---

TEST(Integration, ComparisonCompile) {
    // ann(x > lit(0), TBool) → runtime boolean
    static constexpr auto e = ann(E::var("x") > E::lit(0), TBool);
    static constexpr auto env = TypeEnv<128>{}.bind("x", TInt);
    constexpr auto f = reftype::typed_full_compile<e, env>();
    static_assert(f(5) == true);
    static_assert(f(0) == false);
    static_assert(f(-1) == false);
    EXPECT_TRUE(f(5));
    EXPECT_FALSE(f(0));
}

// --- Logical pipeline ---

TEST(Integration, LogicalCompile) {
    // ann(p && q, TBool) → logical AND at runtime
    static constexpr auto e = ann(E::var("p") && E::var("q"), TBool);
    static constexpr auto env =
        TypeEnv<128>{}.bind("p", TBool).bind("q", TBool);
    constexpr auto f = reftype::typed_full_compile<e, env>();
    static_assert(f(true, true) == true);
    static_assert(f(true, false) == false);
    EXPECT_TRUE(f(true, true));
    EXPECT_FALSE(f(false, true));
}

// --- Negation pipeline ---

TEST(Integration, NegationCompile) {
    // ann(-x, TInt) → negated value
    static constexpr auto e = ann(-E::var("x"), TInt);
    static constexpr auto env = TypeEnv<128>{}.bind("x", TInt);
    constexpr auto f = reftype::typed_full_compile<e, env>();
    static_assert(f(5) == -5);
    static_assert(f(-3) == 3);
    EXPECT_EQ(f(7), -7);
}

// --- Complex multi-step pipeline ---

TEST(Integration, ComplexPipeline) {
    // let x = 3 in cond(x > 0, x + 1, x - 1) → 4
    static constexpr auto body = refmacro::make_node<128>(
        "cond", E::var("x") > E::lit(0), E::var("x") + E::lit(1),
        E::var("x") - E::lit(1));
    static constexpr auto e =
        ann(refmacro::let_<128>("x", E::lit(3), body), TInt);
    constexpr auto f = typed_full_compile<e>();
    static_assert(f() == 4);
    EXPECT_EQ(f(), 4);
}

// --- Type check failure cases (soft failures) ---

TEST(Integration, TypeMismatchAnnotation) {
    // ann(lit(5), TBool) — Int literal annotated as Bool → invalid
    static constexpr auto e = ann(E::lit(5), TBool);
    constexpr auto r = type_check(e);
    static_assert(!r.valid);
}

TEST(Integration, LambdaOutputMismatch) {
    // ann(lambda("x", x+1), (x:Int)->Bool) — body is Int, output wants Bool
    static constexpr auto lam =
        refmacro::lambda<128>("x", E::var("x") + E::lit(1));
    static constexpr auto arrow = tarr("x", TInt, TBool);
    static constexpr auto e = ann(lam, arrow);
    constexpr auto r = type_check(e);
    static_assert(!r.valid);
}

// --- Subtype lattice in pipeline ---

TEST(Integration, IntSubtypeOfReal) {
    // ann(lit(5), TReal) — {#v:Int|#v==5} <: Real via Int <: Real, valid
    static constexpr auto e = ann(E::lit(5), TReal);
    constexpr auto r = type_check(e);
    static_assert(r.valid);
}

// --- Typed vs untyped equivalence ---

TEST(Integration, TypedMatchesUntyped) {
    // Same expression through typed and untyped pipelines produces same result
    static constexpr auto untyped_expr = E::lit(10) - E::lit(3) * E::lit(2);
    static constexpr auto typed_expr =
        ann(E::lit(10) - E::lit(3) * E::lit(2), TInt);

    constexpr auto f_untyped = refmacro::full_compile<untyped_expr>();
    constexpr auto f_typed = typed_full_compile<typed_expr>();

    static_assert(f_untyped() == f_typed());
    static_assert(f_typed() == 4); // 10 - 3*2 = 4
    EXPECT_EQ(f_untyped(), f_typed());
}
