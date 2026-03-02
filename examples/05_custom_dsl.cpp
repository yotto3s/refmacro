// 05_custom_dsl.cpp — A small custom DSL: conditional expressions
//
// Shows: Multiple defmacro() nodes, MacroCaller auto-tracking,
//        compile<>() with auto-discovered macros, building a mini-language.

#include <iostream>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

// "gt" node: gt(a, b) compiles to a > b ? 1.0 : 0.0
constexpr auto Gt = defmacro<"gt">([](auto lhs, auto rhs) {
    return
        [=](auto... a) constexpr { return lhs(a...) > rhs(a...) ? 1.0 : 0.0; };
});

// "if_" node: if_(cond, then, else)
// Treats cond != 0 as true.
constexpr auto If = defmacro<"if_">([](auto cond, auto then_br, auto else_br) {
    return [=](auto... a) constexpr {
        return cond(a...) != 0.0 ? then_br(a...) : else_br(a...);
    };
});

int main() {
    // relu(x) = If(Gt(x, 0), x, 0)
    constexpr auto x = Expr::var("x");
    constexpr auto relu_expr = If(Gt(x, Expr::lit(0.0)), x, Expr::lit(0.0));

    constexpr auto relu = compile<relu_expr>();
    static_assert(relu(-5.0) == 0.0);
    static_assert(relu(0.0) == 0.0);
    static_assert(relu(3.0) == 3.0);

    std::cout << "relu(x) = " << pretty_print(relu_expr).data << "\n";
    for (double v : {-3.0, -1.0, 0.0, 1.0, 3.0}) {
        std::cout << "relu(" << v << ") = " << relu(v) << "\n";
    }

    // step(x) = If(Gt(x*x, 1), 1, 0)  -- fires when |x| > 1
    constexpr auto step_expr =
        If(Gt(x * x, Expr::lit(1.0)), Expr::lit(1.0), Expr::lit(0.0));
    constexpr auto step = compile<step_expr>();

    std::cout << "\nstep(x) = 1 if |x|>1, else 0\n";
    for (double v : {-2.0, -0.5, 0.0, 0.5, 2.0}) {
        std::cout << "step(" << v << ") = " << step(v) << "\n";
    }
}
