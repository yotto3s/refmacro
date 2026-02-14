// Basic refinement type system example
//
// Demonstrates: type annotations, refinement types, the typed compile pipeline.
// Build: cmake --build build --target 01_basic_refinement
// Run:   ./build/types/examples/01_basic_refinement

#include <cstdio>
#include <reftype/refinement.hpp>

using refmacro::Expression;
using reftype::ann;
using reftype::pos_int;
using reftype::TInt;
using reftype::typed_full_compile;

using E = Expression<128>;

// --- Example 1: Simple arithmetic with type annotation ---
// Expression: ann(lit(3) + lit(4), Int)
// Type checks that 3+4 produces an Int, then compiles to a function.
static constexpr auto arith_expr = ann(E::lit(3) + E::lit(4), TInt);
constexpr auto arith_fn = typed_full_compile<arith_expr>();
static_assert(arith_fn() == 7);

// --- Example 2: Refinement type annotation ---
// Expression: ann(lit(5), {#v : Int | #v > 0})
// Verifies at compile time that 5 satisfies the refinement predicate #v > 0.
static constexpr auto refined_expr = ann(E::lit(5), pos_int());
constexpr auto refined_fn = typed_full_compile<refined_expr>();
static_assert(refined_fn() == 5);

// --- Example 3: Let binding with typed compile ---
// Expression: let x = 10 in x + x → 20
static constexpr auto let_expr =
    ann(refmacro::let_<128>("x", E::lit(10), E::var("x") + E::var("x")), TInt);
constexpr auto let_fn = typed_full_compile<let_expr>();
static_assert(let_fn() == 20);

// --- Example 4: Compile-time type error detection ---
// Uncomment to see a compile-time error:
//   static constexpr auto bad = ann(E::lit(0), pos_int());
//   constexpr auto bad_fn = typed_full_compile<bad>();
// Error: 0 does not satisfy #v > 0

int main() {
    std::printf("Example 1 (3 + 4):     %d\n", static_cast<int>(arith_fn()));
    std::printf("Example 2 (lit(5)):    %d\n", static_cast<int>(refined_fn()));
    std::printf("Example 3 (let x=10): %d\n", static_cast<int>(let_fn()));
    std::printf("All examples passed!\n");
    return 0;
}
