# Refinement Type Examples Design

Three example programs demonstrating the refinement type system at progressive complexity.

## Example 1: `02_builtin_type_rules.cpp`

Comprehensive tour of built-in type rules. Basic/approachable.

### Sections

1. **Variables + Type Environments** — `TypeEnv` bindings, multi-variable `ann(x + y, TInt)` with env, compiled function taking arguments
2. **Subtype lattice** — `ann(lit(5), TReal)` (Int <: Real), refined-to-unrefined subtyping
3. **Conditionals + Logic** — `cond(p, x, y)` typed as TInt, boolean `&&`/`||`/`!` typed as TBool
4. **Arrow types + Lambda** — `ann(lambda("x", x+1), tarr("x", TInt, TInt))`, apply(lambda, val)
5. **Refinement arithmetic** — Range types `{#v >= 0 && #v < 100}`, implication proof `{#v >= 1 && #v <= 5} <: {#v > 0 && #v < 10}`, FM solver in action
6. **Error detection** — Commented-out examples: type mismatch, refinement violation

Style matches existing `01_basic_refinement.cpp`: comment headers, `static_assert` + `printf` in main.

---

## Example 2: `03_custom_type_rules.cpp`

Basic custom type rules with a `clamp` operation. Basic/approachable.

### Custom macro: `clamp(x, lo, hi)`

- **Runtime lowering**: `cond(x < lo, lo, cond(x > hi, hi, x))`
- **Type rule**: Synthesizes `{#v : Int | #v >= lo && #v <= hi}` where lo/hi are extracted from literal children

### Sections

1. **Defining the custom macro** — `defmacro("clamp", ...)` with conditional lowering
2. **Defining the custom type rule** — `def_typerule("clamp", ...)` with refinement synthesis
3. **Using the custom rule** — `ann(clamp(x, 0, 100), {#v >= 0 && #v <= 100})` type-checks and compiles
4. **Refinement propagation** — `ann(clamp(x, 1, 10) + lit(5), {#v >= 6 && #v <= 15})` — FM solver proves bounds propagate
5. **Error detection** — Commented-out: `ann(clamp(x, 0, 100), {#v >= 0 && #v <= 50})` fails

---

## Example 3: `04_forth_dsl.cpp`

Compile-time verified Forth subset. Impressive showcase.

A Forth-like stack machine where programs are AST expressions, stack depth is tracked through refinement types, and the FM solver proves at compile time that no stack underflow occurs, branches produce valid ranges, and loop bodies have net-zero effect.

### Helpers

- `depth(N)` = `tref(TInt, #v == lit(N))` — singleton type for exact depth
- `depth_range(lo, hi)` = `tref(TInt, #v >= lit(lo) && #v <= lit(hi))` — range type
- `extract_bounds(type)` → `Bounds{lo, hi}` — extracts bounds from singleton or range predicates

### Stack operations (8 macros + 8 type rules)

| Operation | Macro lowering | Type: input `{lo..hi}` | Guard (FM) | Type: result |
|-----------|---------------|------------------------|------------|--------------|
| `f_new()` | `lit(0)` | — | — | `{#v == 0}` |
| `f_push(N, s)` | `s + 1` | `{lo..hi}` | — | `{lo+1..hi+1}` |
| `f_dup(s)` | `s + 1` | `{lo..hi}` | `lo >= 1` | `{lo+1..hi+1}` |
| `f_drop(s)` | `s - 1` | `{lo..hi}` | `lo >= 1` | `{lo-1..hi-1}` |
| `f_swap(s)` | `s` | `{lo..hi}` | `lo >= 2` | `{lo..hi}` |
| `f_add(s)` | `s - 1` | `{lo..hi}` | `lo >= 2` | `{lo-1..hi-1}` |
| `f_sub(s)` | `s - 1` | `{lo..hi}` | `lo >= 2` | `{lo-1..hi-1}` |
| `f_mul(s)` | `s - 1` | `{lo..hi}` | `lo >= 2` | `{lo-1..hi-1}` |

### Control flow

**`f_if(stack, then_fn, else_fn)`** — Pops condition. Both branches are lambdas:

```cpp
f_if(stack,
    lambda("d", f_push(10, E::var("d"))),            // then: d → d+1
    lambda("d", f_push(20, f_push(30, E::var("d")))) // else: d → d+2
)
```

Type rule:
1. Stack → `{lo..hi}`, prove `lo >= 1` via `is_subtype` + FM solver
2. Bind lambda parameter to `{lo-1..hi-1}` in type env
3. Synth then body → `{lo1..hi1}`, else body → `{lo2..hi2}`
4. Result: `{min(lo1,lo2)..max(hi1,hi2)}`

If both branches produce the same depth: result is singleton (exact).
If different: result is a range. Subsequent operations must satisfy preconditions for ALL values in the range.

**`f_times(count, body_fn, stack)`** — Repeats body. Body must have net-zero effect:

```cpp
f_times(3,
    lambda("d", f_add(f_dup(E::var("d")))), // body: dup, add (net 0)
    f_push(5, f_new())                       // stack: depth 1
)
```

Type rule:
1. Stack → `{lo..hi}`
2. Bind lambda parameter to `{lo..hi}` in type env
3. Synth body → `{lo2..hi2}`
4. Prove lo2 == lo AND hi2 == hi (net-zero effect for all depths in range)
5. Result: `{lo..hi}`

### Demo programs

1. **Basic Forth**: `5 3 + DUP *` → `{#v == 1}`
2. **Balanced branch**: both arms +1 → exact `{#v == 3}`
3. **Unbalanced branch**: then +1, else +2 → range `{#v >= 2 && #v <= 3}`
4. **Range propagation**: `f_add` after unbalanced IF → FM proves `{2..3} <: {#v >= 2}` ✓
5. **Loop**: `3 TIMES { DUP ADD }` → body net-zero proved
6. **Error: underflow**: `f_drop(f_new())` → FM proves `{#v == 0}` NOT `<: {#v >= 1}`
7. **Error: range too wide**: double-add after unbalanced IF → `{1..2}` NOT `<: {#v >= 2}`
8. **Error: unbalanced loop**: body `{ PUSH(5) }` has net +1, not 0

### Key message

The compiler becomes a proof checker for stack machine programs. Custom type rules + FM solver + range types = user-defined compile-time proofs with imprecision handling. Impossible in standard C++.

---

## Build integration

Add all 3 examples to `types/examples/CMakeLists.txt`:

```cmake
add_executable(02_builtin_type_rules 02_builtin_type_rules.cpp)
target_link_libraries(02_builtin_type_rules PRIVATE reftype::reftype)
target_compile_options(02_builtin_type_rules PRIVATE -Wall -Wextra -Werror)

add_executable(03_custom_type_rules 03_custom_type_rules.cpp)
target_link_libraries(03_custom_type_rules PRIVATE reftype::reftype)
target_compile_options(03_custom_type_rules PRIVATE -Wall -Wextra -Werror)

add_executable(04_forth_dsl 04_forth_dsl.cpp)
target_link_libraries(04_forth_dsl PRIVATE reftype::reftype)
target_compile_options(04_forth_dsl PRIVATE -Wall -Wextra -Werror)
```
