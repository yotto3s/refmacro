// Built-in type rules — comprehensive tour
//
// Demonstrates: type environments, subtype lattice, conditionals, logic,
// arrow types, lambdas, refinement arithmetic, and compile-time error
// detection. Build: cmake --build build --target 02_builtin_type_rules Run:
// ./build/types/examples/02_builtin_type_rules

#include <cstdio>
#include <reftype/refinement.hpp>

using refmacro::Expression;
using refmacro::make_node;
using reftype::ann;
using reftype::is_subtype;
using reftype::pos_int;
using reftype::tarr;
using reftype::TBool;
using reftype::TInt;
using reftype::TReal;
using reftype::tref;
using reftype::type_check;
using reftype::typed_full_compile;
using reftype::TypeEnv;

using E = Expression<128>;

// ===================================================================
// Section 1: Variables + Type Environments
// ===================================================================
// Type environments map variable names to types. The compiled function
// takes arguments in the order variables appear in the AST (DFS order).

static constexpr auto env1 = TypeEnv<128>{}.bind("x", TInt).bind("y", TInt);
static constexpr auto vars_expr = ann(E::var("x") + E::var("y"), TInt);
constexpr auto vars_fn = typed_full_compile<vars_expr, env1>();
static_assert(vars_fn(3, 4) == 7); // x=3, y=4 → 7
static_assert(vars_fn(10, 20) == 30);

// ===================================================================
// Section 2: Subtype Lattice
// ===================================================================
// Bool <: Int <: Real (base widening).
// Refined types are subtypes of their base: {#v > 0} <: Int.

// Int literal annotated as Real — valid because Int <: Real
static constexpr auto widen_expr = ann(E::lit(5), TReal);
constexpr auto widen_fn = typed_full_compile<widen_expr>();
static_assert(widen_fn() == 5);

// Subtype checks (used internally by the type checker)
static_assert(is_subtype(TBool, TInt));  // Bool <: Int
static_assert(is_subtype(TInt, TReal));  // Int <: Real
static_assert(!is_subtype(TReal, TInt)); // Real </: Int

// Refined <: unrefined
static_assert(is_subtype(pos_int(), TInt));  // {#v > 0} <: Int
static_assert(!is_subtype(TInt, pos_int())); // Int </: {#v > 0}

// ===================================================================
// Section 3: Conditionals + Logic
// ===================================================================
// cond(test, then, else) requires Bool test, matching then/else types.
// Logical operators &&, ||, ! require Bool operands.

static constexpr auto cond_env =
    TypeEnv<128>{}.bind("p", TBool).bind("x", TInt);
static constexpr auto cond_expr =
    ann(make_node<128>("cond", E::var("p"), E::var("x") + E::lit(1),
                       E::var("x") - E::lit(1)),
        TInt);
constexpr auto cond_fn = typed_full_compile<cond_expr, cond_env>();
static_assert(cond_fn(true, 5) == 6);  // p=true  → x+1
static_assert(cond_fn(false, 5) == 4); // p=false → x-1

// Logical operators: Bool → Bool
static constexpr auto logic_env =
    TypeEnv<128>{}.bind("p", TBool).bind("q", TBool);
static constexpr auto and_expr = ann(E::var("p") && E::var("q"), TBool);
constexpr auto and_fn = typed_full_compile<and_expr, logic_env>();
static_assert(and_fn(true, true) == true);
static_assert(and_fn(true, false) == false);

static constexpr auto not_expr = ann(!E::var("p"), TBool);
static constexpr auto not_env = TypeEnv<128>{}.bind("p", TBool);
constexpr auto not_fn = typed_full_compile<not_expr, not_env>();
static_assert(not_fn(true) == false);

// ===================================================================
// Section 4: Arrow Types + Lambda
// ===================================================================
// Lambda with arrow type annotation: (x : Int) -> Int
// apply(lambda, val) uses let-binding pattern for type checking.

static constexpr auto inc_type = tarr("x", TInt, TInt);
static constexpr auto inc_expr =
    ann(refmacro::lambda<128>("x", E::var("x") + E::lit(1)), inc_type);
constexpr auto inc_result = type_check(inc_expr);
static_assert(inc_result.valid);

// Apply lambda to value
static constexpr auto app_expr =
    ann(refmacro::apply<128>(
            refmacro::lambda<128>("x", E::var("x") + E::lit(1)), E::lit(5)),
        TInt);
constexpr auto app_fn = typed_full_compile<app_expr>();
static_assert(app_fn() == 6);

// ===================================================================
// Section 5: Refinement Arithmetic — FM Solver in Action
// ===================================================================
// The Fourier-Motzkin solver proves implications between linear
// arithmetic predicates at compile time.

// Range type: {#v >= 0 && #v < 100}
static constexpr auto range_type =
    tref(TInt, E::var("#v") >= E::lit(0) && E::var("#v") < E::lit(100));

// lit(42) has singleton type {#v == 42}. FM proves: #v==42 => #v>=0 && #v<100
static constexpr auto range_expr = ann(E::lit(42), range_type);
constexpr auto range_fn = typed_full_compile<range_expr>();
static_assert(range_fn() == 42);

// Subtype implication: {1..5} <: {>0 && <10}
static constexpr auto narrow =
    tref(TInt, E::var("#v") >= E::lit(1) && E::var("#v") <= E::lit(5));
static constexpr auto wide =
    tref(TInt, E::var("#v") > E::lit(0) && E::var("#v") < E::lit(10));
static_assert(is_subtype(narrow, wide));

// Positive integer annotation
static constexpr auto pos_expr = ann(E::lit(7), pos_int());
constexpr auto pos_fn = typed_full_compile<pos_expr>();
static_assert(pos_fn() == 7);

// ===================================================================
// Section 6: Compile-Time Error Detection
// ===================================================================
// Uncomment any of these to see compile-time type errors:

// Type mismatch: Int literal annotated as Bool
//   static constexpr auto err1 = ann(E::lit(5), TBool);
//   constexpr auto err1_r = type_check(err1);
//   static_assert(err1_r.valid); // fails: Int is not Bool

// Refinement violation: 0 does not satisfy #v > 0
//   static constexpr auto err2 = ann(E::lit(0), pos_int());
//   constexpr auto err2_fn = typed_full_compile<err2>(); // compile error

// Narrowing failure: {>0} is not a subtype of {>5}
//   static_assert(is_subtype(
//       tref(TInt, E::var("#v") > E::lit(0)),
//       tref(TInt, E::var("#v") > E::lit(5))));  // fails

int main() {
    std::printf("Section 1 (x+y):       %d\n", static_cast<int>(vars_fn(3, 4)));
    std::printf("Section 2 (widen):      %d\n", static_cast<int>(widen_fn()));
    std::printf("Section 3 (cond t,5):   %d\n",
                static_cast<int>(cond_fn(true, 5)));
    std::printf("Section 3 (p && q):     %d\n",
                static_cast<int>(and_fn(true, true)));
    std::printf("Section 4 (apply):      %d\n", static_cast<int>(app_fn()));
    std::printf("Section 5 (range):      %d\n", static_cast<int>(range_fn()));
    std::printf("Section 5 (pos_int):    %d\n", static_cast<int>(pos_fn()));
    std::printf("All built-in type rule examples passed!\n");
    return 0;
}
