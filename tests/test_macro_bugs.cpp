#include <gtest/gtest.h>
#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <refmacro/pretty_print.hpp>

using namespace refmacro;

// ============================================================================
// F1: Comparison/logical operators now auto-track macros via MacroCaller
// delegation (moved from expr.hpp to control.hpp). Auto-discovery works.
// ============================================================================

// F1a: Comparison operator preserves math macros from children.
TEST(MacroBugs_F1, ComparisonPreservesChildMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto cmp = (x + 1.0) > (x - 1.0);
    constexpr auto e = MCond(cmp, Expr::lit(1.0), Expr::lit(0.0));

    // Auto-discovery works: all macros (MAdd, MSub, MGt, MCond) are tracked.
    constexpr auto fn = compile<e>();
    static_assert(fn(5.0) == 1.0);
    static_assert(fn(0.0) == 1.0);
}

// F1b: Simple comparison auto-tracks its own macro.
TEST(MacroBugs_F1, SimpleComparisonAutoCompile) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = x < Expr::lit(5.0);
    // Auto-discovery finds MLt.
    constexpr auto fn = compile<e>();
    EXPECT_TRUE(fn(3.0));
    EXPECT_FALSE(fn(7.0));
}

// F1c: Logical operator&& auto-tracks MLand.
TEST(MacroBugs_F1, LogicalAndAutoCompile) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = x && y;
    // Auto-discovery finds MLand.
    constexpr auto fn = compile<e>();
    EXPECT_TRUE(fn(1.0, 1.0));
    EXPECT_FALSE(fn(0.0, 1.0));
}

// F1d: Unary logical (!) auto-tracks MLnot.
TEST(MacroBugs_F1, LogicalNotAutoCompile) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = !x;
    // Auto-discovery finds MLnot.
    constexpr auto fn = compile<e>();
    EXPECT_TRUE(fn(0.0));
    EXPECT_FALSE(fn(1.0));
}

// F1e: Mixed math + comparison preserves all macros.
TEST(MacroBugs_F1, MathMacrosPreservedThroughComparison) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = (x * x) == Expr::lit(4.0);
    // Auto-discovery finds both MMul and MEq.
    constexpr auto fn = compile<e>();
    EXPECT_TRUE(fn(2.0));
    EXPECT_FALSE(fn(3.0));
}

// F1f: Math operators still preserve macros (regression check).
TEST(MacroBugs_F1, MathOperatorsDoPreserveMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = x * x + 2.0 * x + 1.0;
    constexpr auto fn = compile<e>();
    static_assert(fn(3.0) == 16.0); // 9 + 6 + 1
}

// ============================================================================
// F2: Nullary MacroCaller now templates on Cap (default 64).
// ============================================================================

TEST(MacroBugs_F2, NullaryAlwaysCap64) {
    constexpr auto MTrue =
        defmacro<"true_">([] { return [](auto...) constexpr { return 1.0; }; });

    constexpr auto e = MTrue();
    constexpr auto fn = compile<e>();
    static_assert(fn() == 1.0);

    // With the fix, nullary can be called with explicit Cap:
    constexpr auto e128 = MTrue.operator()<128>();
    constexpr auto fn128 = compile<e128>();
    static_assert(fn128() == 1.0);
}

// ============================================================================
// F3: lambda(), apply(), let_() now preserve macro tracking.
// Auto-discovery works through these constructs.
// ============================================================================

// F3a: let_ preserves macros from body expression.
TEST(MacroBugs_F3, LetPreservesBodyMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto body = Expr::var("tmp") + Expr::var("tmp");
    constexpr auto e = let_("tmp", x, body);
    // Auto-discovery finds MAdd from body.
    constexpr auto fn = compile<e>();
    EXPECT_DOUBLE_EQ(fn(3.0), 6.0); // tmp=3, tmp+tmp=6
}

// F3b: lambda preserves macros from body.
TEST(MacroBugs_F3, LambdaPreservesBodyMacros) {
    constexpr auto body = Expr::var("x") + Expr::lit(1.0);
    constexpr auto lam = lambda("x", body);
    // Verify the AST is correct:
    constexpr auto s = pretty_print(lam);
    EXPECT_STREQ(s.data, "(lambda (x) (x + 1))");
    // MAdd is now preserved in the type -- apply and compile will work.
}

// F3c: apply preserves macros from both fn and arg.
TEST(MacroBugs_F3, ApplyPreservesMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto lam = lambda("tmp", Expr::var("tmp") * Expr::var("tmp"));
    constexpr auto e = apply(lam, x + 1.0);
    // Auto-discovery finds both MMul (from lambda body) and MAdd (from arg).
    constexpr auto fn = compile<e>();
    // x=2: tmp = x+1 = 3, tmp*tmp = 9
    EXPECT_DOUBLE_EQ(fn(2.0), 9.0);
}

// F3d: Nested let preserves all macro tracking.
TEST(MacroBugs_F3, NestedLetPreservesMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto a = Expr::var("a");
    constexpr auto b = Expr::var("b");
    constexpr auto e = let_("a", x + 1.0, let_("b", a * 2.0, b - a));
    // Auto-discovery finds MAdd, MMul, MSub through the let_ calls.
    constexpr auto fn = compile<e>();
    // x=3: a=4, b=8, result=8-4=4
    EXPECT_DOUBLE_EQ(fn(3.0), 4.0);
    // x=0: a=1, b=2, result=2-1=1
    EXPECT_DOUBLE_EQ(fn(0.0), 1.0);
}
