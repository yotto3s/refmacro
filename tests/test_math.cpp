#include <gtest/gtest.h>
#include <refmacro/math.hpp>

using namespace refmacro;

// --- Test operator sugar ---

TEST(MathOps, Add) {
    constexpr auto e = Expr::var("x") + Expr::lit(1.0);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
}

TEST(MathOps, Mul) {
    constexpr auto e = Expr::var("x") * Expr::var("y");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "mul"));
}

TEST(MathOps, Sub) {
    constexpr auto e = Expr::var("x") - Expr::lit(1.0);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "sub"));
}

TEST(MathOps, Div) {
    constexpr auto e = Expr::var("x") / Expr::lit(2.0);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "div"));
}

TEST(MathOps, UnaryNeg) {
    constexpr auto e = -Expr::var("x");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
}

TEST(MathOps, DoubleLhsAdd) {
    constexpr auto e = 2.0 + Expr::var("x");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
    static_assert(e.ast.nodes[e.ast.nodes[e.id].children[0]].payload == 2.0);
}

TEST(MathOps, DoubleLhsMul) {
    constexpr auto e = 3.0 * Expr::var("x");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "mul"));
}

TEST(MathOps, DoubleRhsAdd) {
    constexpr auto e = Expr::var("x") + 5.0;
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
}

// --- Test math_compile (convenience wrapper) ---

TEST(MathCompile, Linear) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = 3.0 * x + 5.0;
    constexpr auto fn = math_compile<e>();
    static_assert(fn(2.0) == 11.0);
}

TEST(MathCompile, Polynomial) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = x * x + 2.0 * y;
    constexpr auto fn = math_compile<e>();
    static_assert(fn(3.0, 4.0) == 17.0);
}

TEST(MathCompile, AllOps) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = -(x * x - x / 2.0);
    constexpr auto fn = math_compile<e>();
    // x=4: -(16 - 2) = -14
    static_assert(fn(4.0) == -14.0);
}

TEST(MathCompile, Runtime) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = x * x + 1.0;
    constexpr auto fn = math_compile<e>();
    EXPECT_DOUBLE_EQ(fn(3.0), 10.0);
    EXPECT_DOUBLE_EQ(fn(0.0), 1.0);
}

// --- Pretty Printer tests ---

#include <refmacro/pretty_print.hpp>

TEST(PrettyPrint, Lit) {
    constexpr auto e = Expr::lit(42.0);
    constexpr auto s = pretty_print(e);
    static_assert(s == "42");
}

TEST(PrettyPrint, Var) {
    constexpr auto e = Expr::var("x");
    constexpr auto s = pretty_print(e);
    static_assert(s == "x");
}

TEST(PrettyPrint, BinaryInfix) {
    constexpr auto e = Expr::var("x") + Expr::lit(1.0);
    constexpr auto s = pretty_print(e);
    static_assert(s == "(x + 1)");
}

TEST(PrettyPrint, Nested) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = x * x + 2.0 * y;
    constexpr auto s = pretty_print(e);
    static_assert(s == "((x * x) + (2 * y))");
}

TEST(PrettyPrint, Neg) {
    constexpr auto e = -Expr::var("x");
    constexpr auto s = pretty_print(e);
    static_assert(s == "(-x)");
}

TEST(PrettyPrint, GenericTag) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = make_node("custom", x);
    constexpr auto s = pretty_print(e);
    static_assert(s == "custom(x)");
}

#include <refmacro/transforms.hpp>

consteval auto root_tag(const Expr& e) -> std::string_view {
    return e.ast.nodes[e.id].tag;
}
consteval double root_payload(const Expr& e) {
    return e.ast.nodes[e.id].payload;
}
consteval auto root_name(const Expr& e) -> std::string_view {
    return e.ast.nodes[e.id].name;
}

