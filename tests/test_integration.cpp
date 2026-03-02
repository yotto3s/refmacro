#include <gtest/gtest.h>
#include <optional>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

// --- Custom macros (file scope for NTTP use) ---

constexpr auto Abs = defmacro<"abs">([](auto x) {
    return [=](auto... a) constexpr {
        auto v = x(a...);
        return v < 0 ? -v : v;
    };
});

constexpr auto Clamp = defmacro<"clamp">([](auto val, auto lo, auto hi) {
    return [=](auto... a) constexpr {
        auto v = val(a...);
        auto l = lo(a...);
        auto h = hi(a...);
        return v < l ? l : (v > h ? h : v);
    };
});

// --- End-to-end with defmacro ---

TEST(Integration, CustomAbsViaMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = Abs(x);
    constexpr auto fn = compile<e>();
    static_assert(fn(5.0) == 5.0);
    static_assert(fn(-3.0) == 3.0);
}

// --- End-to-end math pipeline ---

TEST(Integration, LinearFunction) {
    constexpr auto x = Expr::var("x");
    constexpr auto f = 3.0 * x + 5.0;
    constexpr auto fn = compile<f>();
    static_assert(fn(2.0) == 11.0);
    static_assert(fn(0.0) == 5.0);

    constexpr auto diff_x = [](auto e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](auto e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto dfn = compile<df>();
    static_assert(dfn(100.0) == 3.0);
}

TEST(Integration, QuadraticRoots) {
    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x - 4.0 * x + 4.0;
    constexpr auto fn = compile<f>();
    static_assert(fn(2.0) == 0.0);

    constexpr auto diff_x = [](auto e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](auto e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto dfn = compile<df>();
    static_assert(dfn(2.0) == 0.0); // minimum
}

TEST(Integration, MultivarGradient) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto f = x * y + x + y;
    constexpr auto fn = compile<f>();
    static_assert(fn(2.0, 3.0) == 11.0);

    constexpr auto dfx = simplify(differentiate(f, "x"));
    constexpr auto dfx_fn = compile<dfx>();
    static_assert(dfx_fn(3.0) ==
                  4.0); // y + 1, only y remains after simplification

    constexpr auto dfy = simplify(differentiate(f, "y"));
    constexpr auto dfy_fn = compile<dfy>();
    static_assert(dfy_fn(2.0) ==
                  3.0); // x + 1, only x remains after simplification
}

TEST(Integration, SecondDerivative) {
    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x * x;
    constexpr auto diff_x = [](auto e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](auto e) consteval { return simplify(e); };
    constexpr auto d2f = f | diff_x | simp | diff_x | simp;
    constexpr auto fn = compile<d2f>();
    static_assert(fn(2.0) == 12.0); // 6*2
}

// --- Mixing custom macros with math ---

TEST(Integration, CustomMacroWithMath) {
    constexpr auto x = Expr::var("x");
    // clamp(x * x - 10, 0, 100)
    constexpr auto e = Clamp(x * x - 10.0, Expr::lit(0.0), Expr::lit(100.0));
    constexpr auto fn = compile<e>();
    static_assert(fn(1.0) == 0.0);    // 1-10 = -9, clamped to 0
    static_assert(fn(4.0) == 6.0);    // 16-10 = 6
    static_assert(fn(20.0) == 100.0); // 400-10 = 390, clamped to 100
}

TEST(Integration, PrettyPrint) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = x * x + 1.0;
    constexpr auto s = pretty_print(e);
    static_assert(s == "((x * x) + 1)");
}

TEST(Integration, RuntimeCalls) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto f = x * x + y * y;
    constexpr auto fn = compile<f>();
    EXPECT_DOUBLE_EQ(fn(3.0, 4.0), 25.0);
    EXPECT_DOUBLE_EQ(fn(0.0, 0.0), 0.0);
}

// --- TG2: differentiate preserving non-math macros ---

TEST(Integration, DifferentiatePreservesControlFlowMacros) {
    // Build an expression mixing math and control-flow: cond(x > 0, x*x, -x)
    // differentiate only knows math tags; control-flow nodes fall through to
    // lit(0) in the differentiation visitor. The key test is that the result
    // compiles successfully with all macros auto-discovered.
    constexpr auto x = Expr::var("x");
    constexpr auto branch_expr = x * x + 1.0;

    // Differentiate the math part, which has control-flow macros in the type
    // because MCond added them when we built the larger expression.
    // Use MCond to create a mixed-macro expression, then differentiate a
    // subpart.
    constexpr auto cond_expr =
        MCond(x > Expr::lit(0.0), branch_expr, Expr::lit(0.0));

    // The expression type includes MCond, MGt, MAdd, MMul etc.
    // differentiate adds MAdd, MSub, MMul, MDiv, MNeg to the type.
    // Non-math tags are treated as constants (return 0) by differentiate.
    constexpr auto df = differentiate(cond_expr, "x");
    constexpr auto sdf = simplify(df);

    // The result should compile with all macros (math + control)
    // auto-discovered
    constexpr auto fn = compile<sdf>();
    // differentiate treats unknown tags as constants -> d/dx(cond(...)) = 0
    EXPECT_DOUBLE_EQ(fn(), 0.0);
}

// --- TG3: merged embedded + explicit macros ---

constexpr auto Double = defmacro<"double_it">(
    [](auto x) { return [=](auto... a) constexpr { return x(a...) * 2.0; }; });

TEST(Integration, MergedEmbeddedAndExplicitMacros) {
    // Build an expression with math macros embedded in the type
    constexpr auto x = Expr::var("x");
    constexpr auto e = x + 1.0; // type has MAdd embedded

    // Manually create a node using a macro (Double) that is NOT in e's type
    // We use make_node to add a "double_it" tag without auto-tracking
    constexpr auto wrapped = make_node("double_it", e);

    // compile<wrapped, Double>() provides Double explicitly while e's MAdd
    // is auto-discovered from the embedded macros
    constexpr auto fn = compile<wrapped, Double>();

    // double_it(x + 1) = (x + 1) * 2
    static_assert(fn(3.0) == 8.0);   // (3 + 1) * 2 = 8
    EXPECT_DOUBLE_EQ(fn(4.0), 10.0); // (4 + 1) * 2 = 10
}

// --- TG4: auto-discovery through rewrite ---

TEST(Integration, AutoDiscoveryThroughRewrite) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");

    // Build expression: x * y + x * 0 (has MAdd, MMul in type)
    constexpr auto e = x * y + x * 0.0;

    // Apply a custom rewrite rule to remove multiplication by zero
    constexpr auto cleaned =
        rewrite(e, [](NodeView<64> n) consteval -> std::optional<Expr> {
            if (n.tag() == "mul" && n.child_count() == 2) {
                if (n.child(1).tag() == "lit" && n.child(1).payload() == 0.0)
                    return Expr::lit(0.0);
                if (n.child(0).tag() == "lit" && n.child(0).payload() == 0.0)
                    return Expr::lit(0.0);
            }
            if (n.tag() == "add" && n.child_count() == 2) {
                if (n.child(1).tag() == "lit" && n.child(1).payload() == 0.0)
                    return to_expr(n, n.child(0));
            }
            return std::nullopt;
        });

    // After rewrite, the expression should be x * y
    // Macros (MAdd, MMul) are preserved in the type through rewrite
    constexpr auto fn = compile<cleaned>();
    static_assert(fn(3.0, 4.0) == 12.0);
    EXPECT_DOUBLE_EQ(fn(5.0, 6.0), 30.0);
}
