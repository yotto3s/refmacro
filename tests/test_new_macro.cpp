#include <gtest/gtest.h>
#include <refmacro/compile.hpp>
#include <refmacro/macro.hpp>

using namespace refmacro;

// New-style defmacro
constexpr auto MyNeg = defmacro<"neg">(
    [](auto x) { return [=](auto... a) constexpr { return -x(a...); }; });

constexpr auto MyAdd = defmacro<"add">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) + rhs(a...); };
});

constexpr auto MyCond = defmacro<"cond">([](auto test, auto then_, auto else_) {
    return [=](auto... a) constexpr {
        return test(a...) ? then_(a...) : else_(a...);
    };
});

TEST(NewDefmacro, BuildsUnaryNode) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = MyNeg(x);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
    static_assert(e.ast.nodes[e.id].child_count == 1);
}

TEST(NewDefmacro, BuildsBinaryNode) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = MyAdd(x, y);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
    static_assert(e.ast.nodes[e.id].child_count == 2);
}

TEST(NewDefmacro, BuildsTernaryNode) {
    constexpr auto c = Expr::var("c");
    constexpr auto t = Expr::lit(1.0);
    constexpr auto f = Expr::lit(2.0);
    constexpr auto e = MyCond(c, t, f);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "cond"));
    static_assert(e.ast.nodes[e.id].child_count == 3);
}

TEST(NewDefmacro, NestedConstruction) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = MyNeg(MyAdd(x, Expr::lit(1.0)));
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
}

TEST(NewDefmacro, HasTagAndFnFields) {
    static_assert(str_eq(MyAdd.tag, "add"));
    static_assert(str_eq(MyNeg.tag, "neg"));
}

TEST(NewDefmacro, OldStyleStillWorks) {
    constexpr auto OldAdd = defmacro("add", [](auto lhs, auto rhs) {
        return [=](auto... a) constexpr { return lhs(a...) + rhs(a...); };
    });
    constexpr auto x = Expr::var("x");
    constexpr auto e = OldAdd(x, Expr::lit(1.0));
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
}

TEST(AutoCompile, UnaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = MyNeg(x);
    constexpr auto fn = compile<e>(); // no macro list!
    static_assert(fn(5.0) == -5.0);
}

TEST(AutoCompile, BinaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = MyAdd(x, y);
    constexpr auto fn = compile<e>();
    static_assert(fn(3.0, 4.0) == 7.0);
}

TEST(AutoCompile, NestedMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = MyNeg(MyAdd(x, y));
    constexpr auto fn = compile<e>();
    static_assert(fn(3.0, 4.0) == -7.0);
}

TEST(AutoCompile, TernaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = MyCond(x, Expr::lit(1.0), Expr::lit(0.0));
    constexpr auto fn = compile<e>();
    static_assert(fn(1.0) == 1.0);
    static_assert(fn(0.0) == 0.0);
}

TEST(AutoCompile, BackwardCompatExplicitMacros) {
    // Old style: pass macros explicitly — still works
    constexpr auto x = Expr::var("x");
    constexpr auto e = make_node("neg", x);  // raw node, no macro tracking
    constexpr auto fn = compile<e, MyNeg>(); // must pass explicitly
    static_assert(fn(5.0) == -5.0);
}
