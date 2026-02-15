// 02_custom_macro.cpp — Defining custom AST operations with defmacro
//
// Shows: defmacro(), custom node tags, compile<> with custom macros,
//        mixing custom and math macros.

#include <iostream>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

// A unary "abs" macro: abs(x) = x < 0 ? -x : x
constexpr auto Abs = defmacro<"abs">([](auto x) {
    return [=](auto... a) constexpr {
        auto v = x(a...);
        return v < 0 ? -v : v;
    };
});

// A ternary "clamp" macro: clamp(val, lo, hi)
constexpr auto Clamp = defmacro<"clamp">([](auto val, auto lo, auto hi) {
    return [=](auto... a) constexpr {
        auto v = val(a...);
        auto l = lo(a...);
        auto h = hi(a...);
        return v < l ? l : (v > h ? h : v);
    };
});

int main() {
    constexpr auto x = Expr::var("x");

    // --- Use Abs alone ---
    constexpr auto e1 = Abs(x);
    constexpr auto fn1 = compile<e1, Abs>();
    static_assert(fn1(5.0) == 5.0);
    static_assert(fn1(-3.0) == 3.0);

    // --- Mix custom macros with math ---
    // clamp(x^2 - 10, 0, 100)
    constexpr auto e2 = Clamp(x * x - 10.0, Expr::lit(0.0), Expr::lit(100.0));
    constexpr auto fn2 = compile<e2, MAdd, MSub, MMul, MDiv, MNeg, Clamp>();

    static_assert(fn2(1.0) == 0.0);    // 1-10 = -9 → clamped to 0
    static_assert(fn2(4.0) == 6.0);    // 16-10 = 6
    static_assert(fn2(20.0) == 100.0); // 400-10 = 390 → clamped to 100

    std::cout << "abs(-7) = " << fn1(-7.0) << "\n";
    std::cout << "clamp(1^2-10, 0, 100) = " << fn2(1.0) << "\n";
    std::cout << "clamp(4^2-10, 0, 100) = " << fn2(4.0) << "\n";
    std::cout << "clamp(20^2-10, 0, 100) = " << fn2(20.0) << "\n";
}
