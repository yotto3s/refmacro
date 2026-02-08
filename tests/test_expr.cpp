#include <gtest/gtest.h>
#include <refmacro/expr.hpp>

using namespace refmacro;

TEST(Expr, Lit) {
    constexpr auto e = Expr::lit(42.0);
    static_assert(e.ast.count == 1);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "lit"));
    static_assert(e.ast.nodes[e.id].payload == 42.0);
}

TEST(Expr, Var) {
    constexpr auto e = Expr::var("x");
    static_assert(e.ast.count == 1);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "var"));
    static_assert(str_eq(e.ast.nodes[e.id].name, "x"));
}

// make_node: generic node construction (the core of defmacro)
TEST(Expr, MakeNodeUnary) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = make_node("neg", x);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
    static_assert(e.ast.nodes[e.id].child_count == 1);
}

TEST(Expr, MakeNodeBinary) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = make_node("add", x, y);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
    static_assert(e.ast.nodes[e.id].child_count == 2);
    // LHS is var("x"), RHS is var("y")
    static_assert(
        str_eq(e.ast.nodes[e.ast.nodes[e.id].children[0]].tag, "var"));
    static_assert(
        str_eq(e.ast.nodes[e.ast.nodes[e.id].children[1]].tag, "var"));
}

TEST(Expr, MakeNodeTernary) {
    constexpr auto c = Expr::var("c");
    constexpr auto t = Expr::var("t");
    constexpr auto f = Expr::var("f");
    constexpr auto e = make_node("if_", c, t, f);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "if_"));
    static_assert(e.ast.nodes[e.id].child_count == 3);
}

TEST(Expr, MakeNodeLeaf) {
    constexpr auto e = make_node<>("custom_leaf");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "custom_leaf"));
    static_assert(e.ast.nodes[e.id].child_count == 0);
}

TEST(Expr, MakeNodeQuaternary) {
    constexpr auto a = Expr::var("a");
    constexpr auto b = Expr::var("b");
    constexpr auto c = Expr::var("c");
    constexpr auto d = Expr::var("d");
    constexpr auto e = make_node("quad", a, b, c, d);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "quad"));
    static_assert(e.ast.nodes[e.id].child_count == 4);
}

TEST(Expr, NestedMakeNode) {
    constexpr auto x = Expr::var("x");
    constexpr auto one = Expr::lit(1.0);
    constexpr auto inner = make_node("add", x, one);
    constexpr auto outer = make_node("neg", inner);
    static_assert(str_eq(outer.ast.nodes[outer.id].tag, "neg"));
    static_assert(outer.ast.nodes[outer.id].child_count == 1);
    // Child is the add node
    constexpr int child_id = outer.ast.nodes[outer.id].children[0];
    static_assert(str_eq(outer.ast.nodes[child_id].tag, "add"));
}
