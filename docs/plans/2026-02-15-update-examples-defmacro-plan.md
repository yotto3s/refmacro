# Update Refinement Type Examples to New defmacro API — Implementation Plan

**Goal:** Rebase `feat/add-refinement-type-examples` onto `main` and update all examples to use the new `defmacro<"tag">` syntax with auto-tracked macros.

**Architecture:** The new `defmacro<"tag">(lambda)` API uses NTTP tag strings and `MacroCaller` to auto-track macros in `Expression<Cap, auto... Macros>`. `compile<e>()` auto-discovers macros from the expression type. `reftype::detail::compile_with_macros_from<stripped>(original_expr)` recovers macros after `strip_types`.

**Tech Stack:** C++26, GCC 16+, `-freflection`, refmacro header-only library

**Strategy:** Subagent-driven

---

### Task 1: Rebase onto main

**Files:**
- No file changes — git operation only

**Agent role:** junior-engineer

**Step 1: Rebase**

```bash
git rebase main
```

Expected: Clean rebase (branch only adds files under `types/examples/`; no overlap with main's changes to headers).

**Step 2: Verify rebase succeeded**

```bash
git log --oneline -5
```

Expected: Branch commits appear on top of `fdd65b9 feat: auto-tracking defmacro...`.

**Step 3: Build to confirm baseline**

```bash
cmake -B build --toolchain cmake/xg++-toolchain.cmake -DGCC_BUILD_DIR=/home/yotto/dev/gcc/build/gcc && cmake --build build 2>&1 | tail -20
```

Expected: Build fails on the example files because they use old `defmacro("tag", fn)` syntax (which no longer exists). This confirms the rebase landed and the old API is gone. Do NOT try to fix — that's Tasks 2-4.

**Step 4: Run existing tests (excluding new examples)**

```bash
ctest --test-dir build --output-on-failure -E "02_builtin|03_custom|04_forth"
```

Expected: All non-example tests pass. This confirms the rebase didn't break existing code.

---

### Task 2: Update `03_custom_type_rules.cpp`

**Files:**
- Modify: `types/examples/03_custom_type_rules.cpp`

**Agent role:** junior-engineer

**Step 1: Update defmacro syntax**

Change line 49 from:
```cpp
inline constexpr auto MClamp = defmacro("clamp", [](auto x, auto lo, auto hi) {
```
to:
```cpp
inline constexpr auto MClamp = defmacro<"clamp">([](auto x, auto lo, auto hi) {
```

**Step 2: Remove `clamp()` helper and use `MClamp()` directly**

Delete lines 40-45 (the `clamp()` template function):
```cpp
// AST constructor: builds a 3-child "clamp" node
template <std::size_t Cap = 128>
consteval Expression<Cap> clamp(Expression<Cap> x, Expression<Cap> lo,
                                Expression<Cap> hi) {
    return make_node("clamp", x, lo, hi);
}
```

Replace all `clamp(...)` calls with `MClamp(...)`:
- Line 125: `clamp(E::var("x"), E::lit(0), E::lit(100))` → `MClamp(E::var("x"), E::lit(0), E::lit(100))`
- Line 149: `clamp(E::var("x"), E::lit(1), E::lit(5))` → `MClamp(E::var("x"), E::lit(1), E::lit(5))`
- Line 166 (commented): `clamp(E::var("x"), E::lit(0), E::lit(100))` → `MClamp(E::var("x"), E::lit(0), E::lit(100))`
- Line 179: `clamp(E::var("x"), E::var("lo"), E::var("hi"))` → `MClamp(E::var("x"), E::var("lo"), E::var("hi"))`
- Line 199: `clamp(E::var("x"), E::var("lo") + E::lit(1), E::var("hi") - E::lit(1))` → `MClamp(E::var("x"), E::var("lo") + E::lit(1), E::var("hi") - E::lit(1))`

**Step 3: Simplify `clamp_compile()` to use `compile_with_macros_from`**

Change lines 113-120 from:
```cpp
template <auto expr, auto env> consteval auto clamp_compile() {
    using namespace refmacro;
    constexpr auto result = type_check<TRClamp>(expr, env);
    static_assert(result.valid, "clamp_compile: type check failed");
    constexpr auto stripped = strip_types(expr);
    return compile<stripped, MClamp, MAdd, MSub, MMul, MDiv, MNeg, MCond, MLand,
                   MLor, MLnot, MEq, MLt, MGt, MLe, MGe, MProgn>();
}
```
to:
```cpp
template <auto expr, auto env> consteval auto clamp_compile() {
    constexpr auto result = type_check<TRClamp>(expr, env);
    static_assert(result.valid, "clamp_compile: type check failed");
    constexpr auto stripped = strip_types(expr);
    return reftype::detail::compile_with_macros_from<stripped>(expr);
}
```

**Step 4: Clean up imports**

Remove these lines (no longer needed):
```cpp
using refmacro::compile;
using refmacro::defmacro;
using refmacro::make_node;
```

Keep `using refmacro::str_eq;` (used in type rule), `using refmacro::NodeView;`, `using refmacro::to_expr;`, and `using refmacro::Expression;`.

**Step 5: Build and test**

```bash
cmake --build build --target 03_custom_type_rules && ./build/types/examples/03_custom_type_rules
```

Expected output:
```
Section 3 (clamp 50):  50
Section 3 (clamp -5):  0
Section 3 (clamp 200): 100
Section 4 (subtype 3): 3
Section 4 (subtype 0): 1
Section 4 (subtype 7): 5
Section 6 (var 5,0,10):   5
Section 6 (var -1,0,10):  0
Section 6 (var 15,0,10):  10
Section 7 (expr 5,0,10):  5
Section 7 (expr 0,0,10):  1
Section 7 (expr 10,0,10): 9
All custom type rule examples passed!
```

**Step 6: Commit**

```bash
git add types/examples/03_custom_type_rules.cpp
git commit -m "refactor: update 03_custom_type_rules to new defmacro<> syntax with auto-tracked macros"
```

---

### Task 3: Update `04_forth_dsl.cpp`

**Files:**
- Modify: `types/examples/04_forth_dsl.cpp`

**Agent role:** junior-engineer

**Step 1: Update all 8 defmacro calls**

Change each `refmacro::defmacro("tag", fn)` to `refmacro::defmacro<"tag">(fn)`:

Line 181-182:
```cpp
// OLD:
inline constexpr auto MFNew = refmacro::defmacro(
    "f_new", []() { return [](auto...) constexpr { return 0; }; });
// NEW:
inline constexpr auto MFNew = refmacro::defmacro<"f_new">(
    []() { return [](auto...) constexpr { return 0; }; });
```

Line 184-187:
```cpp
// OLD:
inline constexpr auto MFPush =
    refmacro::defmacro("f_push", [](auto /*n*/, auto s) {
        return [=](auto... a) constexpr { return s(a...) + 1; };
    });
// NEW:
inline constexpr auto MFPush =
    refmacro::defmacro<"f_push">([](auto /*n*/, auto s) {
        return [=](auto... a) constexpr { return s(a...) + 1; };
    });
```

Line 189-191:
```cpp
// OLD:
inline constexpr auto MFDup = refmacro::defmacro("f_dup", [](auto s) {
// NEW:
inline constexpr auto MFDup = refmacro::defmacro<"f_dup">([](auto s) {
```

Line 193-195:
```cpp
// OLD:
inline constexpr auto MFDrop = refmacro::defmacro("f_drop", [](auto s) {
// NEW:
inline constexpr auto MFDrop = refmacro::defmacro<"f_drop">([](auto s) {
```

Line 197-199:
```cpp
// OLD:
inline constexpr auto MFSwap = refmacro::defmacro("f_swap", [](auto s) {
// NEW:
inline constexpr auto MFSwap = refmacro::defmacro<"f_swap">([](auto s) {
```

Line 201-203:
```cpp
// OLD:
inline constexpr auto MFAdd = refmacro::defmacro("f_add", [](auto s) {
// NEW:
inline constexpr auto MFAdd = refmacro::defmacro<"f_add">([](auto s) {
```

Line 205-207:
```cpp
// OLD:
inline constexpr auto MFSub = refmacro::defmacro("f_sub", [](auto s) {
// NEW:
inline constexpr auto MFSub = refmacro::defmacro<"f_sub">([](auto s) {
```

Line 209-211:
```cpp
// OLD:
inline constexpr auto MFMul = refmacro::defmacro("f_mul", [](auto s) {
// NEW:
inline constexpr auto MFMul = refmacro::defmacro<"f_mul">([](auto s) {
```

**Step 2: Remove Forth AST helper functions, use `MFXxx()` directly**

Delete lines 120-173 (all `f_new`, `f_push`, `f_dup`, `f_drop`, `f_swap`, `f_add`, `f_sub`, `f_mul`, `f_if`, `f_times` helper functions).

Replace with two kept helpers (f_if and f_times still use make_node since they have no macros):
```cpp
// f_if and f_times use make_node (no compile macros — handled by rewrite)
template <std::size_t Cap = 128, auto... Ms1, auto... Ms2, auto... Ms3>
consteval auto f_if(Expression<Cap, Ms1...> s, Expression<Cap, Ms2...> then_fn,
                    Expression<Cap, Ms3...> else_fn) {
    return make_node<Cap>("f_if", s, then_fn, else_fn);
}

template <std::size_t Cap = 128, auto... Ms>
consteval auto f_times(int count, Expression<Cap, Ms...> body_fn,
                       Expression<Cap, Ms...> s) {
    return make_node("f_times", Expression<Cap>::lit(count), body_fn, s);
}
```

**Step 3: Update `FixedString` → `PrintBuffer`**

Line 263: change `refmacro::FixedString<32>` to `refmacro::PrintBuffer<32>`:
```cpp
// OLD:
                refmacro::FixedString<32> s{};
// NEW:
                refmacro::PrintBuffer<32> s{};
```

**Step 4: Update program construction to use `MFXxx()` directly**

Replace all `f_xxx(...)` calls with `MFXxx(...)` in demo programs. Note: `f_push(n, s)` becomes `MFPush(E::lit(n), s)` since MacroCaller takes Expression children.

Demo 1 (line 471-472):
```cpp
// OLD:
static constexpr auto prog1 =
    f_mul(f_dup(f_add(f_push(3, f_push(5, f_new())))));
// NEW:
static constexpr auto prog1 =
    MFMul(MFDup(MFAdd(MFPush(E::lit(3), MFPush(E::lit(5), MFNew<128>())))));
```

Demo 2 (lines 481-485):
```cpp
// OLD:
static constexpr auto prog2 =
    f_if(f_push(1, f_push(3, f_push(5, f_new()))),            // depth 3
         refmacro::lambda<128>("d", f_push(10, E::var("d"))), // then: +1
         refmacro::lambda<128>("d", f_push(20, E::var("d")))  // else: +1
    );
// NEW:
static constexpr auto prog2 =
    f_if(MFPush(E::lit(1), MFPush(E::lit(3), MFPush(E::lit(5), MFNew<128>()))),
         refmacro::lambda<128>("d", MFPush(E::lit(10), E::var("d"))),
         refmacro::lambda<128>("d", MFPush(E::lit(20), E::var("d"))));
```

Demo 3 (lines 494-498):
```cpp
// OLD:
static constexpr auto prog3 = f_if(
    f_push(1, f_push(5, f_new())),                                  // depth 2
    refmacro::lambda<128>("d", f_push(10, E::var("d"))),            // then: d+1
    refmacro::lambda<128>("d", f_push(30, f_push(20, E::var("d")))) // else: d+2
);
// NEW:
static constexpr auto prog3 = f_if(
    MFPush(E::lit(1), MFPush(E::lit(5), MFNew<128>())),
    refmacro::lambda<128>("d", MFPush(E::lit(10), E::var("d"))),
    refmacro::lambda<128>("d", MFPush(E::lit(30), MFPush(E::lit(20), E::var("d")))));
```

Demo 4 (lines 512-515):
```cpp
// OLD:
static constexpr auto prog4 = f_add(
    f_if(f_push(1, f_push(5, f_new())),
         refmacro::lambda<128>("d", f_push(10, E::var("d"))),
         refmacro::lambda<128>("d", f_push(30, f_push(20, E::var("d"))))));
// NEW:
static constexpr auto prog4 = MFAdd(
    f_if(MFPush(E::lit(1), MFPush(E::lit(5), MFNew<128>())),
         refmacro::lambda<128>("d", MFPush(E::lit(10), E::var("d"))),
         refmacro::lambda<128>("d", MFPush(E::lit(30), MFPush(E::lit(20), E::var("d"))))));
```

Demo 5 (lines 526-528):
```cpp
// OLD:
static constexpr auto prog5 =
    f_times(3, refmacro::lambda<128>("d", f_add(f_dup(E::var("d")))),
            f_push(5, f_new()));
// NEW:
static constexpr auto prog5 =
    f_times(3, refmacro::lambda<128>("d", MFAdd(MFDup(E::var("d")))),
            MFPush(E::lit(5), MFNew<128>()));
```

Commented error demos (lines 535-566): apply same pattern:
- `f_drop(f_new())` → `MFDrop(MFNew<128>())`
- `f_push(...)` → `MFPush(E::lit(...), ...)`
- `f_add(...)` → `MFAdd(...)`
- etc.

**Step 5: Build and test**

```bash
cmake --build build --target 04_forth_dsl && ./build/types/examples/04_forth_dsl
```

Expected output:
```
Demo 1 (5 3 + DUP *):      depth 1
Demo 2 (balanced IF):       depth 3
Demo 3 (unbalanced IF):     range [2..3] verified
Demo 4 (add after range):   FM proved {2..3} >= 2
Demo 5 (3x DUP ADD loop):   depth 1
Demos 6-8: uncomment to see compile-time errors
All Forth DSL examples passed!
```

**Step 6: Commit**

```bash
git add types/examples/04_forth_dsl.cpp
git commit -m "refactor: update 04_forth_dsl to new defmacro<> syntax with MacroCaller AST construction"
```

---

### Task 4: Verify `02_builtin_type_rules.cpp` and full test suite

**Files:**
- No modifications expected to `types/examples/02_builtin_type_rules.cpp`

**Agent role:** junior-engineer

**Step 1: Build and run 02_builtin_type_rules**

```bash
cmake --build build --target 02_builtin_type_rules && ./build/types/examples/02_builtin_type_rules
```

Expected output:
```
Section 1 (x+y):       7
Section 2 (widen):      5
Section 3 (cond t,5):   6
Section 3 (p && q):     1
Section 4 (apply):      6
Section 5 (range):      42
Section 5 (pos_int):    7
All built-in type rule examples passed!
```

If it fails to compile (e.g., `Expression<128>` vs `Expression<128, Ms...>` issues with `make_node<128>("cond", ...)`), fix the issue. The most likely needed change: `make_node<128>("cond", ...)` on line 68 may need the explicit `<128>` template arg since mixed-macro `make_node` deduction might differ. Investigate and fix if needed.

**Step 2: Run full test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: All tests pass.

**Step 3: Commit if changes were needed**

```bash
# Only if 02_builtin_type_rules.cpp required changes:
git add types/examples/02_builtin_type_rules.cpp
git commit -m "fix: update 02_builtin_type_rules for Expression<Cap, Ms...> compatibility"
```

---

## Execution: Subagent-Driven

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> to execute this plan task-by-task.

**Task Order:** Sequential, dependency-respecting order listed below.

1. Task 1: Rebase onto main — no dependencies
2. Task 2: Update 03_custom_type_rules.cpp — depends on Task 1
3. Task 3: Update 04_forth_dsl.cpp — depends on Task 1
4. Task 4: Verify 02_builtin_type_rules.cpp and full suite — depends on Tasks 1-3

Each task is self-contained with full context. Execute one at a time with
fresh subagent per task and two-stage review (spec compliance, then code
quality).
