#include <gtest/gtest.h>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

// ============================================================================
// Bug-hunting tests for the simplified compile API (auto-discovery) and the
// differentiate() return type change.
// ============================================================================

// --- F1: Duplicate macros in the type after repeated differentiation ---
//
// differentiate() prepends MAdd, MSub, MMul, MDiv, MNeg to the return type.
// If the input already has these macros (from math operators), the resulting
// Expression type has duplicates. Verify compile handles this correctly.

TEST(CompileApiBugs_F1, DuplicateMacrosAfterDifferentiate) {
    // f(x) = x * x has type Expression<64, MMul> (one math macro from
    // operator*)
    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x;

    // differentiate adds MAdd, MSub, MMul, MDiv, MNeg before the existing pack
    // Result type: Expression<64, MAdd, MSub, MMul, MDiv, MNeg, MMul>
    // Note: MMul appears twice
    constexpr auto df = differentiate(f, "x");
    constexpr auto sdf = simplify(df);
    constexpr auto fn = compile<sdf>();

    // d/dx(x^2) = 2x after simplification
    // The product rule gives: x*1 + 1*x = x + x
    // Simplification should produce x + x (no further simplification rule for
    // x+x) fn(3.0) should be 6.0
    static_assert(fn(3.0) == 6.0);
    EXPECT_DOUBLE_EQ(fn(3.0), 6.0);
    EXPECT_DOUBLE_EQ(fn(0.0), 0.0);
}

TEST(CompileApiBugs_F1, TripleDifferentiationMacroAccumulation) {
    // Each differentiate() call adds 5 macros to the type.
    // After 3 calls, the type has 15+ macros (with duplicates).
    // Verify compile still works.
    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x * x; // x^3

    constexpr auto diff_x = [](auto e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](auto e) consteval { return simplify(e); };

    // d/dx(x^3) = 3x^2-ish (before simplify)
    // d^2/dx^2(x^3) = 6x-ish (before simplify)
    // d^3/dx^3(x^3) = 6 (constant)
    constexpr auto d3f = f | diff_x | simp | diff_x | simp | diff_x | simp;
    constexpr auto fn = compile<d3f>();

    // Third derivative of x^3 is 6
    static_assert(fn() == 6.0);
    EXPECT_DOUBLE_EQ(fn(), 6.0);
}

// --- F2: differentiate on bare Expression (no macros in input) ---
//
// If an expression is built with make_node (no MacroCaller tracking),
// differentiate() should still add math macros to the return type,
// making the result compilable.

TEST(CompileApiBugs_F2, DifferentiateBareMakeNodeExpr) {
    // Build x * x using make_node (no macro tracking)
    constexpr auto x = Expr::var("x");
    constexpr auto bare = make_node("mul", x, x);
    // bare has type Expression<64> -- no macros

    // differentiate should return Expression<64, MAdd, MSub, MMul, MDiv, MNeg>
    constexpr auto df = differentiate(bare, "x");
    constexpr auto sdf = simplify(df);

    // Should compile successfully because differentiate added math macros
    constexpr auto fn = compile<sdf>();

    // d/dx(x^2) = x + x = 2x behavior
    static_assert(fn(3.0) == 6.0);
    EXPECT_DOUBLE_EQ(fn(3.0), 6.0);
}

// --- F3: simplify preserves macro type from differentiate ---
//
// When simplify reduces an expression to a constant (eliminates all variables),
// the resulting Expression still has the macro type from its input. Verify
// that compile works on such a zero-arity function.

TEST(CompileApiBugs_F3, SimplifyToConstantPreservesMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto f = 2.0 * x + 1.0; // linear

    constexpr auto diff_x = [](auto e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](auto e) consteval { return simplify(e); };

    // d/dx(2x + 1) = 2 (constant)
    constexpr auto df = f | diff_x | simp;

    // Differentiate again to get d^2/dx^2 = 0
    constexpr auto d2f = df | diff_x | simp;
    constexpr auto fn = compile<d2f>();

    // Second derivative of a linear function is 0
    static_assert(fn() == 0.0);
    EXPECT_DOUBLE_EQ(fn(), 0.0);
}

// --- F4: Pipe chain with rewrite preserves macros through auto lambda ---
//
// When using (auto e) in pipe lambdas, rewrite/transform should preserve
// the macro type from the input expression, making the result compilable
// via compile<>().

