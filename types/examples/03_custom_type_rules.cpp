// Custom type rules with a clamp operation
//
// Demonstrates: defmacro for custom AST nodes, def_typerule for custom
// type rules, the typed compile pipeline with user-defined extensions.
// Build: cmake --build build --target 03_custom_type_rules
// Run:   ./build/types/examples/03_custom_type_rules

#include <cstdio>
#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <reftype/refinement.hpp>

using refmacro::defmacro;
using refmacro::Expression;
using reftype::ann;
using reftype::BaseKind;
using reftype::def_typerule;
using reftype::get_base_kind;
using reftype::report_error;
using reftype::TInt;
using reftype::tint;
using reftype::tref;
using reftype::type_check;
using reftype::TypeEnv;
using reftype::TypeResult;

using E = Expression<128>;
using refmacro::NodeView;
using refmacro::to_expr;

// --- Section 1: Defining the custom macro ---
//
// clamp(x, lo, hi) clamps x to the range [lo, hi].
// We define both an AST constructor and a runtime lowering macro.

// Runtime lowering: cond(x < lo, lo, cond(x > hi, hi, x))
// The macro receives compiled children as lambdas and returns a lambda.
inline constexpr auto MClamp = defmacro<"clamp">([](auto x, auto lo, auto hi) {
    return [=](auto... a) constexpr {
        auto xv = x(a...);
        auto lov = lo(a...);
        auto hiv = hi(a...);
        return xv < lov ? lov : (xv > hiv ? hiv : xv);
    };
});

// --- Section 2: Defining the custom type rule ---
//
// When the type checker encounters clamp(x, lo, hi), it synthesizes the
// refinement type {#v : Int | #v >= lo && #v <= hi}, where lo and hi are
// extracted from the children as sub-expressions.
// Validates that all three arguments are numeric.

inline constexpr auto TRClamp = def_typerule(
    "clamp", [](const auto& expr, const auto& env, auto synth_rec) {
        using Expr = std::remove_cvref_t<decltype(expr)>;
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        const auto& node = expr.ast.nodes[expr.id];

        // Type-check all three children
        auto x_res = synth_rec(Expr{expr.ast, node.children[0]}, env);
        auto lo_res = synth_rec(Expr{expr.ast, node.children[1]}, env);
        auto hi_res = synth_rec(Expr{expr.ast, node.children[2]}, env);

        // Validate: all arguments must be numeric (Int or Real)
        auto xk = get_base_kind(x_res.type);
        if (xk != BaseKind::Int && xk != BaseKind::Real)
            report_error("clamp: first argument must be numeric", "Int or Real",
                         reftype::kind_name(xk), "clamp");
        auto lok = get_base_kind(lo_res.type);
        if (lok != BaseKind::Int && lok != BaseKind::Real)
            report_error("clamp: lo bound must be numeric", "Int or Real",
                         reftype::kind_name(lok), "clamp");
        auto hik = get_base_kind(hi_res.type);
        if (hik != BaseKind::Int && hik != BaseKind::Real)
            report_error("clamp: hi bound must be numeric", "Int or Real",
                         reftype::kind_name(hik), "clamp");

        // Extract lo/hi sub-expressions for the refinement predicate.
        // to_expr copies the sub-tree into a standalone Expression, so
        // literals, variables, and arithmetic all work uniformly.
        using NV = refmacro::NodeView<Cap>;
        NV root{expr.ast, expr.id};
        auto lo_expr = refmacro::to_expr(root, NV{expr.ast, node.children[1]});
        auto hi_expr = refmacro::to_expr(root, NV{expr.ast, node.children[2]});

        // Synthesize {#v : Int | #v >= lo && #v <= hi}
        auto v = Expr::var("#v");
        auto pred = (v >= lo_expr) && (v <= hi_expr);

        return TypeResult<Cap>{reftype::tref(reftype::tint<Cap>(), pred),
                               x_res.valid && lo_res.valid && hi_res.valid};
    });

// --- Section 3: Using the custom rule ---
//
// clamp(x, 0, 100) annotated as {#v : Int | #v >= 0 && #v <= 100}.
// The custom type rule synthesizes exactly this refinement type, so the
// annotation matches and type checking succeeds.

// Helper: type check with TRClamp, strip annotations, compile with all macros
template <auto expr, auto env> consteval auto clamp_compile() {
    return reftype::typed_compile<expr, env, TRClamp>();
}

static constexpr auto clamp_type =
    tref(TInt, E::var("#v") >= E::lit(0) && E::var("#v") <= E::lit(100));
static constexpr auto clamp_expr =
    ann(MClamp(E::var("x"), E::lit(0), E::lit(100)), clamp_type);
static constexpr auto clamp_env = TypeEnv<128>{}.bind("x", TInt);

constexpr auto clamp_fn = clamp_compile<clamp_expr, clamp_env>();
static_assert(clamp_fn(50) == 50);   // in range: unchanged
static_assert(clamp_fn(-5) == 0);    // below lo: clamped to 0
static_assert(clamp_fn(200) == 100); // above hi: clamped to 100

// --- Section 4: Refinement subtyping — FM solver in action ---
//
// clamp(x, 1, 5) annotated with a WIDER type {#v : Int | #v >= 0 && #v <= 10}.
// The clamp rule synthesizes {#v >= 1 && #v <= 5}. The FM solver proves the
// implication: (#v >= 1 && #v <= 5) => (#v >= 0 && #v <= 10).
//
// Note: The design originally called for arithmetic propagation through add,
// e.g. ann(clamp(x,1,10) + lit(5), {#v >= 6 && #v <= 15}). However, the
// built-in TRAdd rule returns plain tint() rather than a refined type, so
// refinement bounds are not currently propagated through arithmetic. This
// section demonstrates the FM solver via subtype checking instead, which
// exercises the same implication-proving machinery.

