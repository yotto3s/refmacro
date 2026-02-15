// Reproduction tests for types subsystem bugs found on feature/auto-defmacro.
//
// F1: typed_compile loses embedded macros — auto-only workflow fails.
//     When an expression is built using auto-tracked macros (e.g., operator+
//     via MAdd), typed_compile<expr>() with no explicit Macros loses the
//     embedded macros because:
//       (a) ann() strips macros (returns Expression<Cap>), and
//       (b) strip_types() also strips macros, and
//       (c) typed_compile does not extract embedded macros from expr's type
//           before calling compile<stripped, Macros...>().
//
// F2: ann() unconditionally strips macros from its result, breaking
//     auto-tracking for the typed compile pipeline. Even if typed_compile
//     were fixed, ann()'s return type is Expression<Cap> which discards
//     any embedded macros from the child expression.

#include <gtest/gtest.h>

#include <refmacro/compile.hpp>
#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <reftype/check.hpp>
#include <reftype/strip.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using reftype::ann;
using reftype::TInt;
using reftype::TypeEnv;

using E = Expression<128>;

// ============================================================
// F1: typed_compile without explicit macros fails for auto-tracked expressions
// ============================================================

// F1a: Expression built with auto-tracked operator+ (MAdd embedded in type)
//      passed to typed_compile with no explicit Macros.
//      Expected: compiles and returns correct result (5).
//      Actual:   compile-time error "no macro defined for AST tag" because
//                strip_types produces Expression<128> (no macros) and
//                typed_compile passes no macros to compile().
TEST(TypesSubsystemBugs, F1_TypedCompile_AutoOnly_NoExplicitMacros) {
    // Build expression with auto-tracked macros (no ann wrapping)
    static constexpr auto e = E::lit(2) + E::lit(3);
    // e has type Expression<128, MAdd> — MAdd is embedded

    // type_check works (strips macros internally, handles "add" via TRAdd)
    static constexpr auto result = reftype::type_check(e);
    static_assert(result.valid);

    // FIX: typed_compile now extracts embedded macros from expr's type
    constexpr auto f = reftype::typed_compile<e>();
    static_assert(f() == 5);
    EXPECT_EQ(f(), 5);
}

// F1b: Same issue with env overload of typed_compile.
//      Expression with auto-tracked operator+, passed to typed_compile
//      with env but no explicit Macros.
TEST(TypesSubsystemBugs, F1_TypedCompile_AutoOnly_WithEnv) {
    static constexpr auto e = E::var("x") + E::lit(1);
    // e has type Expression<128, MAdd> — MAdd embedded
    static constexpr auto env = TypeEnv<128>{}.bind("x", TInt);

    // type_check works fine
    static constexpr auto result = reftype::type_check(e, env);
    static_assert(result.valid);

    // FIX: typed_compile now extracts embedded macros from expr's type
    constexpr auto f = reftype::typed_compile<e, env>();
    static_assert(f(3) == 4);
    EXPECT_EQ(f(3), 4);
}

// ============================================================
// F2: ann() strips macros from its output, breaking auto-tracking
// ============================================================

// F2a: ann() returns Expression<Cap> (no macros), even when the child
//      expression has embedded macros from auto-tracking.
TEST(TypesSubsystemBugs, F2_AnnStripsEmbeddedMacros) {
    // Build expression with auto-tracked MAdd
    static constexpr auto expr_with_macros = E::var("x") + E::lit(1);

    // strip_types returns Expression<Cap> (no macros), confirming
    // it also strips macro tracking from the expression type.
    static constexpr auto stripped_no_ann =
        reftype::strip_types(expr_with_macros);
    (void)stripped_no_ann; // used only to verify it compiles

    // FIX: ann() now preserves macros from the child expression
    static constexpr auto annotated = ann(expr_with_macros, TInt);
    // annotated has type Expression<128, MAdd> — macros preserved

    static constexpr auto env = TypeEnv<128>{}.bind("x", TInt);
    static constexpr auto result = reftype::type_check(annotated, env);
    static_assert(result.valid);

    // typed_compile now auto-discovers MAdd from annotated's type
    constexpr auto f = reftype::typed_compile<annotated, env>();
    static_assert(f(3) == 4);
    EXPECT_EQ(f(3), 4);
}

// F2b: tref() also strips macros from its inputs.
//      If a predicate contains auto-tracked arithmetic, tref discards
//      the macro tracking. This is not a functional bug for type operations
//      (the FM solver doesn't need macros), but it means any expression
//      containing tref(...) loses all auto-tracked macros.
TEST(TypesSubsystemBugs, F2_TrefStripsEmbeddedMacros) {
    // Build a predicate with auto-tracked arithmetic
    // #v + 1 > 0  (+ creates MAdd-tracked expression, > strips)
    static constexpr auto pred =
        Expression<128>::var("#v") + Expression<128>::lit(1) >
        Expression<128>::lit(0);
    // pred already has type Expression<128> because > strips macros

    // tref also returns Expression<128>
    static constexpr auto ref_type = reftype::tref(reftype::tint(), pred);

    // Verify this works for type operations (no macros needed)
    static_assert(reftype::is_refined(ref_type));
    EXPECT_TRUE(true); // Test compiles and runs
}

// ============================================================
// F1c: typed_full_compile with custom macros not in the hardcoded list
// ============================================================

// When a user defines a custom macro and uses it in a typed expression,
// typed_full_compile won't discover it because it only passes the built-in
// set of macros. The user would need to use typed_compile with explicit macros,
// but that also fails due to F1 (ann strips macros).
//
// This is a design gap rather than a crash bug — typed_full_compile is
// documented as including "all macros" but only includes the built-in set.
// A custom macro would require typed_compile<expr, CustomMacro>() which works
// as long as macros are passed explicitly.

// ============================================================
// Verify existing pipeline still works (regression guard)
// ============================================================

TEST(TypesSubsystemBugs, RegressionGuard_TypedFullCompileWorks) {
    // typed_full_compile with ann + explicit macros still works
    static constexpr auto e = ann(E::lit(3) + E::lit(4), TInt);
    constexpr auto f = reftype::typed_full_compile<e>();
    static_assert(f() == 7);
    EXPECT_EQ(f(), 7);
}

TEST(TypesSubsystemBugs, RegressionGuard_TypedCompileExplicitMacros) {
    // typed_compile with explicit macros still works
    static constexpr auto e = ann(E::var("x") + E::lit(1), TInt);
    static constexpr auto env = TypeEnv<128>{}.bind("x", TInt);
    constexpr auto f = reftype::typed_compile<e, env, refmacro::MAdd>();
    static_assert(f(5) == 6);
    EXPECT_EQ(f(5), 6);
}

TEST(TypesSubsystemBugs, RegressionGuard_StripTypesPreservesAST) {
    // strip_types preserves the AST structure even though it strips macros
    static constexpr auto e = ann(E::lit(5) + E::lit(3), TInt);
    static constexpr auto stripped = reftype::strip_types(e);
    static constexpr auto expected = E::lit(5) + E::lit(3);
    // stripped loses macros from the type but AST content is the same
    static_assert(reftype::types_equal(stripped, expected));
}
