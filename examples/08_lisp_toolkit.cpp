// 08_lisp_toolkit.cpp — "The Lisp Hacker's Toolkit"
//
// Progressive showcase of refmacro's code-is-data philosophy.
// Shows: pretty_print, pipe operator, differentiate, simplify,
//        custom rewrite rules, multi-variable gradients.
//
// Run this and watch compile-time AST metaprogramming in action.

#include <iostream>
#include <optional>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

int main() {
    // ── Shared transform closures ──────────────────────────────
    constexpr auto diff_x = [](Expr e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto diff_y = [](Expr e) consteval {
        return differentiate(e, "y");
    };
    constexpr auto simp = [](Expr e) consteval { return simplify(e); };

    // ── Act 1: Code is Data ──────────────────────────────────
    //
    // In Lisp, programs are data structures you can inspect and transform.
    // refmacro brings this to C++: expressions are compile-time ASTs.

    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x + x; // f(x) = x² + x

    std::cout << "=== Act 1: Code is Data ===\n\n";
    std::cout << "  An expression is just data — an AST you can print:\n";
    std::cout << "  f(x) = " << pretty_print(f).data << "\n\n";

    constexpr auto f_fn = math_compile<f>();
    static_assert(f_fn(3.0) == 12.0);
    static_assert(f_fn(10.0) == 110.0);

    std::cout << "  ...and compiles into a function:\n";
    std::cout << "  f(3)  = " << f_fn(3.0) << "\n";
    std::cout << "  f(10) = " << f_fn(10.0) << "\n";

    // ── Act 2: Transform Pipeline ────────────────────────────
    //
    // Like Lisp's macro-expansion, we transform ASTs in stages.
    // The pipe operator chains transforms. Compare the raw
    // derivative with the simplified form to see what cleanup does.

    std::cout << "\n=== Act 2: Transform Pipeline ===\n\n";

    constexpr auto raw_df = f | diff_x;
    constexpr auto df = raw_df | simp;
    constexpr auto d2f = df | diff_x | simp;

    std::cout << "  f'(x) raw = " << pretty_print(raw_df).data << "\n";
    std::cout << "  f'(x)     = " << pretty_print(df).data << "\n";
    std::cout << "  f''(x)    = " << pretty_print(d2f).data << "\n\n";

    constexpr auto df_fn = math_compile<df>();
    constexpr auto d2f_fn = math_compile<d2f>();
    static_assert(df_fn(3.0) == 7.0);
    static_assert(d2f_fn() == 2.0);

    std::cout << "  f'(3) = " << df_fn(3.0) << "\n";
    std::cout << "  f''   = " << d2f_fn() << "  (constant — x eliminated)\n";

    // ── Act 3: Your Own Rewrite Rules ────────────────────────
    //
    // Like defmacro in Lisp, you can define your own AST transforms.
    // rewrite() applies rules bottom-up until fixed-point.

    std::cout << "\n=== Act 3: Your Own Rewrite Rules ===\n\n";

    // Rule: Distribute multiplication over addition.
    //   a * (b + c) → a*b + a*c
    //   (b + c) * a → b*a + c*a
    // rewrite() iterates to fixed-point, so nested sums like
    // a * ((b + c) + d) are fully distributed across passes.
    constexpr auto expand =
        [](NodeView<64> n) consteval -> std::optional<Expr> {
        if (n.tag() != "mul" || n.child_count() != 2)
            return std::nullopt;
        if (n.child(1).tag() == "add" && n.child(1).child_count() == 2) {
            auto a = to_expr(n, n.child(0));
            auto b = to_expr(n, n.child(1).child(0));
            auto c = to_expr(n, n.child(1).child(1));
            return a * b + a * c;
        }
        if (n.child(0).tag() == "add" && n.child(0).child_count() == 2) {
            auto a = to_expr(n, n.child(1));
            auto b = to_expr(n, n.child(0).child(0));
            auto c = to_expr(n, n.child(0).child(1));
            return b * a + c * a;
        }
        return std::nullopt;
    };

    // Rule: Collect identical variables.  x + x → 2 * x
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

    // Demo: simple distribution
    constexpr auto e1 = x * (x + 1.0);
    constexpr auto e1_expanded = rewrite(e1, expand);
    constexpr auto e1_clean = simplify(e1_expanded);

    constexpr auto e1_fn = math_compile<e1>();
    constexpr auto e1_clean_fn = math_compile<e1_clean>();
    static_assert(e1_fn(3.0) == e1_clean_fn(3.0));

    std::cout << "  expand: a*(b+c) -> a*b + a*c\n";
    std::cout << "  before:   " << pretty_print(e1).data << "\n";
    std::cout << "  expanded: " << pretty_print(e1_expanded).data << "\n";
    std::cout << "  cleaned:  " << pretty_print(e1_clean).data << "\n\n";

    // Demo: nested distribution via fixed-point iteration.
    // x * ((x + 1) + x) needs two passes to fully distribute:
    //   pass 1: x*(x+1) + x*x
    //   pass 2: x*x + x*1 + x*x
    constexpr auto e2 = x * (x + 1.0 + x);
    constexpr auto e2_expanded = rewrite(e2, expand);
    constexpr auto e2_clean = simplify(e2_expanded);

    constexpr auto e2_fn = math_compile<e2>();
    constexpr auto e2_clean_fn = math_compile<e2_clean>();
    static_assert(e2_fn(3.0) == e2_clean_fn(3.0));

    std::cout << "  Nested distribution (fixed-point):\n";
    std::cout << "  before:   " << pretty_print(e2).data << "\n";
    std::cout << "  expanded: " << pretty_print(e2_expanded).data << "\n";
    std::cout << "  cleaned:  " << pretty_print(e2_clean).data << "\n\n";

    // Demo: collect turns x + x → 2 * x in the derivative
    constexpr auto df_collected = rewrite(df, collect);
    constexpr auto df_coll_fn = math_compile<df_collected>();
    static_assert(df_fn(5.0) == df_coll_fn(5.0));

    std::cout << "  collect: x+x -> 2*x\n";
    std::cout << "  f'(x) before:  " << pretty_print(df).data << "\n";
    std::cout << "  f'(x) after:   " << pretty_print(df_collected).data << "\n";

    // ── Act 4: The Grand Finale ──────────────────────────────
    //
    // Full custom pipeline on a multivariate function:
    // differentiate → simplify → expand → collect → compile.
    // Custom rewrite rules compose with built-in transforms.

    std::cout << "\n=== Act 4: The Grand Finale ===\n\n";

    constexpr auto y = Expr::var("y");
    constexpr auto g = x * x + x * y + y * y;

    // Partial derivatives through the full pipeline:
    constexpr auto gx_s = g | diff_x | simp;
    constexpr auto gx = rewrite(rewrite(gx_s, expand), collect);

    constexpr auto gy_s = g | diff_y | simp;
    constexpr auto gy = rewrite(rewrite(gy_s, expand), collect);

    std::cout << "  g(x,y) = " << pretty_print(g).data << "\n";
    std::cout << "  dg/dx  = " << pretty_print(gx).data << "\n";
    std::cout << "  dg/dy  = " << pretty_print(gy).data << "\n\n";

    constexpr auto g_fn = math_compile<g>();
    constexpr auto gx_fn = math_compile<gx>();
    constexpr auto gy_fn = math_compile<gy>();
    static_assert(g_fn(2.0, 3.0) == 19.0);
    static_assert(gx_fn(2.0, 3.0) == 7.0);
    static_assert(gy_fn(2.0, 3.0) == 8.0);

    std::cout << "  At (x=2, y=3):\n";
    std::cout << "  g(2,3) = " << g_fn(2.0, 3.0) << "\n";
    std::cout << "  dg/dx  = " << gx_fn(2.0, 3.0) << "\n";
    std::cout << "  dg/dy  = " << gy_fn(2.0, 3.0) << "\n\n";

    std::cout << "  All computed at compile-time. Code is data.\n";
}
