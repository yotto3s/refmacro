// Compile-time verified Forth subset
//
// A stack-based DSL where refinement types track stack depth. The FM solver
// proves at compile time that no stack underflow occurs. Branches produce
// range types; loops require net-zero body effect. Impossible in standard C++.
//
// Build: cmake --build build --target 04_forth_dsl
// Run:   ./build/types/examples/04_forth_dsl

#include <cstdio>
#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <reftype/refinement.hpp>

using refmacro::Expression;
using refmacro::make_node;
using refmacro::NodeView;
using reftype::ann;
using reftype::BaseKind;
using reftype::def_typerule;
using reftype::get_base_kind;
using reftype::get_refined_base;
using reftype::get_refined_pred;
using reftype::is_refined;
using reftype::is_subtype;
using reftype::report_error;
using reftype::tint;
using reftype::TInt;
using reftype::tref;
using reftype::type_check;
using reftype::TypeEnv;
using reftype::TypeResult;

using E = Expression<128>;

// ===================================================================
// Helpers: depth types and bounds extraction
// ===================================================================

// Singleton depth type: {#v : Int | #v == N}
template <std::size_t Cap = 128> consteval Expression<Cap> depth(int n) {
    return tref<Cap>(tint<Cap>(),
                     Expression<Cap>::var("#v") == Expression<Cap>::lit(n));
}

// Range depth type: {#v : Int | #v >= lo && #v <= hi}
template <std::size_t Cap = 128>
consteval Expression<Cap> depth_range(int lo, int hi) {
    if (lo == hi)
        return depth<Cap>(lo);
    return tref<Cap>(tint<Cap>(),
                     Expression<Cap>::var("#v") >= Expression<Cap>::lit(lo) &&
                         Expression<Cap>::var("#v") <=
                             Expression<Cap>::lit(hi));
}

// Bounds extraction from a type
struct Bounds {
    int lo{0};
    int hi{0};
    bool ok{false};
};

template <std::size_t Cap>
consteval Bounds extract_bounds(const Expression<Cap>& type) {
    if (!is_refined(type))
        return {};
    auto base = get_refined_base(type);
    if (!refmacro::str_eq(base.ast.nodes[base.id].tag, "tint"))
        return {};

    auto pred = get_refined_pred(type);
    const auto& root = pred.ast.nodes[pred.id];

    // Singleton: eq(var("#v"), lit(N))
    if (refmacro::str_eq(root.tag, "eq") && root.child_count == 2) {
        const auto& lhs = pred.ast.nodes[root.children[0]];
        const auto& rhs = pred.ast.nodes[root.children[1]];
        if (refmacro::str_eq(lhs.tag, "var") &&
            refmacro::str_eq(lhs.name, "#v") &&
            refmacro::str_eq(rhs.tag, "lit")) {
            int n = static_cast<int>(rhs.payload);
            return {n, n, true};
        }
    }

    // Range: land(ge(var("#v"), lit(lo)), le(var("#v"), lit(hi)))
    if (refmacro::str_eq(root.tag, "land") && root.child_count == 2) {
        const auto& ge_node = pred.ast.nodes[root.children[0]];
        const auto& le_node = pred.ast.nodes[root.children[1]];
        if (refmacro::str_eq(ge_node.tag, "ge") && ge_node.child_count == 2 &&
            refmacro::str_eq(le_node.tag, "le") && le_node.child_count == 2) {
            const auto& ge_lhs = pred.ast.nodes[ge_node.children[0]];
            const auto& ge_rhs = pred.ast.nodes[ge_node.children[1]];
            const auto& le_lhs = pred.ast.nodes[le_node.children[0]];
            const auto& le_rhs = pred.ast.nodes[le_node.children[1]];
            if (refmacro::str_eq(ge_lhs.tag, "var") &&
                refmacro::str_eq(ge_lhs.name, "#v") &&
                refmacro::str_eq(ge_rhs.tag, "lit") &&
                refmacro::str_eq(le_lhs.tag, "var") &&
                refmacro::str_eq(le_lhs.name, "#v") &&
                refmacro::str_eq(le_rhs.tag, "lit")) {
                int lo = static_cast<int>(ge_rhs.payload);
                int hi = static_cast<int>(le_rhs.payload);
                return {lo, hi, true};
            }
        }
    }

    return {};
}

