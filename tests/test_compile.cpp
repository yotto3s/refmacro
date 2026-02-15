#include <gtest/gtest.h>
#include <refmacro/compile.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/macro.hpp>

using namespace refmacro;

// --- Define macros for testing ---

// Unary: negation
constexpr auto Neg = defmacro<"neg">(
    [](auto x) { return [=](auto... args) constexpr { return -x(args...); }; });

// Binary: addition
constexpr auto Add = defmacro<"add">([](auto lhs, auto rhs) {
    return [=](auto... args) constexpr { return lhs(args...) + rhs(args...); };
});

// Binary: multiplication
constexpr auto Mul = defmacro<"mul">([](auto lhs, auto rhs) {
    return [=](auto... args) constexpr { return lhs(args...) * rhs(args...); };
});

// --- Test defmacro as AST builder ---

TEST(Defmacro, BuildsASTNode) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = Neg(x);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
    static_assert(e.ast.nodes[e.id].child_count == 1);
}

TEST(Defmacro, BuildsBinaryNode) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = Add(x, y);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
    static_assert(e.ast.nodes[e.id].child_count == 2);
}

TEST(Defmacro, NestedConstruction) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = Neg(Add(x, Expr::lit(1.0)));
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
}

// --- Test compile ---

TEST(Compile, LitBuiltIn) {
    constexpr auto e = Expr::lit(42.0);
    constexpr auto fn = compile<e>();
    static_assert(fn() == 42.0);
}

TEST(Compile, SingleVar) {
    constexpr auto e = Expr::var("x");
    constexpr auto fn = compile<e>();
    static_assert(fn(5.0) == 5.0);
}

TEST(Compile, TwoVars) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    // Build with macro: Add(x, y)
    constexpr auto e = Add(x, y);
    constexpr auto fn = compile<e, Add>();
    static_assert(fn(3.0, 4.0) == 7.0);
}

TEST(Compile, UnaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = Neg(x);
    constexpr auto fn = compile<e, Neg>();
    static_assert(fn(5.0) == -5.0);
}

TEST(Compile, NestedMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    // neg(add(x, y))
    constexpr auto e = Neg(Add(x, y));
    constexpr auto fn = compile<e, Neg, Add>();
    static_assert(fn(3.0, 4.0) == -7.0);
}

TEST(Compile, ComplexExpression) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    // x * x + 2 * y
    constexpr auto e = Add(Mul(x, x), Mul(Expr::lit(2.0), y));
    constexpr auto fn = compile<e, Add, Mul>();
    static_assert(fn(3.0, 4.0) == 17.0); // 9 + 8
}

TEST(Compile, RuntimeCall) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = Add(Mul(x, x), y);
    constexpr auto fn = compile<e, Add, Mul>();
    EXPECT_DOUBLE_EQ(fn(3.0, 1.0), 10.0);
    EXPECT_DOUBLE_EQ(fn(0.0, 5.0), 5.0);
}

// --- Test that macros are truly generic (custom DSL node) ---

constexpr auto If =
    defmacro<"if_">([](auto cond, auto then_val, auto else_val) {
        return [=](auto... args) constexpr {
            return cond(args...) ? then_val(args...) : else_val(args...);
        };
    });

constexpr auto Gt = defmacro<"gt">([](auto lhs, auto rhs) {
    return [=](auto... args) constexpr { return lhs(args...) > rhs(args...); };
});

TEST(Compile, CustomDSLNode) {
    constexpr auto x = Expr::var("x");
    // if_(x > 0, x, neg(x))  -- absolute value
    constexpr auto e = If(Gt(x, Expr::lit(0.0)), x, Neg(x));
    constexpr auto fn = compile<e, If, Gt, Neg>();
    static_assert(fn(5.0) == 5.0);
    static_assert(fn(-3.0) == 3.0);
}

// --- Test MacroCaller fields ---

TEST(Defmacro, HasTagAndFnFields) {
    static_assert(str_eq(Add.tag, "add"));
    static_assert(str_eq(Neg.tag, "neg"));
}

TEST(Defmacro, BuildsTernaryNode) {
    constexpr auto c = Expr::var("c");
    constexpr auto t = Expr::lit(1.0);
    constexpr auto f = Expr::lit(2.0);
    constexpr auto e = If(c, t, f);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "if_"));
    static_assert(e.ast.nodes[e.id].child_count == 3);
}

// --- Test auto-compile (macros discovered from Expression type) ---

TEST(AutoCompile, UnaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = Neg(x);
    constexpr auto fn = compile<e>(); // no macro list!
    static_assert(fn(5.0) == -5.0);
}

TEST(AutoCompile, BinaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = Add(x, y);
    constexpr auto fn = compile<e>();
    static_assert(fn(3.0, 4.0) == 7.0);
}

TEST(AutoCompile, NestedMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = Neg(Add(x, y));
    constexpr auto fn = compile<e>();
    static_assert(fn(3.0, 4.0) == -7.0);
}

TEST(AutoCompile, TernaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = If(x, Expr::lit(1.0), Expr::lit(0.0));
    constexpr auto fn = compile<e>();
    static_assert(fn(1.0) == 1.0);
    static_assert(fn(0.0) == 0.0);
}

TEST(AutoCompile, BackwardCompatExplicitMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = make_node("neg", x); // raw node, no macro tracking
    constexpr auto fn = compile<e, Neg>();  // must pass explicitly
    static_assert(fn(5.0) == -5.0);
}

TEST(AutoCompile, CustomDSLAutoDiscover) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = If(Gt(x, Expr::lit(0.0)), x, Neg(x));
    constexpr auto fn = compile<e>(); // auto-discovers If, Gt, Neg
    static_assert(fn(5.0) == 5.0);
    static_assert(fn(-3.0) == 3.0);
}