TEST(CompileApiBugs_F4, PipeRewritePreservesMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");

    // Expression with math macros
    constexpr auto e = x * y + 0.0; // add with zero -- simplifiable

    // Custom rewrite via pipe with auto lambda
    constexpr auto remove_add_zero = [](auto e) consteval {
        return rewrite(e, [](NodeView<64> n) consteval -> std::optional<Expr> {
            if (n.tag() == "add" && n.child_count() == 2) {
                if (n.child(1).tag() == "lit" && n.child(1).payload() == 0.0)
                    return to_expr(n, n.child(0));
            }
            return std::nullopt;
        });
    };

    constexpr auto e_clean = e | remove_add_zero;

    // e_clean should have macros from e (MAdd, MMul from the original
    // expression) Even though add was removed from the AST, the type still has
    // MAdd, MMul
    constexpr auto fn = compile<e_clean>();
    static_assert(fn(3.0, 4.0) == 12.0); // x * y
    EXPECT_DOUBLE_EQ(fn(3.0, 4.0), 12.0);
}

// --- F5: Mixed custom + math macros with auto-discovery ---
//
// Custom macros defined via defmacro + math operators should all be
// auto-discovered without explicit macro lists.

constexpr auto Square = defmacro<"square">([](auto x) {
    return [=](auto... a) constexpr {
        auto v = x(a...);
        return v * v;
    };
});

TEST(CompileApiBugs_F5, CustomMacroWithMathAutoDiscovery) {
    constexpr auto x = Expr::var("x");
    // square(x) + 1.0
    constexpr auto e = Square(x) + 1.0;
    // e type should include Square and MAdd

    constexpr auto fn = compile<e>();
    static_assert(fn(3.0) == 10.0); // 9 + 1
    static_assert(fn(0.0) == 1.0);  // 0 + 1
    EXPECT_DOUBLE_EQ(fn(3.0), 10.0);
}

// --- F6: differentiate then control-flow compile ---
//
// After differentiation (which adds math macros), using the result
// in a control-flow expression should work because control macros
// are auto-tracked and math macros come from differentiate.

TEST(CompileApiBugs_F6, DifferentiateThenControlFlow) {
    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x; // x^2

    constexpr auto diff_x = [](auto e) consteval {
        return differentiate(e, "x");
    };
    constexpr auto simp = [](auto e) consteval { return simplify(e); };

    constexpr auto df = f | diff_x | simp; // 2x (approximately x + x)

    // Use the derivative in a conditional: if df > 0, return df, else return 0
    // This mixes math macros (from differentiate) with control macros
    constexpr auto clamped = MCond(df > Expr::lit(0.0), df, Expr::lit(0.0));
    constexpr auto fn = compile<clamped>();

    // For x = 3: df = 6 > 0, so result is 6
    EXPECT_DOUBLE_EQ(fn(3.0), 6.0);
    // For x = -3: df = -6 < 0, so result is 0
    EXPECT_DOUBLE_EQ(fn(-3.0), 0.0);
    // For x = 0: df = 0, not > 0, so result is 0
    EXPECT_DOUBLE_EQ(fn(0.0), 0.0);
}

// --- F7: compile on literal-only expression (zero macros needed) ---
//
// An expression that is just a literal should compile with zero macros
// since lit/var are built-in tags.

TEST(CompileApiBugs_F7, CompilePureLiteral) {
    constexpr auto e = Expr::lit(42.0);
    constexpr auto fn = compile<e>();
    static_assert(fn() == 42.0);
    EXPECT_DOUBLE_EQ(fn(), 42.0);
}

TEST(CompileApiBugs_F7, CompilePureVar) {
    constexpr auto e = Expr::var("x");
    constexpr auto fn = compile<e>();
    static_assert(fn(7.0) == 7.0);
    EXPECT_DOUBLE_EQ(fn(7.0), 7.0);
}

// --- F8: let_ with differentiated expression ---
//
// Combining let_ bindings with differentiated expressions should work
// because both control macros (from let_/apply/lambda structure) and
// math macros (from differentiate) are present in the type.

TEST(CompileApiBugs_F8, LetWithDifferentiatedExpr) {
    constexpr auto x = Expr::var("x");
    constexpr auto f = x * x;                            // x^2
    constexpr auto df = simplify(differentiate(f, "x")); // ~2x

    // let slope = d/dx(x^2) in slope + 1
    // The df expression has math macros from differentiate.
    // let_ should preserve them.
    constexpr auto e = let_("slope", df, Expr::var("slope") + Expr::lit(1.0));
    constexpr auto fn = compile<e>();

    // x=3: slope = 3+3 = 6, result = 6 + 1 = 7
    EXPECT_DOUBLE_EQ(fn(3.0), 7.0);
    // x=0: slope = 0, result = 0 + 1 = 1
    EXPECT_DOUBLE_EQ(fn(0.0), 1.0);
}
