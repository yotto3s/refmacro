#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <refmacro/pretty_print.hpp>
#include <gtest/gtest.h>

using namespace refmacro;

// --- Simple macro tests ---

TEST(ControlMacros, CondTrueBranch) {
    // cond(true, 1.0, 2.0) -> 1.0
    constexpr auto e = MCond(Expr<>::lit(1.0), Expr<>::lit(10.0), Expr<>::lit(20.0));
    constexpr auto fn = ctrl_compile<e>();
    static_assert(fn() == 10.0);
    EXPECT_DOUBLE_EQ(fn(), 10.0);
}

TEST(ControlMacros, CondFalseBranch) {
    constexpr auto e = MCond(Expr<>::lit(0.0), Expr<>::lit(10.0), Expr<>::lit(20.0));
    constexpr auto fn = ctrl_compile<e>();
    static_assert(fn() == 20.0);
    EXPECT_DOUBLE_EQ(fn(), 20.0);
}

TEST(ControlMacros, CondWithVars) {
    // abs(x): cond(x < 0, -x, x)
    constexpr auto x = Expr<>::var("x");
    constexpr auto abs_x = MCond(x < Expr<>::lit(0.0), -x, x);
    constexpr auto fn = full_compile<abs_x>();
    static_assert(fn(-3.0) == 3.0);
    static_assert(fn(5.0) == 5.0);
    EXPECT_DOUBLE_EQ(fn(-3.0), 3.0);
    EXPECT_DOUBLE_EQ(fn(5.0), 5.0);
}

TEST(ControlMacros, LogicalAnd) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = x && y;
    constexpr auto fn = ctrl_compile<e>();
    // Both truthy
    EXPECT_TRUE(fn(1.0, 1.0));
    // One falsy
    EXPECT_FALSE(fn(0.0, 1.0));
    EXPECT_FALSE(fn(1.0, 0.0));
}

TEST(ControlMacros, LogicalOr) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = x || y;
    constexpr auto fn = ctrl_compile<e>();
    EXPECT_TRUE(fn(1.0, 0.0));
    EXPECT_TRUE(fn(0.0, 1.0));
    EXPECT_FALSE(fn(0.0, 0.0));
}

TEST(ControlMacros, LogicalNot) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = !x;
    constexpr auto fn = ctrl_compile<e>();
    EXPECT_TRUE(fn(0.0));
    EXPECT_FALSE(fn(1.0));
}

TEST(ControlMacros, EqualityComparison) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = x == y;
    constexpr auto fn = ctrl_compile<e>();
    EXPECT_TRUE(fn(3.0, 3.0));
    EXPECT_FALSE(fn(3.0, 4.0));
}

TEST(ControlMacros, LessThan) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = x < Expr<>::lit(5.0);
    constexpr auto fn = ctrl_compile<e>();
    EXPECT_TRUE(fn(3.0));
    EXPECT_FALSE(fn(5.0));
    EXPECT_FALSE(fn(7.0));
}

TEST(ControlMacros, GreaterThan) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = x > Expr<>::lit(5.0);
    constexpr auto fn = ctrl_compile<e>();
    EXPECT_FALSE(fn(3.0));
    EXPECT_FALSE(fn(5.0));
    EXPECT_TRUE(fn(7.0));
}

TEST(ControlMacros, LessEqual) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = x <= Expr<>::lit(5.0);
    constexpr auto fn = ctrl_compile<e>();
    EXPECT_TRUE(fn(3.0));
    EXPECT_TRUE(fn(5.0));
    EXPECT_FALSE(fn(7.0));
}

TEST(ControlMacros, GreaterEqual) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = x >= Expr<>::lit(5.0);
    constexpr auto fn = ctrl_compile<e>();
    EXPECT_FALSE(fn(3.0));
    EXPECT_TRUE(fn(5.0));
    EXPECT_TRUE(fn(7.0));
}

TEST(ControlMacros, Progn) {
    // progn evaluates both, returns second
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = MProgn(x, y);
    constexpr auto fn = ctrl_compile<e>();
    EXPECT_DOUBLE_EQ(fn(10.0, 20.0), 20.0);
}

TEST(ControlMacros, ComparisonWithDouble) {
    // Test double-on-LHS and double-on-RHS overloads
    constexpr auto x = Expr<>::var("x");

    constexpr auto e1 = 5.0 == x;
    constexpr auto fn1 = ctrl_compile<e1>();
    EXPECT_TRUE(fn1(5.0));

    constexpr auto e2 = 5.0 < x;
    constexpr auto fn2 = ctrl_compile<e2>();
    EXPECT_TRUE(fn2(6.0));
    EXPECT_FALSE(fn2(4.0));
}

// --- Pretty-print tests ---

TEST(ControlPrettyPrint, Cond) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = MCond(x, Expr<>::lit(1.0), Expr<>::lit(2.0));
    constexpr auto s = pretty_print(e);
    EXPECT_STREQ(s.data, "(cond x 1 2)");
}

TEST(ControlPrettyPrint, LogicalOps) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");

    constexpr auto s_and = pretty_print(x && y);
    EXPECT_STREQ(s_and.data, "(x && y)");

    constexpr auto s_or = pretty_print(x || y);
    EXPECT_STREQ(s_or.data, "(x || y)");

    constexpr auto s_not = pretty_print(!x);
    EXPECT_STREQ(s_not.data, "(!x)");
}

