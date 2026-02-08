#include <gtest/gtest.h>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

// --- Custom macros (file scope for NTTP use) ---

constexpr auto Abs = defmacro("abs", [](auto x) {
    return [=](auto... a) constexpr {
        auto v = x(a...);
        return v < 0 ? -v : v;
    };
});

constexpr auto Clamp = defmacro("clamp", [](auto val, auto lo, auto hi) {
    return [=](auto... a) constexpr {
        auto v = val(a...);
        auto l = lo(a...);
        auto h = hi(a...);
        return v < l ? l : (v > h ? h : v);
    };
});

// --- End-to-end with defmacro ---

TEST(Integration, CustomAbsViaMacro) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = Abs(x);
    constexpr auto fn = compile<e, Abs>();
    static_assert(fn(5.0) == 5.0);
    static_assert(fn(-3.0) == 3.0);
}

// --- End-to-end math pipeline ---

TEST(Integration, LinearFunction) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = 3.0 * x + 5.0;
    constexpr auto fn = math_compile<f>();
    static_assert(fn(2.0) == 11.0);
    static_assert(fn(0.0) == 5.0);

    constexpr auto diff_x = [](Expr<> e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto dfn = math_compile<df>();
    static_assert(dfn(100.0) == 3.0);
}

TEST(Integration, QuadraticRoots) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = x * x - 4.0 * x + 4.0;
    constexpr auto fn = math_compile<f>();
    static_assert(fn(2.0) == 0.0);

    constexpr auto diff_x = [](Expr<> e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto dfn = math_compile<df>();
    static_assert(dfn(2.0) == 0.0); // minimum
}

TEST(Integration, MultivarGradient) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto f = x * y + x + y;
    constexpr auto fn = math_compile<f>();
    static_assert(fn(2.0, 3.0) == 11.0);

    constexpr auto dfx = simplify(differentiate(f, "x"));
    constexpr auto dfx_fn = math_compile<dfx>();
    static_assert(dfx_fn(3.0) ==
                  4.0); // y + 1, only y remains after simplification

    constexpr auto dfy = simplify(differentiate(f, "y"));
    constexpr auto dfy_fn = math_compile<dfy>();
    static_assert(dfy_fn(2.0) ==
                  3.0); // x + 1, only x remains after simplification
}

TEST(Integration, SecondDerivative) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = x * x * x;
    constexpr auto diff_x = [](Expr<> e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto d2f = f | diff_x | simp | diff_x | simp;
    constexpr auto fn = math_compile<d2f>();
    static_assert(fn(2.0) == 12.0); // 6*2
}

// --- Mixing custom macros with math ---

TEST(Integration, CustomMacroWithMath) {
    constexpr auto x = Expr<>::var("x");
    // clamp(x * x - 10, 0, 100)
    constexpr auto e =
        Clamp(x * x - 10.0, Expr<>::lit(0.0), Expr<>::lit(100.0));
    constexpr auto fn = compile<e, MAdd, MSub, MMul, MDiv, MNeg, Clamp>();
    static_assert(fn(1.0) == 0.0);    // 1-10 = -9, clamped to 0
    static_assert(fn(4.0) == 6.0);    // 16-10 = 6
    static_assert(fn(20.0) == 100.0); // 400-10 = 390, clamped to 100
}

TEST(Integration, PrettyPrint) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = x * x + 1.0;
    constexpr auto s = pretty_print(e);
    static_assert(s == "((x * x) + 1)");
}

TEST(Integration, RuntimeCalls) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto f = x * x + y * y;
    constexpr auto fn = math_compile<f>();
    EXPECT_DOUBLE_EQ(fn(3.0, 4.0), 25.0);
    EXPECT_DOUBLE_EQ(fn(0.0, 0.0), 0.0);
}
