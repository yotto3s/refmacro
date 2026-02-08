// 03_math_pipeline.cpp — Symbolic differentiation and simplification
//
// Shows: expr() lambda binding, pipe operator, differentiate(),
//        simplify(), pretty_print(), second derivatives, gradients.

#include <iostream>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

int main() {
    constexpr auto diff_x = [](Expr<> e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };

    // --- First derivative of a polynomial ---
    // f(x) = x^3 (represented as x*x*x)
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = x * x * x;
    constexpr auto df = f | diff_x | simp;
    constexpr auto d2f = df | diff_x | simp;

    constexpr auto fn = math_compile<f>();
    constexpr auto dfn = math_compile<df>();
    constexpr auto d2fn = math_compile<d2f>();

    std::cout << "f(x)   = " << pretty_print(f).data << "\n";
    std::cout << "f'(x)  = " << pretty_print(df).data << "\n";
    std::cout << "f''(x) = " << pretty_print(d2f).data << "\n\n";

    for (double v : {-1.0, 0.0, 1.0, 2.0}) {
        std::cout << "f(" << v << ")=" << fn(v) << "  f'(" << v
                  << ")=" << dfn(v) << "  f''(" << v << ")=" << d2fn(v) << "\n";
    }

    // --- Multivariate: gradient of f(x,y) = x*y + x + y ---
    constexpr auto y = Expr<>::var("y");
    constexpr auto g = x * y + x + y;
    constexpr auto diff_y = [](Expr<> e) consteval {
        return differentiate(e, "y");
    };

    constexpr auto gx = g | diff_x | simp; // dg/dx = y + 1
    constexpr auto gy = g | diff_y | simp; // dg/dy = x + 1

    constexpr auto gx_fn = math_compile<gx>();
    constexpr auto gy_fn = math_compile<gy>();

    // Note: after differentiation + simplification, eliminated variables
    // change the function arity. dg/dx = y+1 takes only y.
    std::cout << "\ng(x,y) = " << pretty_print(g).data << "\n";
    std::cout << "dg/dx  = " << pretty_print(gx).data << "\n";
    std::cout << "dg/dy  = " << pretty_print(gy).data << "\n";
    std::cout << "grad g at (2,3) = (" << gx_fn(3.0) << ", " << gy_fn(2.0)
              << ")\n";
}
