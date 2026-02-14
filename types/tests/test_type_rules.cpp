#include <gtest/gtest.h>

#include <refmacro/control.hpp>
#include <reftype/check.hpp>
#include <reftype/typerule.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using reftype::BaseKind;
using reftype::def_typerule;
using reftype::get_base_kind;
using reftype::is_subtype;
using reftype::synth;
using reftype::TInt;
using reftype::TReal;
using reftype::type_check;
using reftype::TypeEnv;
using reftype::TypeResult;
using reftype::types_equal;

using E = Expression<128>;

// --- User-defined custom type rule: "double_it" ---
// A custom AST node that type-checks like unary numeric: child must be
// Int or Real, result is the same numeric type.
inline constexpr auto TRDoubleIt = def_typerule(
    "double_it", [](const auto& expr, const auto& env, auto synth_rec) {
        using ExprT = std::remove_cvref_t<decltype(expr)>;
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        const auto& node = expr.ast.nodes[expr.id];
        auto child = synth_rec(ExprT{expr.ast, node.children[0]}, env);
        auto ck = reftype::get_base_kind(child.type);
        if (ck != reftype::BaseKind::Int && ck != reftype::BaseKind::Real)
            reftype::report_error("non-numeric operand in double_it",
                                  "Int or Real", reftype::kind_name(ck),
                                  "double_it");
        auto result_type = (ck == reftype::BaseKind::Int)
                               ? reftype::tint<Cap>()
                               : reftype::treal<Cap>();
        return decltype(child){result_type, child.valid};
    });

// --- Tests ---

TEST(TypeRules, DefTyperuleCreatesRule) {
    // Verify def_typerule produces a rule with the correct tag
    static_assert(refmacro::str_eq(TRDoubleIt.tag, "double_it"));
}

TEST(TypeRules, CustomRuleIntOperand) {
    // double_it(x) where x : Int → Int
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", TInt);
    constexpr auto expr = refmacro::make_node<128>("double_it", E::var("x"));
    constexpr auto r = type_check<TRDoubleIt>(expr, env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeRules, CustomRuleRealOperand) {
    // double_it(x) where x : Real → Real
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", TReal);
    constexpr auto expr = refmacro::make_node<128>("double_it", E::var("x"));
    constexpr auto r = type_check<TRDoubleIt>(expr, env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TReal));
}

TEST(TypeRules, CustomRuleLiteralOperand) {
    // double_it(lit(5)) → Int (lit(5) has singleton Int type)
    constexpr auto expr = refmacro::make_node<128>("double_it", E::lit(5));
    constexpr auto r = type_check<TRDoubleIt>(expr);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeRules, CustomRuleInArithmetic) {
    // double_it(x) + y where x : Int, y : Int → Int
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", TInt).bind("y", TInt);
    constexpr auto di = refmacro::make_node<128>("double_it", E::var("x"));
    constexpr auto expr = di + E::var("y");
    constexpr auto r = type_check<TRDoubleIt>(expr, env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeRules, BuiltinRulesStillWork) {
    // Verify built-in rules work when extending with custom rules
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", TInt).bind("y", TInt);
    constexpr auto r = type_check<TRDoubleIt>(E::var("x") + E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeRules, DefaultTypeCheckUnchanged) {
    // Default type_check (no extra rules) still works
    constexpr auto r = type_check(E::lit(42));
    static_assert(r.valid);
    static_assert(get_base_kind(r.type) == BaseKind::Int);
}

TEST(TypeRules, SynthWithExplicitRules) {
    // Direct synth<Rules...> with only arithmetic + custom rule
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", TInt);
    constexpr auto expr = refmacro::make_node<128>("double_it", E::var("x"));
    constexpr auto r = synth<TRDoubleIt>(expr, env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}
