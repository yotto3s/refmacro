// 07_let_bindings.cpp — Lambda, apply, and let-bindings
//
// Shows: lambda(), apply(), let_(), nested let, combining let with
//        control-flow macros, pretty_print() for let expressions.

#include <iostream>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

int main() {
    constexpr auto x = Expr<>::var("x");

    // --- Raw lambda and apply ---
    // lambda("n", body) creates a first-class function node
    // apply(fn, arg) applies the function to an argument
    // At compile time: apply(lambda(n, body), val) binds n = val in body

    // double_it = (lambda n. n + n)  applied to  x * 3
    constexpr auto double_it =
        apply(lambda("n", Expr<>::var("n") + Expr<>::var("n")), x * 3.0);
    constexpr auto double_fn = full_compile<double_it>();
    static_assert(double_fn(2.0) == 12.0); // n = 2*3 = 6, result = 6+6 = 12

    std::cout << "apply(lambda n. n+n, x*3)\n";
    std::cout << "  AST: " << pretty_print(double_it).data << "\n";
    for (double v : {1.0, 2.0, 3.0, 4.0})
        std::cout << "  f(" << v << ") = " << double_fn(v) << "\n";

    // Standalone lambda pretty-prints as (lambda (param) body)
    constexpr auto inc = lambda("k", Expr<>::var("k") + 1.0);
    std::cout << "\nstandalone lambda:\n";
    std::cout << "  " << pretty_print(inc).data << "\n";

    // --- Basic let: compute x*x once, use twice ---
    constexpr auto square_sum_expr =
        let_("tmp", x * x, Expr<>::var("tmp") + Expr<>::var("tmp"));
    constexpr auto square_sum = full_compile<square_sum_expr>();
    static_assert(square_sum(3.0) == 18.0);

    std::cout << "let tmp = x*x in tmp+tmp\n";
    std::cout << "  AST: " << pretty_print(square_sum_expr).data << "\n";
    for (double v : {1.0, 2.0, 3.0, 4.0, 5.0})
        std::cout << "  f(" << v << ") = " << square_sum(v) << "\n";

    // --- Nested let: quadratic formula pieces ---
    // let a = x*x in let b = a + a in b + 1
    constexpr auto nested_expr = let_(
        "a", x * x,
        let_("b", Expr<>::var("a") + Expr<>::var("a"), Expr<>::var("b") + 1.0));
    constexpr auto nested_fn = full_compile<nested_expr>();

    std::cout << "\nlet a = x*x in let b = a+a in b+1\n";
    std::cout << "  AST: " << pretty_print(nested_expr).data << "\n";
    for (double v : {0.0, 1.0, 2.0, 3.0})
        std::cout << "  f(" << v << ") = " << nested_fn(v) << "\n";

    // --- Let with control flow: smooth step ---
    // let t = clamp((x - edge0) / (edge1 - edge0), 0, 1)
    // in t * t * (3 - 2*t)
    constexpr auto edge0 = Expr<>::var("edge0");
    constexpr auto edge1 = Expr<>::var("edge1");
    constexpr auto t = Expr<>::var("t");

    constexpr auto raw = (x - edge0) / (edge1 - edge0);
    constexpr auto clamped = MCond(raw < 0.0, Expr<>::lit(0.0),
                                   MCond(raw > 1.0, Expr<>::lit(1.0), raw));
    constexpr auto smoothstep_expr =
        let_("t", clamped, t * t * (3.0 - 2.0 * t));
    constexpr auto smoothstep = full_compile<smoothstep_expr>();

    std::cout << "\nsmoothstep(x, edge0, edge1)\n";
    std::cout << "  AST: " << pretty_print(smoothstep_expr).data << "\n";
    // DFS order: x, edge0, edge1
    for (double v : {0.0, 0.25, 0.5, 0.75, 1.0, 1.5})
        std::cout << "  smoothstep(" << v
                  << ", 0, 1) = " << smoothstep(v, 0.0, 1.0) << "\n";

    // --- Constant let: no free variables ---
    constexpr auto const_expr =
        let_("pi", Expr<>::lit(3.14159), Expr<>::var("pi") * Expr<>::var("pi"));
    constexpr auto pi_squared = full_compile<const_expr>();

    std::cout << "\nlet pi = 3.14159 in pi*pi\n";
    std::cout << "  AST: " << pretty_print(const_expr).data << "\n";
    std::cout << "  result = " << pi_squared() << "\n";
}
