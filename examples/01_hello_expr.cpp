// 01_hello_expr.cpp — Building and compiling your first expression
//
// Shows: Expr::lit(), Expr::var(), operator sugar, pretty_print(),
//        math_compile(), static_assert, runtime output.

#include <iostream>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

int main() {
    // --- Build an expression: f(x) = x^2 + 2x + 1 ---
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = x * x + 2.0 * x + 1.0;

    // Pretty-print the AST at compile time
    constexpr auto text = pretty_print(f);
    static_assert(text == "(((x * x) + (2 * x)) + 1)");

    // Compile the AST into a callable lambda
    constexpr auto fn = math_compile<f>();

    // Verify at compile time
    static_assert(fn(0.0) == 1.0);  // (0+1)^2 = 1
    static_assert(fn(3.0) == 16.0); // (3+1)^2 = 16

    // Use at runtime
    std::cout << "f(x) = " << text.data << "\n";
    for (double v : {-2.0, -1.0, 0.0, 1.0, 2.0, 3.0}) {
        std::cout << "f(" << v << ") = " << fn(v) << "\n";
    }
}
