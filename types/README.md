# reftype — Compile-Time Refinement Types

A compile-time refinement type system built on [refmacro](../README.md). Types are AST nodes, type rules are macros, refinement predicates are validated by a Fourier-Motzkin elimination solver. Everything runs at `consteval` time — type errors are compile errors.

## Quick Start

### Type annotation

```cpp
#include <reftype/refinement.hpp>

using refmacro::Expression;
using reftype::ann, reftype::pos_int, reftype::TInt, reftype::typed_full_compile;
using E = Expression<128>;

// ann(expr, type) annotates an expression with a type.
// typed_full_compile checks types, strips annotations, and compiles.
static constexpr auto expr = ann(E::lit(3) + E::lit(4), TInt);
constexpr auto fn = typed_full_compile<expr>();
static_assert(fn() == 7);
```

### Refinement types

```cpp
// pos_int() = {#v : Int | #v > 0}
// The solver proves that 5 satisfies #v > 0 at compile time.
static constexpr auto refined = ann(E::lit(5), pos_int());
constexpr auto rf = typed_full_compile<refined>();
static_assert(rf() == 5);
```

### Compile-time type errors

```cpp
// lit(0) does not satisfy #v > 0 — this is a compile error:
// static constexpr auto bad = ann(E::lit(0), pos_int());
// constexpr auto bad_fn = typed_full_compile<bad>();
```

## Type Constructors

All constructors are `consteval` and return `Expression<Cap>` (default `Cap = 128`).

| Constructor | Description | Example |
|-------------|-------------|---------|
| `tint()` | Integer base type | `TInt` (convenience constant) |
| `tbool()` | Boolean base type | `TBool` |
| `treal()` | Real base type | `TReal` |
| `tref(base, pred)` | Refinement type `{#v : base \| pred}` | `tref(tint(), var("#v") > lit(0))` |
| `tarr(param, in, out)` | Arrow type `(param : In) -> Out` | `tarr("x", TInt, TInt)` |
| `ann(expr, type)` | Type annotation | `ann(lit(5), TInt)` |
| `pos_int()` | Convenience: `{#v : Int \| #v > 0}` | `ann(lit(5), pos_int())` |

Note: Convenience constants `TInt`, `TBool`, `TReal` are `inline constexpr` for the default `Cap = 128`.

## Pipeline

The typed compile pipeline runs three stages:

```
type_check(expr)  →  strip_types(expr)  →  compile<stripped, macros...>()
```

1. `type_check(expr)` — Bidirectional type synthesis. Walks the AST, dispatches to type rules by tag. Returns `TypeResult{type, valid}`. Compile-time errors on type mismatches.
2. `strip_types(expr)` — Removes `ann(expr, type)` nodes, leaving a plain expression tree ready for compilation. Bare type nodes outside annotations are compile errors.
3. `compile<stripped, macros...>()` — The standard refmacro compiler.

### Convenience wrappers

```cpp
// typed_compile: type check + strip + compile with explicit macros
constexpr auto fn = typed_compile<expr, MAdd, MSub, MMul>();

// typed_full_compile: includes all math + control-flow macros
constexpr auto fn = typed_full_compile<expr>();
```

Both accept an optional `TypeEnv` for pre-bound variables:

```cpp
constexpr auto env = reftype::TypeEnv<128>{}.bind("x", TInt);
constexpr auto fn = typed_full_compile<expr, env>();
```

## Extensibility: def_typerule

Define custom type rules with `def_typerule(tag, fn)`, parallel to `defmacro(tag, fn)`:

```cpp
#include <reftype/typerule.hpp>
#include <reftype/check.hpp>

// Custom "abs" type rule: numeric in, numeric out
inline constexpr auto TRAbs = reftype::def_typerule(
    "abs", [](const auto& expr, const auto& env, auto synth_rec) {
        using E = std::remove_cvref_t<decltype(expr)>;
        const auto& node = expr.ast.nodes[expr.id];
        auto child = synth_rec(E{expr.ast, node.children[0]}, env);
        // abs preserves the numeric type of its argument
        return child;
    });

// Use with type_check by passing extra rules:
constexpr auto result = reftype::type_check<TRAbs>(my_expr);
```

The type checker includes 18 built-in rules for arithmetic, comparisons, logical operators, conditionals, application, lambda, sequencing, and type annotations. Extra rules passed to `type_check<ExtraRules...>()` extend the set.

## Subtype Checking

`is_subtype(sub, super)` checks structural subtyping at compile time:

| Rule | Example |
|------|---------|
| **Reflexivity** | `Int <: Int` |
| **Base widening** | `Bool <: Int <: Real` (transitive) |
| **Refined <: unrefined** | `{#v:Int \| #v>0} <: Int` |
| **Refined <: refined** | `{#v:Int \| #v>0} <: {#v:Int \| #v>=0}` (P => Q via FM solver) |
| **Arrow contravariance** | `(Int -> Int) <: (PosInt -> Int)` |

`join(t1, t2)` computes the least upper bound (used for conditional branches):
- Same base types: identity
- Different bases: widen (`Int, Real → Real`)
- Refined types: disjunction of predicates

Refinement implication (`P => Q`) is decided by a **Fourier-Motzkin elimination** solver with integer rounding and DNF disjunction support.

## Architecture

```text
types/include/reftype/
├── types.hpp            Type constructors (TInt, TBool, TReal, tref, tarr, ann)
├── pretty.hpp           Type-aware pretty printing
├── type_env.hpp         Immutable TypeEnv (bind, lookup, shadowing)
├── constraints.hpp      Constraint / ConstraintSet
├── typerule.hpp         TypeRule, def_typerule()
├── check.hpp            Bidirectional type checker, built-in rules, type_check()
├── subtype.hpp          is_subtype(), join(), base widening
├── strip.hpp            strip_types(), typed_compile(), typed_full_compile()
├── refinement.hpp       Umbrella include
└── fm/                  Fourier-Motzkin solver
    ├── types.hpp        LinearInequality, InequalitySystem, VarInfo
    ├── parser.hpp       Expression → inequality parser with DNF
    ├── eliminate.hpp    Variable elimination (FM core algorithm)
    ├── rounding.hpp     Integer ceil/floor tightening
    ├── solver.hpp       is_unsat(), is_valid_implication(), is_valid()
    ├── disjunction.hpp  DNF clause splitting, clause_implies()
    └── fm.hpp           FM umbrella include
```

Note: The type system is a pure add-on — zero modifications to core refmacro headers. It depends on `refmacro::refmacro` as an INTERFACE library.

## Building

The type system builds automatically with the main project:

```bash
cmake -B build --toolchain cmake/xg++-toolchain.cmake \
      -DGCC_BUILD_DIR=/path/to/gcc/build/gcc
cmake --build build
ctest --test-dir build --output-on-failure
```

To disable: `-DREFMACRO_BUILD_TYPES=OFF`.

The example program can be built and run directly:

```bash
cmake --build build --target 01_basic_refinement
./build/types/examples/01_basic_refinement
```