// --- simplify ---
TEST(Simplify, AddZero) {
    static_assert(root_tag(simplify(Expr::var("x") + 0.0)) == "var");
}
TEST(Simplify, ZeroAdd) {
    static_assert(root_tag(simplify(0.0 + Expr::var("x"))) == "var");
}
TEST(Simplify, MulOne) {
    static_assert(root_tag(simplify(Expr::var("x") * 1.0)) == "var");
}
TEST(Simplify, OneMul) {
    static_assert(root_tag(simplify(1.0 * Expr::var("x"))) == "var");
}
TEST(Simplify, MulZero) {
    static_assert(root_payload(simplify(0.0 * Expr::var("x"))) == 0.0);
}
TEST(Simplify, SubZero) {
    static_assert(root_tag(simplify(Expr::var("x") - 0.0)) == "var");
}
TEST(Simplify, DivOne) {
    static_assert(root_tag(simplify(Expr::var("x") / 1.0)) == "var");
}
TEST(Simplify, DoubleNeg) {
    static_assert(root_tag(simplify(-(-Expr::var("x")))) == "var");
}
TEST(Simplify, ConstFold) {
    static_assert(root_payload(simplify(Expr::lit(3.0) + 4.0)) == 7.0);
}
TEST(Simplify, Nested) {
    static_assert(root_tag(simplify((Expr::var("x") + 0.0) * 1.0)) == "var");
}

// --- differentiate ---
TEST(Diff, Lit) {
    static_assert(root_payload(differentiate(Expr::lit(5.0), "x")) == 0.0);
}
TEST(Diff, VarSelf) {
    static_assert(root_payload(differentiate(Expr::var("x"), "x")) == 1.0);
}
TEST(Diff, VarOther) {
    static_assert(root_payload(differentiate(Expr::var("y"), "x")) == 0.0);
}
TEST(Diff, SumSimplified) {
    static_assert(root_payload(simplify(differentiate(
                      Expr::var("x") + Expr::var("y"), "x"))) == 1.0);
}
TEST(Diff, ConstTimesVar) {
    static_assert(root_payload(simplify(
                      differentiate(2.0 * Expr::var("x"), "x"))) == 2.0);
}
TEST(Diff, NegVar) {
    static_assert(root_payload(simplify(differentiate(-Expr::var("x"), "x"))) ==
                  -1.0);
}

// --- Full pipeline: build -> differentiate -> simplify -> compile ---
TEST(Pipeline, Quadratic) {
    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x + 2.0 * x + 1.0;
    constexpr auto diff_x = [](Expr e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](Expr e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto fn = math_compile<df>();
    static_assert(fn(3.0) == 8.0); // 2*3 + 2
    static_assert(fn(0.0) == 2.0);
}

TEST(Pipeline, SecondDerivative) {
    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x * x;
    constexpr auto diff_x = [](Expr e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](Expr e) consteval { return simplify(e); };
    constexpr auto d2f = f | diff_x | simp | diff_x | simp;
    constexpr auto fn = math_compile<d2f>();
    static_assert(fn(2.0) == 12.0); // 6*2
}

// --- expr() binding tests ---

TEST(ExprBinding, SingleVar) {
    constexpr auto e = expr([](auto x) { return x * x; }, "x");
    constexpr auto fn = math_compile<e>();
    static_assert(fn(3.0) == 9.0);
}

TEST(ExprBinding, TwoVars) {
    constexpr auto e =
        expr([](auto x, auto y) { return x * x + 2.0 * y; }, "x", "y");
    constexpr auto fn = math_compile<e>();
    static_assert(fn(3.0, 4.0) == 17.0);
}

TEST(ExprBinding, Pipeline) {
    constexpr auto f = expr([](auto x) { return x * x + 2.0 * x + 1.0; }, "x");
    constexpr auto diff_x = [](Expr e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](Expr e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto fn = math_compile<df>();
    static_assert(fn(3.0) == 8.0);
}

TEST(PrettyPrint, PrintBufferRename) {
    constexpr auto e = Expr::lit(1.0);
    constexpr refmacro::PrintBuffer<256> s = pretty_print(e);
    static_assert(s == "1");
}
