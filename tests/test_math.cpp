#include <refmacro/math.hpp>
#include <gtest/gtest.h>

using namespace refmacro;

// --- Test operator sugar ---

TEST(MathOps, Add) {
    constexpr auto e = Expr<>::var("x") + Expr<>::lit(1.0);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
}

TEST(MathOps, Mul) {
    constexpr auto e = Expr<>::var("x") * Expr<>::var("y");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "mul"));
}

TEST(MathOps, Sub) {
    constexpr auto e = Expr<>::var("x") - Expr<>::lit(1.0);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "sub"));
}

TEST(MathOps, Div) {
    constexpr auto e = Expr<>::var("x") / Expr<>::lit(2.0);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "div"));
}

TEST(MathOps, UnaryNeg) {
    constexpr auto e = -Expr<>::var("x");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
}

TEST(MathOps, DoubleLhsAdd) {
    constexpr auto e = 2.0 + Expr<>::var("x");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
    static_assert(e.ast.nodes[e.ast.nodes[e.id].children[0]].payload == 2.0);
}

TEST(MathOps, DoubleLhsMul) {
    constexpr auto e = 3.0 * Expr<>::var("x");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "mul"));
}

TEST(MathOps, DoubleRhsAdd) {
    constexpr auto e = Expr<>::var("x") + 5.0;
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
}

// --- Test math_compile (convenience wrapper) ---

TEST(MathCompile, Linear) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = 3.0 * x + 5.0;
    constexpr auto fn = math_compile<e>();
    static_assert(fn(2.0) == 11.0);
}

TEST(MathCompile, Polynomial) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = x * x + 2.0 * y;
    constexpr auto fn = math_compile<e>();
    static_assert(fn(3.0, 4.0) == 17.0);
}

TEST(MathCompile, AllOps) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = -(x * x - x / 2.0);
    constexpr auto fn = math_compile<e>();
    // x=4: -(16 - 2) = -14
    static_assert(fn(4.0) == -14.0);
}

TEST(MathCompile, Runtime) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = x * x + 1.0;
    constexpr auto fn = math_compile<e>();
    EXPECT_DOUBLE_EQ(fn(3.0), 10.0);
    EXPECT_DOUBLE_EQ(fn(0.0), 1.0);
}