// Minimum-depth requirement type: {#v : Int | #v >= N}
template <std::size_t Cap = 128> consteval Expression<Cap> min_depth(int n) {
    return tref<Cap>(tint<Cap>(),
                     Expression<Cap>::var("#v") >= Expression<Cap>::lit(n));
}

// ===================================================================
// Forth AST constructors
// ===================================================================

template <std::size_t Cap = 128> consteval Expression<Cap> f_new() {
    return make_node<Cap>("f_new");
}

template <std::size_t Cap = 128>
consteval Expression<Cap> f_push(int n, Expression<Cap> s) {
    return make_node("f_push", Expression<Cap>::lit(n), s);
}

template <std::size_t Cap = 128>
consteval Expression<Cap> f_dup(Expression<Cap> s) {
    return make_node("f_dup", s);
}

template <std::size_t Cap = 128>
consteval Expression<Cap> f_drop(Expression<Cap> s) {
    return make_node("f_drop", s);
}

template <std::size_t Cap = 128>
consteval Expression<Cap> f_swap(Expression<Cap> s) {
    return make_node("f_swap", s);
}

template <std::size_t Cap = 128>
consteval Expression<Cap> f_add(Expression<Cap> s) {
    return make_node("f_add", s);
}

template <std::size_t Cap = 128>
consteval Expression<Cap> f_sub(Expression<Cap> s) {
    return make_node("f_sub", s);
}

template <std::size_t Cap = 128>
consteval Expression<Cap> f_mul(Expression<Cap> s) {
    return make_node("f_mul", s);
}

template <std::size_t Cap = 128>
consteval Expression<Cap> f_if(Expression<Cap> s, Expression<Cap> then_fn,
                               Expression<Cap> else_fn) {
    return make_node("f_if", s, then_fn, else_fn);
}

template <std::size_t Cap = 128>
consteval Expression<Cap> f_times(int count, Expression<Cap> body_fn,
                                  Expression<Cap> s) {
    return make_node("f_times", Expression<Cap>::lit(count), body_fn, s);
}

// ===================================================================
// Forth macros — runtime lowering (value = depth tracking)
// ===================================================================
// Only basic ops need macros. f_if and f_times are rewritten into
// apply(lambda) form before compilation, so they don't need macros.

inline constexpr auto MFNew = refmacro::defmacro(
    "f_new", []() { return [](auto...) constexpr { return 0; }; });

inline constexpr auto MFPush =
    refmacro::defmacro("f_push", [](auto /*n*/, auto s) {
        return [=](auto... a) constexpr { return s(a...) + 1; };
    });

inline constexpr auto MFDup = refmacro::defmacro("f_dup", [](auto s) {
    return [=](auto... a) constexpr { return s(a...) + 1; };
});

inline constexpr auto MFDrop = refmacro::defmacro("f_drop", [](auto s) {
    return [=](auto... a) constexpr { return s(a...) - 1; };
});

inline constexpr auto MFSwap = refmacro::defmacro("f_swap", [](auto s) {
    return [=](auto... a) constexpr { return s(a...); };
});

inline constexpr auto MFAdd = refmacro::defmacro("f_add", [](auto s) {
    return [=](auto... a) constexpr { return s(a...) - 1; };
});

inline constexpr auto MFSub = refmacro::defmacro("f_sub", [](auto s) {
    return [=](auto... a) constexpr { return s(a...) - 1; };
});

inline constexpr auto MFMul = refmacro::defmacro("f_mul", [](auto s) {
    return [=](auto... a) constexpr { return s(a...) - 1; };
});

// ===================================================================
// Forth type rules — compile-time stack depth verification
// ===================================================================

