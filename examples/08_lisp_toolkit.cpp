// 08_lisp_toolkit.cpp — "The Lisp Hacker's Toolkit"
//
// Progressive showcase of refmacro's code-is-data philosophy.
// Shows: pretty_print, pipe operator, differentiate, simplify,
//        custom rewrite rules, multi-variable gradients.
//
// Run this and watch compile-time AST metaprogramming in action.

#include <iostream>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

int main() {
    // ── Act 1: Code is Data ──────────────────────────────────
    //
    // In Lisp, programs are data structures you can inspect and transform.
    // refmacro brings this to C++: expressions are compile-time ASTs.

    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x + x; // f(x) = x² + x

    std::cout << "=== Act 1: Code is Data ===\n\n";
    std::cout << "  An expression is just data — an AST you can print:\n";
    std::cout << "  f(x) = " << pretty_print(f).data << "\n\n";

    // The same data compiles into an efficient function:
    constexpr auto f_fn = math_compile<f>();
    std::cout << "  ...and compile into a function:\n";
    std::cout << "  f(3)  = " << f_fn(3.0) << "\n";
    std::cout << "  f(10) = " << f_fn(10.0) << "\n";

    // ── Act 2: Transform Pipeline ────────────────────────────
    //
    // Like Lisp's macro-expansion, we transform ASTs in stages.
    // The pipe operator chains transforms: expr | diff | simplify

    std::cout << "\n=== Act 2: Transform Pipeline ===\n\n";

    constexpr auto diff_x = [](Expr e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](Expr e) consteval { return simplify(e); };

    // Raw derivative — ugly, full of 1* and *0 terms:
    constexpr auto raw_df = f | diff_x;
    // Simplify cleans it up:
    constexpr auto df = raw_df | simp;
    // Chain further — second derivative:
    constexpr auto d2f = df | diff_x | simp;

    std::cout << "  f(x)       = " << pretty_print(f).data << "\n";
    std::cout << "  f'(x) raw  = " << pretty_print(raw_df).data << "\n";
    std::cout << "  f'(x)      = " << pretty_print(df).data << "\n";
    std::cout << "  f''(x)     = " << pretty_print(d2f).data << "\n\n";

    constexpr auto df_fn = math_compile<df>();
    constexpr auto d2f_fn = math_compile<d2f>();
    std::cout << "  f'(3)  = " << df_fn(3.0) << "  (expected 7)\n";
    std::cout << "  f''(x) = " << d2f_fn() << "  (expected 2)\n";

    // ── Act 3: Your Own Rewrite Rules ────────────────────────
    //
    // Like defmacro in Lisp, you can define your own AST transforms.
    // rewrite() applies rules bottom-up until fixed-point.
    // The system is extensible — add your own passes alongside built-ins.

    std::cout << "\n=== Act 3: Your Own Rewrite Rules ===\n\n";

    // Rule 1: Distribute multiplication over addition.
    //   a * (b + c) → a*b + a*c
    //   (b + c) * a → b*a + c*a
    constexpr auto expand =
        [](NodeView<64> n) consteval -> std::optional<Expr> {
        if (n.tag() != "mul" || n.child_count() != 2)
            return std::nullopt;
        // a * (b + c)
        if (n.child(1).tag() == "add" && n.child(1).child_count() == 2) {
            auto a = to_expr(n, n.child(0));
            auto b = to_expr(n, n.child(1).child(0));
            auto c = to_expr(n, n.child(1).child(1));
            return a * b + a * c;
        }
        // (b + c) * a
        if (n.child(0).tag() == "add" && n.child(0).child_count() == 2) {
            auto a = to_expr(n, n.child(1));
            auto b = to_expr(n, n.child(0).child(0));
            auto c = to_expr(n, n.child(0).child(1));
            return b * a + c * a;
        }
        return std::nullopt;
    };

    // Rule 2: Collect identical variables.
    //   x + x → 2 * x
    constexpr auto collect =
        [](NodeView<64> n) consteval -> std::optional<Expr> {
        if (n.tag() != "add" || n.child_count() != 2)
            return std::nullopt;
        if (n.child(0).tag() == "var" && n.child(1).tag() == "var" &&
            n.child(0).name() == n.child(1).name()) {
            return Expr::lit(2.0) * to_expr(n, n.child(0));
        }
        return std::nullopt;
    };

    // Demo: expand  x * (x + 1) → x*x + x*1 → (x * x) + x
    constexpr auto e2 = x * (x + 1.0);
    constexpr auto e2_expanded = rewrite(e2, expand);
    constexpr auto e2_clean = simplify(e2_expanded);

    std::cout << "  Rule: expand  a * (b + c) -> a*b + a*c\n";
    std::cout << "  before:   " << pretty_print(e2).data << "\n";
    std::cout << "  expanded: " << pretty_print(e2_expanded).data << "\n";
    std::cout << "  cleaned:  " << pretty_print(e2_clean).data << "\n\n";

    // Demo: collect applied to f'(x) = (x + x) + 1 → (2 * x) + 1
    constexpr auto df_collected = rewrite(df, collect);

    std::cout << "  Rule: collect  x + x -> 2 * x\n";
    std::cout << "  f'(x) before:  " << pretty_print(df).data << "\n";
    std::cout << "  f'(x) after:   " << pretty_print(df_collected).data
              << "\n\n";

    // Both forms compile to the same function:
    constexpr auto df_coll_fn = math_compile<df_collected>();
    std::cout << "  f'(3) original:  " << df_fn(3.0) << "\n";
    std::cout << "  f'(3) collected: " << df_coll_fn(3.0) << "\n";

    // ── Act 4: The Grand Finale ──────────────────────────────
    //
    // Multi-variable function through the full pipeline:
    // build → differentiate → simplify → collect → compile → verify.
    // Two partial derivatives, one expression, all at compile-time.

    std::cout << "\n=== Act 4: The Grand Finale ===\n\n";

    constexpr auto y = Expr::var("y");
    constexpr auto g = x * x + x * y + y * y; // g(x,y) = x² + xy + y²

    constexpr auto diff_y = [](Expr e) consteval {
        return differentiate(e, "y");
    };

    // Full pipeline for each partial derivative:
    constexpr auto gx_raw = g | diff_x;
    constexpr auto gx = gx_raw | simp;
    constexpr auto gx_nice = rewrite(gx, collect);

    constexpr auto gy_raw = g | diff_y;
    constexpr auto gy = gy_raw | simp;
    constexpr auto gy_nice = rewrite(gy, collect);

    std::cout << "  g(x,y) = " << pretty_print(g).data << "\n\n";

    std::cout << "  dg/dx raw  = " << pretty_print(gx_raw).data << "\n";
    std::cout << "  dg/dx      = " << pretty_print(gx).data << "\n";
    std::cout << "  dg/dx      = " << pretty_print(gx_nice).data
              << "  [collect]\n\n";

    std::cout << "  dg/dy raw  = " << pretty_print(gy_raw).data << "\n";
    std::cout << "  dg/dy      = " << pretty_print(gy).data << "\n";
    std::cout << "  dg/dy      = " << pretty_print(gy_nice).data
              << "  [collect]\n\n";

    // Compile all three and verify at a known point:
    constexpr auto g_fn = math_compile<g>();
    constexpr auto gx_fn = math_compile<gx_nice>();
    constexpr auto gy_fn = math_compile<gy_nice>();

    std::cout << "  At (x=2, y=3):\n";
    std::cout << "  g(2,3)   = " << g_fn(2.0, 3.0) << "  (expected 19)\n";
    std::cout << "  dg/dx    = " << gx_fn(2.0, 3.0) << "  (expected 7)\n";
    std::cout << "  dg/dy    = " << gy_fn(2.0, 3.0) << "  (expected 8)\n\n";

    std::cout << "  All computed at compile-time. Code is data.\n";
}