TEST(ControlPrettyPrint, Comparisons) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");

    constexpr auto s_eq = pretty_print(x == y);
    EXPECT_STREQ(s_eq.data, "(x == y)");

    constexpr auto s_lt = pretty_print(x < y);
    EXPECT_STREQ(s_lt.data, "(x < y)");

    constexpr auto s_gt = pretty_print(x > y);
    EXPECT_STREQ(s_gt.data, "(x > y)");

    constexpr auto s_le = pretty_print(x <= y);
    EXPECT_STREQ(s_le.data, "(x <= y)");

    constexpr auto s_ge = pretty_print(x >= y);
    EXPECT_STREQ(s_ge.data, "(x >= y)");
}

TEST(ControlPrettyPrint, Progn) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto s = pretty_print(MProgn(x, y));
    EXPECT_STREQ(s.data, "(progn x y)");
}

// --- Combined math + control ---

TEST(ControlMacros, FullCompileAbsValue) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto abs_x = MCond(x < Expr<>::lit(0.0), -x, x);
    constexpr auto fn = full_compile<abs_x>();
    EXPECT_DOUBLE_EQ(fn(-7.0), 7.0);
    EXPECT_DOUBLE_EQ(fn(3.0), 3.0);
    EXPECT_DOUBLE_EQ(fn(0.0), 0.0);
}

TEST(ControlMacros, SafeDiv) {
    // Note: DFS visits y first (in y == 0 check), so y=arg0, x=arg1
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto safe_div = MCond(y == Expr<>::lit(0.0), Expr<>::lit(0.0), x / y);
    constexpr auto fn = full_compile<safe_div>();
    // fn(y, x) due to DFS ordering
    EXPECT_DOUBLE_EQ(fn(2.0, 10.0), 5.0);
    EXPECT_DOUBLE_EQ(fn(0.0, 10.0), 0.0);
}

TEST(ControlMacros, Clamp) {
    // clamp(x, lo, hi) = cond(x < lo, lo, cond(x > hi, hi, x))
    constexpr auto x = Expr<>::var("x");
    constexpr auto lo = Expr<>::var("lo");
    constexpr auto hi = Expr<>::var("hi");
    constexpr auto clamped = MCond(x < lo, lo, MCond(x > hi, hi, x));
    constexpr auto fn = full_compile<clamped>();
    EXPECT_DOUBLE_EQ(fn(5.0, 0.0, 10.0), 5.0);
    EXPECT_DOUBLE_EQ(fn(-1.0, 0.0, 10.0), 0.0);
    EXPECT_DOUBLE_EQ(fn(15.0, 0.0, 10.0), 10.0);
}

// --- Lambda / Apply / Let tests ---

TEST(LambdaApply, BasicLet) {
    // let tmp = x * x in tmp + tmp
    // Equivalent to: ((lambda tmp. tmp + tmp) (x * x))
    // At runtime: computes x*x, adds it to itself
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = let_("tmp", x * x, Expr<>::var("tmp") + Expr<>::var("tmp"));
    constexpr auto fn = full_compile<e>();
    static_assert(fn(3.0) == 18.0);
    EXPECT_DOUBLE_EQ(fn(3.0), 18.0);  // 9 + 9
    EXPECT_DOUBLE_EQ(fn(5.0), 50.0);  // 25 + 25
}

TEST(LambdaApply, LetWithConstant) {
    // let c = 42 in c + x
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = let_("c", Expr<>::lit(42.0), Expr<>::var("c") + x);
    constexpr auto fn = full_compile<e>();
    EXPECT_DOUBLE_EQ(fn(8.0), 50.0);
}

TEST(LambdaApply, NestedLet) {
    // let a = x + 1 in
    //   let b = a * 2 in
    //     b + a
    constexpr auto x = Expr<>::var("x");
    constexpr auto a = Expr<>::var("a");
    constexpr auto b = Expr<>::var("b");
    constexpr auto e = let_("a", x + 1.0,
                        let_("b", a * 2.0,
                            b + a));
    constexpr auto fn = full_compile<e>();
    // x=3: a=4, b=8, result=12
    EXPECT_DOUBLE_EQ(fn(3.0), 12.0);
    // x=0: a=1, b=2, result=3
    EXPECT_DOUBLE_EQ(fn(0.0), 3.0);
}

TEST(LambdaApply, LetNoFreeVars) {
    // let x = 10 in x + x (no free variables — 0-arg function)
    constexpr auto e = let_("x", Expr<>::lit(10.0),
                            Expr<>::var("x") + Expr<>::var("x"));
    constexpr auto fn = full_compile<e>();
    EXPECT_DOUBLE_EQ(fn(), 20.0);
}

TEST(LambdaApply, LetWithControlFlow) {
    // let threshold = 5 in cond(x > threshold, x, threshold)
    constexpr auto x = Expr<>::var("x");
    constexpr auto t = Expr<>::var("threshold");
    constexpr auto e = let_("threshold", Expr<>::lit(5.0),
                            MCond(x > t, x, t));
    constexpr auto fn = full_compile<e>();
    EXPECT_DOUBLE_EQ(fn(3.0), 5.0);   // below threshold
    EXPECT_DOUBLE_EQ(fn(7.0), 7.0);   // above threshold
    EXPECT_DOUBLE_EQ(fn(5.0), 5.0);   // equal
}

// --- Pretty-print for lambda/apply/let ---

TEST(ControlPrettyPrint, Let) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = let_("tmp", x * x, Expr<>::var("tmp") + Expr<>::var("tmp"));
    constexpr auto s = pretty_print(e);
    EXPECT_STREQ(s.data, "(let tmp (x * x) (tmp + tmp))");
}

TEST(ControlPrettyPrint, Lambda) {
    constexpr auto body = Expr<>::var("x") + Expr<>::lit(1.0);
    constexpr auto e = lambda("x", body);
    constexpr auto s = pretty_print(e);
    EXPECT_STREQ(s.data, "(lambda (x) (x + 1))");
}