// Helper: synthesize child and extract bounds (used by most rules)
template <std::size_t Cap>
consteval auto synth_and_bounds(const Expression<Cap>& expr, int child_idx,
                                const TypeEnv<Cap>& env, auto synth_rec) {
    using ExprT = Expression<Cap>;
    const auto& node = expr.ast.nodes[expr.id];
    auto child_r = synth_rec(ExprT{expr.ast, node.children[child_idx]}, env);
    auto bounds = extract_bounds(child_r.type);
    struct Result {
        TypeResult<Cap> tr;
        Bounds b;
    };
    return Result{child_r, bounds};
}

inline constexpr auto TRFNew = def_typerule(
    "f_new", [](const auto& /*expr*/, const auto& /*env*/, auto /*synth*/) {
        constexpr std::size_t Cap = 128;
        return TypeResult<Cap>{depth<Cap>(0), true};
    });

// Helper macro for unary stack ops that shift depth by delta and require
// min_depth
#define FORTH_UNARY_RULE(NAME, TAG, DELTA, MIN_DEPTH)                          \
    inline constexpr auto NAME = def_typerule(                                 \
        TAG, [](const auto& expr, const auto& env, auto synth_rec) {           \
            constexpr auto Cap =                                               \
                sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);            \
            const auto& node = expr.ast.nodes[expr.id];                        \
            auto [tr, b] = synth_and_bounds<Cap>(                              \
                expr, refmacro::str_eq(node.tag, "f_push") ? 1 : 0, env,       \
                synth_rec);                                                    \
            if (!b.ok)                                                         \
                report_error(TAG ": cannot determine stack depth", TAG);       \
            if constexpr ((MIN_DEPTH) > 0) {                                   \
                if (!is_subtype(tr.type, min_depth<Cap>(MIN_DEPTH)))           \
                    report_error(TAG ": stack underflow",                      \
                                 "depth >= " #MIN_DEPTH, "insufficient depth", \
                                 TAG);                                         \
            }                                                                  \
            return TypeResult<Cap>{                                            \
                depth_range<Cap>(b.lo + (DELTA), b.hi + (DELTA)), tr.valid};   \
        })

FORTH_UNARY_RULE(TRFPush, "f_push", +1, 0);
FORTH_UNARY_RULE(TRFDup, "f_dup", +1, 1);
FORTH_UNARY_RULE(TRFDrop, "f_drop", -1, 1);
FORTH_UNARY_RULE(TRFSwap, "f_swap", 0, 2);
FORTH_UNARY_RULE(TRFAdd, "f_add", -1, 2);
FORTH_UNARY_RULE(TRFSub, "f_sub", -1, 2);
FORTH_UNARY_RULE(TRFMul, "f_mul", -1, 2);

#undef FORTH_UNARY_RULE

// f_if: pops condition, runs one of two lambda branches, returns range
inline constexpr auto TRFIf =
    def_typerule("f_if", [](const auto& expr, const auto& env, auto synth_rec) {
        using ExprT = std::remove_cvref_t<decltype(expr)>;
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        const auto& node = expr.ast.nodes[expr.id];

        // Child 0: stack with condition on top
        auto stack_r = synth_rec(ExprT{expr.ast, node.children[0]}, env);
        auto sb = extract_bounds(stack_r.type);
        if (!sb.ok)
            report_error("f_if: cannot determine stack depth", "f_if");
        if (!is_subtype(stack_r.type, min_depth<Cap>(1)))
            report_error("f_if: need >= 1 for condition", "depth >= 1",
                         "insufficient depth", "f_if");

        // Depth after popping condition
        auto popped = depth_range<Cap>(sb.lo - 1, sb.hi - 1);

        // Children 1 and 2 must be lambda("d", body)
        auto check_lambda = [&](int child_idx) {
            const auto& fn_node = expr.ast.nodes[node.children[child_idx]];
            if (!refmacro::str_eq(fn_node.tag, "lambda"))
                report_error("f_if: branch must be a lambda", "f_if");
            auto param_name = expr.ast.nodes[fn_node.children[0]].name;
            ExprT body{expr.ast, fn_node.children[1]};
            auto branch_env = env.bind(param_name, popped);
            return synth_rec(body, branch_env);
        };

        auto then_r = check_lambda(1);
        auto else_r = check_lambda(2);

        auto tb = extract_bounds(then_r.type);
        auto eb = extract_bounds(else_r.type);
        if (!tb.ok || !eb.ok)
            report_error("f_if: cannot determine branch depths", "f_if");

        // Result: range covering both branches
        int rlo = tb.lo < eb.lo ? tb.lo : eb.lo;
        int rhi = tb.hi > eb.hi ? tb.hi : eb.hi;

        bool valid = stack_r.valid && then_r.valid && else_r.valid;
        return TypeResult<Cap>{depth_range<Cap>(rlo, rhi), valid};
    });