static constexpr auto wider_type =
    tref(TInt, E::var("#v") >= E::lit(0) && E::var("#v") <= E::lit(10));
static constexpr auto subtype_expr =
    ann(MClamp(E::var("x"), E::lit(1), E::lit(5)), wider_type);

constexpr auto subtype_fn = clamp_compile<subtype_expr, clamp_env>();
static_assert(subtype_fn(3) == 3); // in range
static_assert(subtype_fn(0) == 1); // clamped to lo
static_assert(subtype_fn(7) == 5); // clamped to hi

// --- Section 5: Compile-time type error detection ---
//
// Uncomment to see a compile-time error:
//   The clamp rule synthesizes {#v >= 0 && #v <= 100}, but the annotation
//   demands {#v >= 0 && #v <= 50}. The FM solver proves the implication
//   FAILS: (#v >= 0 && #v <= 100) does NOT imply (#v >= 0 && #v <= 50).
//
//   static constexpr auto narrow_type =
//       tref(TInt, E::var("#v") >= E::lit(0) && E::var("#v") <= E::lit(50));
//   static constexpr auto bad_expr =
//       ann(MClamp(E::var("x"), E::lit(0), E::lit(100)), narrow_type);
//   constexpr auto bad_fn = clamp_compile<bad_expr, clamp_env>();

// --- Section 6: Variable bounds ---
//
// clamp(x, lo, hi) where lo and hi are variables, not literals.
// The type rule extracts the variable sub-expressions and synthesizes
// {#v : Int | #v >= lo && #v <= hi}. The annotation matches exactly,
// so the FM solver trivially proves the implication.

static constexpr auto var_clamp_type =
    tref(TInt, E::var("#v") >= E::var("lo") && E::var("#v") <= E::var("hi"));
static constexpr auto var_clamp_expr =
    ann(MClamp(E::var("x"), E::var("lo"), E::var("hi")), var_clamp_type);
static constexpr auto var_clamp_env =
    TypeEnv<128>{}.bind("x", TInt).bind("lo", TInt).bind("hi", TInt);

constexpr auto var_clamp_fn = clamp_compile<var_clamp_expr, var_clamp_env>();
static_assert(var_clamp_fn(5, 0, 10) == 5);   // in range: unchanged
static_assert(var_clamp_fn(-1, 0, 10) == 0);  // below lo: clamped to 0
static_assert(var_clamp_fn(15, 0, 10) == 10); // above hi: clamped to 10

// --- Section 7: Expression bounds with subtype checking ---
//
// clamp(x, lo + 1, hi - 1) with inner bounds annotated with WIDER type
// {#v : Int | #v >= lo && #v <= hi}. The type rule synthesizes
// {#v >= lo + 1 && #v <= hi - 1}. The FM solver proves:
//   (#v >= lo + 1 && #v <= hi - 1) => (#v >= lo && #v <= hi)
// because lo + 1 >= lo and hi - 1 <= hi always hold.

static constexpr auto expr_bounds_type =
    tref(TInt, E::var("#v") >= E::var("lo") && E::var("#v") <= E::var("hi"));
static constexpr auto expr_bounds_expr =
    ann(MClamp(E::var("x"), E::var("lo") + E::lit(1), E::var("hi") - E::lit(1)),
        expr_bounds_type);
static constexpr auto expr_bounds_env =
    TypeEnv<128>{}.bind("x", TInt).bind("lo", TInt).bind("hi", TInt);

constexpr auto expr_bounds_fn =
    clamp_compile<expr_bounds_expr, expr_bounds_env>();
static_assert(expr_bounds_fn(5, 0, 10) == 5);  // in range [1,9]: unchanged
static_assert(expr_bounds_fn(0, 0, 10) == 1);  // below lo+1: clamped to 1
static_assert(expr_bounds_fn(10, 0, 10) == 9); // above hi-1: clamped to 9

int main() {
    std::printf("Section 3 (clamp 50):  %d\n", static_cast<int>(clamp_fn(50)));
    std::printf("Section 3 (clamp -5):  %d\n", static_cast<int>(clamp_fn(-5)));
    std::printf("Section 3 (clamp 200): %d\n", static_cast<int>(clamp_fn(200)));
    std::printf("Section 4 (subtype 3): %d\n", static_cast<int>(subtype_fn(3)));
    std::printf("Section 4 (subtype 0): %d\n", static_cast<int>(subtype_fn(0)));
    std::printf("Section 4 (subtype 7): %d\n", static_cast<int>(subtype_fn(7)));
    std::printf("Section 6 (var 5,0,10):   %d\n",
                static_cast<int>(var_clamp_fn(5, 0, 10)));
    std::printf("Section 6 (var -1,0,10):  %d\n",
                static_cast<int>(var_clamp_fn(-1, 0, 10)));
    std::printf("Section 6 (var 15,0,10):  %d\n",
                static_cast<int>(var_clamp_fn(15, 0, 10)));
    std::printf("Section 7 (expr 5,0,10):  %d\n",
                static_cast<int>(expr_bounds_fn(5, 0, 10)));
    std::printf("Section 7 (expr 0,0,10):  %d\n",
                static_cast<int>(expr_bounds_fn(0, 0, 10)));
    std::printf("Section 7 (expr 10,0,10): %d\n",
                static_cast<int>(expr_bounds_fn(10, 0, 10)));
    std::printf("All custom type rule examples passed!\n");
    return 0;
}
