// 06_control_flow.cpp — Control-flow macros: conditionals, comparisons, logic
//
// Shows: MCond, comparison operators (==, <, >), logical operators (&&, ||, !),
//        full_compile<> with math + control macros, pretty_print().

#include <iostream>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

int main() {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");

    // --- abs(x) via conditional ---
    constexpr auto abs_expr = MCond(x < Expr<>::lit(0.0), -x, x);
    constexpr auto abs_fn = full_compile<abs_expr>();
    static_assert(abs_fn(-3.0) == 3.0);
    static_assert(abs_fn(5.0) == 5.0);

    std::cout << "abs(x) = " << pretty_print(abs_expr).data << "\n";
    for (double v : {-5.0, -1.0, 0.0, 1.0, 5.0})
        std::cout << "  abs(" << v << ") = " << abs_fn(v) << "\n";

    // --- clamp(x, lo=0, hi=10) ---
    constexpr auto lo = Expr<>::var("lo");
    constexpr auto hi = Expr<>::var("hi");
    constexpr auto clamp_expr = MCond(x < lo, lo, MCond(x > hi, hi, x));
    constexpr auto clamp_fn = full_compile<clamp_expr>();

    std::cout << "\nclamp(x, lo, hi) = " << pretty_print(clamp_expr).data
              << "\n";
    for (double v : {-2.0, 0.0, 5.0, 10.0, 15.0})
        std::cout << "  clamp(" << v << ", 0, 10) = " << clamp_fn(v, 0.0, 10.0)
                  << "\n";

    // --- safe division: avoid divide-by-zero ---
    // DFS visits y first (in y == 0), so arg order is (y, x)
    constexpr auto safe_div_expr =
        MCond(y == Expr<>::lit(0.0), Expr<>::lit(0.0), x / y);
    constexpr auto safe_div = full_compile<safe_div_expr>();

    std::cout << "\nsafe_div(x, y) = " << pretty_print(safe_div_expr).data
              << "\n";
    std::cout << "  safe_div(10, 3) = " << safe_div(3.0, 10.0) << "\n";
    std::cout << "  safe_div(10, 0) = " << safe_div(0.0, 10.0) << "\n";

    // --- logical: is x in range [lo, hi]? ---
    constexpr auto in_range_expr = (x >= lo) && (x <= hi);
    constexpr auto in_range = full_compile<in_range_expr>();

    std::cout << "\nin_range(x, lo, hi) = " << pretty_print(in_range_expr).data
              << "\n";
    for (double v : {-1.0, 0.0, 5.0, 10.0, 11.0})
        std::cout << "  in_range(" << v
                  << ", 0, 10) = " << in_range(v, 0.0, 10.0) << "\n";
}