// f_times: body must have net-zero effect
inline constexpr auto TRFTimes = def_typerule(
    "f_times", [](const auto& expr, const auto& env, auto synth_rec) {
        using ExprT = std::remove_cvref_t<decltype(expr)>;
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        const auto& node = expr.ast.nodes[expr.id];

        // Child 2: stack state
        auto stack_r = synth_rec(ExprT{expr.ast, node.children[2]}, env);
        auto sb = extract_bounds(stack_r.type);
        if (!sb.ok)
            report_error("f_times: cannot determine stack depth", "f_times");

        // Child 1: body lambda
        const auto& body_node = expr.ast.nodes[node.children[1]];
        if (!refmacro::str_eq(body_node.tag, "lambda"))
            report_error("f_times: body must be a lambda", "f_times");
        auto param_name = expr.ast.nodes[body_node.children[0]].name;
        ExprT body{expr.ast, body_node.children[1]};
        auto body_env = env.bind(param_name, stack_r.type);
        auto body_r = synth_rec(body, body_env);

        auto bb = extract_bounds(body_r.type);
        if (!bb.ok)
            report_error("f_times: cannot determine body depth", "f_times");

        // Net-zero check: body output bounds must equal input bounds
        if (bb.lo != sb.lo || bb.hi != sb.hi)
            report_error("f_times: body must have net-zero stack effect",
                         "f_times");

        bool valid = stack_r.valid && body_r.valid;
        return TypeResult<Cap>{stack_r.type, valid};
    });

// ===================================================================
// rewrite_forth: transform f_if/f_times into apply(lambda) form
// ===================================================================
// compile_node only handles apply(lambda(param, body), val) as a built-in.
// Bare lambda nodes inside f_if/f_times would fail compilation because
// there's no "lambda" macro. This transform rewrites them:
//   f_if(s, lambda(d, body1), lambda(d, body2))
//     => apply(lambda(d, body1), sub(s, lit(1)))
//   f_times(count, lambda(d, body), s)
//     => s  (net-zero effect proven by type checker)

