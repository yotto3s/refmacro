#include <gtest/gtest.h>
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