template <std::size_t Cap = 128>
consteval Expression<Cap> rewrite_forth(Expression<Cap> e) {
    return refmacro::transform(
        e, [](NodeView<Cap> n, auto rec) consteval -> Expression<Cap> {
            // f_if(s, lambda(d, body1), lambda(d, body2))
            // => apply(lambda(d, rec(body1)), sub(rec(s), lit(1)))
            if (n.tag() == "f_if") {
                auto s_expr = rec(n.child(0));

                // Extract then-branch lambda: child(1) = lambda(d, body1)
                auto then_lambda = n.child(1);
                auto param_name = then_lambda.child(0).name();
                auto body_expr = rec(then_lambda.child(1));

                // Build: sub(s, lit(1)) — depth after popping condition
                auto depth_val = s_expr - Expression<Cap>::lit(1);

                // Build: lambda(param, body)
                char pname[16]{};
                for (std::size_t i = 0; i < 16 && i < param_name.size(); ++i)
                    pname[i] = param_name[i];
                auto new_lambda = refmacro::lambda<Cap>(pname, body_expr);

                // Build: apply(lambda(d, body1), sub(s, 1))
                return refmacro::apply(new_lambda, depth_val);
            }

            // f_times(count, lambda(d, body), s)
            // => rec(s)  (net-zero body effect, depth unchanged)
            if (n.tag() == "f_times") {
                return rec(n.child(2)); // Just return the stack expression
            }

            // Leaf nodes: copy
            if (n.child_count() == 0) {
                Expression<Cap> leaf;
                leaf.id = leaf.ast.add_node(n.ast.nodes[n.id]);
                return leaf;
            }

            // Generic parent: rebuild with recursed children
            const auto* tag = n.ast.nodes[n.id].tag;
            auto first = rec(n.child(0));
            Expression<Cap> result;
            result.ast = first.ast;
            int ids[8]{first.id};
            for (int i = 1; i < n.child_count(); ++i) {
                auto child = rec(n.child(i));
                ids[i] = child.id + result.ast.merge(child.ast);
            }
            result.id = result.ast.add_tagged_node(tag, ids, n.child_count());
            return result;
        });
}

// ===================================================================
// Forth compile pipeline
// ===================================================================
// Pipeline: type_check → strip_types → rewrite_forth → compile
// f_if/f_times are rewritten into apply(lambda) form before compile,
// so no MFIf/MFTimes macros needed. MSub IS needed for the sub(s, 1)
// nodes introduced by rewrite_forth.

template <auto expr, auto env> consteval auto forth_compile() {
    using namespace refmacro;
    constexpr auto result =
        type_check<TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap, TRFAdd, TRFSub,
                   TRFMul, TRFIf, TRFTimes>(expr, env);
    static_assert(result.valid, "forth_compile: type check failed");
    constexpr auto stripped = reftype::strip_types(expr);
    constexpr auto rewritten = rewrite_forth(stripped);
    return compile<rewritten, MFNew, MFPush, MFDup, MFDrop, MFSwap, MFAdd,
                   MFSub, MFMul, MAdd, MSub, MMul, MDiv, MNeg, MCond, MLand,
                   MLor, MLnot, MEq, MLt, MGt, MLe, MGe, MProgn>();
}

template <auto expr> consteval auto forth_compile() {
    using namespace refmacro;
    constexpr auto result =
        type_check<TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap, TRFAdd, TRFSub,
                   TRFMul, TRFIf, TRFTimes>(expr);
    static_assert(result.valid, "forth_compile: type check failed");
    constexpr auto stripped = reftype::strip_types(expr);
    constexpr auto rewritten = rewrite_forth(stripped);
    return compile<rewritten, MFNew, MFPush, MFDup, MFDrop, MFSwap, MFAdd,
                   MFSub, MFMul, MAdd, MSub, MMul, MDiv, MNeg, MCond, MLand,
                   MLor, MLnot, MEq, MLt, MGt, MLe, MGe, MProgn>();
}

// ===================================================================
// Demo 1: Basic Forth — 5 3 + DUP *
// ===================================================================
// push 5, push 3, add, dup, mul → depth 1

static constexpr auto prog1 =
    f_mul(f_dup(f_add(f_push(3, f_push(5, f_new())))));
constexpr auto fn1 = forth_compile<prog1>();
static_assert(fn1() == 1); // depth: 0→1→2→1→2→1

// ===================================================================
// Demo 2: Balanced branch — both arms push 1 element
// ===================================================================
// push 5, push 3, push <cond>, IF push(10) ELSE push(20) THEN → depth 3

static constexpr auto prog2 =
    f_if(f_push(1, f_push(3, f_push(5, f_new()))),            // depth 3
         refmacro::lambda<128>("d", f_push(10, E::var("d"))), // then: +1
         refmacro::lambda<128>("d", f_push(20, E::var("d")))  // else: +1
    );
constexpr auto fn2 = forth_compile<prog2>();
static_assert(fn2() == 3); // depth: 0→1→2→3→pop→2→push→3

// ===================================================================
// Demo 3: Unbalanced branch — range type
// ===================================================================
// IF push(10) ELSE push(20), push(30) THEN → range {2..3}

static constexpr auto prog3 = f_if(
    f_push(1, f_push(5, f_new())),                                  // depth 2
    refmacro::lambda<128>("d", f_push(10, E::var("d"))),            // then: d+1
    refmacro::lambda<128>("d", f_push(30, f_push(20, E::var("d")))) // else: d+2
);

// Type check to verify the range
constexpr auto r3 = type_check<TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
                               TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(prog3);
static_assert(r3.valid);
// Result type should be {#v >= 2 && #v <= 3}
static_assert(is_subtype(r3.type, depth_range(2, 3)));

// ===================================================================
// Demo 4: Range propagation — add after unbalanced IF
// ===================================================================
// After prog3 (range {2..3}), add is safe because 2 >= 2

static constexpr auto prog4 = f_add(
    f_if(f_push(1, f_push(5, f_new())),
         refmacro::lambda<128>("d", f_push(10, E::var("d"))),
         refmacro::lambda<128>("d", f_push(30, f_push(20, E::var("d"))))));

constexpr auto r4 = type_check<TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
                               TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(prog4);
static_assert(r4.valid); // FM proves {2..3} >= 2

// ===================================================================
// Demo 5: Loop — 3 TIMES { DUP ADD } (net zero)
// ===================================================================
// push 5, then loop 3x: dup (+1) then add (-1) = net 0

static constexpr auto prog5 =
    f_times(3, refmacro::lambda<128>("d", f_add(f_dup(E::var("d")))),
            f_push(5, f_new()));
constexpr auto fn5 = forth_compile<prog5>();
static_assert(fn5() == 1); // depth stays at 1

// ===================================================================
// Demo 6: Error — stack underflow (uncomment to see compile error)
// ===================================================================
//   static constexpr auto err1 = f_drop(f_new());
//   constexpr auto err1_r = type_check<
//       TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
//       TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(err1);
//   static_assert(err1_r.valid); // fails: drop requires depth >= 1

// ===================================================================
// Demo 7: Error — range too wide for double-add (uncomment)
// ===================================================================
//   // After unbalanced IF: {1..2}. Second add needs >= 2 but range includes 1.
//   static constexpr auto err2 = f_add(f_add(f_if(
//       f_push(1, f_push(5, f_new())),
//       refmacro::lambda<128>("d", f_push(10, E::var("d"))),
//       refmacro::lambda<128>("d", f_push(30, f_push(20, E::var("d"))))
//   )));
//   constexpr auto err2_r = type_check<
//       TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
//       TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(err2);
//   static_assert(err2_r.valid); // fails: {1..2} </: {#v >= 2}

// ===================================================================
// Demo 8: Error — non-zero loop body (uncomment)
// ===================================================================
//   static constexpr auto err3 = f_times(
//       3,
//       refmacro::lambda<128>("d", f_push(5, E::var("d"))),  // net +1
//       f_push(5, f_new())
//   );
//   constexpr auto err3_r = type_check<
//       TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
//       TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(err3);
//   static_assert(err3_r.valid); // fails: body {2} != input {1}

int main() {
    std::printf("Demo 1 (5 3 + DUP *):      depth %d\n",
                static_cast<int>(fn1()));
    std::printf("Demo 2 (balanced IF):       depth %d\n",
                static_cast<int>(fn2()));
    std::printf("Demo 3 (unbalanced IF):     range [2..3] verified\n");
    std::printf("Demo 4 (add after range):   FM proved {2..3} >= 2\n");
    std::printf("Demo 5 (3x DUP ADD loop):   depth %d\n",
                static_cast<int>(fn5()));
    std::printf("Demos 6-8: uncomment to see compile-time errors\n");
    std::printf("All Forth DSL examples passed!\n");
    return 0;
}
