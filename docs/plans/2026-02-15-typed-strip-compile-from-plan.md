# typed_strip + compile_from Public API Implementation Plan

**Goal:** Add `compile_from` and `typed_strip` as public functions in `strip.hpp`, then simplify `04_forth_dsl.cpp` by extracting repeated type rule lists into local helpers.

**Architecture:** `compile_from` is a public wrapper around the existing `detail::compile_with_macros_from`. `typed_strip` combines type_check + static_assert + strip_types into one call (like `typed_compile` but stops before compile). The Forth example simplification uses local `forth_type_check` helpers to DRY the 10-rule type rule list that currently appears 7 times.

**Tech Stack:** C++26 consteval, concepts, GCC 16+

**Strategy:** Subagent-driven

---

### Task 1: Add `compile_from` and `typed_strip` to `strip.hpp`

**Files:**
- Modify: `types/include/reftype/strip.hpp`

**Agent role:** senior-engineer

**Context:** `strip.hpp` already has `detail::compile_with_macros_from` and three `typed_compile` overloads with concept-based disambiguation (`detail::is_macro`, `detail::is_type_rule`). The new functions follow the same patterns.

**Step 1: Add `compile_from` after the `detail` namespace block**

After the closing `} // namespace detail` (currently line 87), add:

```cpp
// --- compile_from: compile with macros recovered from original expression ---
//
// Compiles `stripped` (an Expression<Cap> with no auto-tracked macros),
// recovering macros from the type of `original_expr`. Extra macros can
// be passed explicitly as leading template arguments.
//
// Use case: custom compile pipelines with intermediate transformations
// (e.g., rewrite passes) between strip_types and compile.
//
// Usage:
//   constexpr auto stripped = strip_types(expr);
//   constexpr auto rewritten = my_rewrite(stripped);
//   constexpr auto fn = compile_from<rewritten>(expr);

template <auto stripped, auto... ExtraMacros, std::size_t Cap, auto... Embedded>
consteval auto compile_from(Expression<Cap, Embedded...>) {
    return refmacro::compile<stripped, Embedded..., ExtraMacros...>();
}
```

**Step 2: Add `typed_strip` overloads before the `typed_full_compile` section**

Place these after the last `typed_compile` overload (after the ExtraRules overload, currently ending at line 138). Add before the `typed_full_compile` comment block:

```cpp
// --- typed_strip: type check + strip in one step (no compile) ---
//
// Like typed_compile but stops after strip_types, returning the stripped
// Expression<Cap>. Useful when a custom transformation (e.g., rewrite pass)
// must run between strip and compile.
//
// Usage:
//   constexpr auto stripped = typed_strip<expr>();
//   constexpr auto stripped = typed_strip<expr, env>();
//   constexpr auto stripped = typed_strip<expr, env, TRCustom>();

template <auto expr>
consteval auto typed_strip() {
    constexpr auto result = type_check(expr);
    static_assert(result.valid, "typed_strip: type check failed");
    return strip_types(expr);
}

template <auto expr, auto env>
    requires requires {
        env.count;
        env.lookup("");
    }
consteval auto typed_strip() {
    constexpr auto result = type_check(expr, env);
    static_assert(result.valid, "typed_strip: type check failed");
    return strip_types(expr);
}

template <auto expr, auto env, auto... ExtraRules>
    requires(requires {
        env.count;
        env.lookup("");
    } && sizeof...(ExtraRules) > 0 && (... && detail::is_type_rule<ExtraRules>))
consteval auto typed_strip() {
    constexpr auto result = type_check<ExtraRules...>(expr, env);
    static_assert(result.valid, "typed_strip: type check failed");
    return strip_types(expr);
}
```

**Step 3: Build and verify**

Run: `cmake --build build 2>&1 | tail -5`
Expected: clean build (no errors from strip.hpp changes)

**Step 4: Run tests**

Run: `ctest --test-dir build --output-on-failure 2>&1 | tail -5`
Expected: `100% tests passed, 0 tests failed out of 558`

**Step 5: Commit**

```bash
git add types/include/reftype/strip.hpp
git commit -m "feat: add compile_from and typed_strip public API"
```

---

### Task 2: Add tests for `compile_from` and `typed_strip`

**Files:**
- Modify: `types/tests/test_strip.cpp`

**Agent role:** junior-engineer

**Context:** `test_strip.cpp` already tests `strip_types` and `typed_compile`. Add parallel tests for the new functions after the existing `typed_compile` tests (after the `Pipeline` section).

**Step 1: Add `compile_from` tests at the end of `test_strip.cpp`**

Before the file's closing, add:

```cpp
// ============================================================
// compile_from: compile with macros recovered from original
// ============================================================

TEST(CompileFrom, RecoversAutoTrackedMacros) {
    // Expression with auto-tracked MAdd
    static constexpr auto original = E::lit(2) + E::lit(3);
    // Strip macros (simulates strip_types output)
    static constexpr auto stripped = strip_types(ann(original, TInt));
    // compile_from recovers MAdd from original's type
    constexpr auto f = reftype::compile_from<stripped>(original);
    static_assert(f() == 5);
}

TEST(CompileFrom, WithExplicitExtraMacros) {
    // Expression with auto-tracked MAdd, compile with explicit MMul too
    static constexpr auto original = E::lit(2) + E::lit(3);
    static constexpr auto stripped = strip_types(ann(original, TInt));
    constexpr auto f =
        reftype::compile_from<stripped, refmacro::MMul>(original);
    static_assert(f() == 5);
}
```

**Step 2: Add `typed_strip` tests**

```cpp
// ============================================================
// typed_strip: type check + strip (no compile)
// ============================================================

TEST(TypedStrip, BasicLiteral) {
    // ann(lit(5), TInt) → stripped to lit(5)
    static constexpr auto e = ann(E::lit(5), TInt);
    static constexpr auto stripped = reftype::typed_strip<e>();
    static constexpr auto expected = E::lit(5);
    static_assert(types_equal(stripped, expected));
}

TEST(TypedStrip, WithEnvironment) {
    // ann(var("x") + lit(1), TInt) with env → stripped to var("x") + lit(1)
    static constexpr auto e = ann(E::var("x") + E::lit(1), TInt);
    static constexpr auto env = reftype::TypeEnv<128>{}.bind("x", TInt);
    static constexpr auto stripped = reftype::typed_strip<e, env>();
    static constexpr auto expected = E::var("x") + E::lit(1);
    static_assert(types_equal(stripped, expected));
}

TEST(TypedStrip, NestedAnnotations) {
    // ann(ann(lit(5), TInt) + lit(1), TInt) → lit(5) + lit(1)
    static constexpr auto inner = ann(E::lit(5), TInt);
    static constexpr auto e = ann(inner + E::lit(1), TInt);
    static constexpr auto stripped = reftype::typed_strip<e>();
    static constexpr auto expected = E::lit(5) + E::lit(1);
    static_assert(types_equal(stripped, expected));
}
```

**Step 3: Build and run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure 2>&1 | tail -10`
Expected: all tests pass (558 + 5 new = 563)

**Step 4: Commit**

```bash
git add types/tests/test_strip.cpp
git commit -m "test: add tests for compile_from and typed_strip"
```

---

### Task 3: Simplify `04_forth_dsl.cpp` with `forth_type_check` helpers

**Files:**
- Modify: `types/examples/04_forth_dsl.cpp`

**Agent role:** senior-engineer

**Context:** The 10-rule type rule list `TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap, TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes` currently appears 7 times in the file: twice in `forth_compile`, and five times in demos (3, 4, 6, 7, 8). Extract into local helpers. The compile macro list stays explicit (cannot be eliminated because `f_if`/`f_times` use `make_node` which drops macro tracking).

**Step 1: Add `forth_type_check` helpers before `forth_compile`**

Insert after `rewrite_forth` (before the `// Forth compile pipeline` comment, currently line 389):

```cpp
// ===================================================================
// Forth type-check helper — wraps the 10 type rules
// ===================================================================

template <auto expr> consteval auto forth_type_check() {
    return type_check<TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap, TRFAdd,
                      TRFSub, TRFMul, TRFIf, TRFTimes>(expr);
}

template <auto expr, auto env>
    requires requires {
        env.count;
        env.lookup("");
    }
consteval auto forth_type_check() {
    return type_check<TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap, TRFAdd,
                      TRFSub, TRFMul, TRFIf, TRFTimes>(expr, env);
}
```

**Step 2: Refactor `forth_compile` to use `forth_type_check`**

Replace the two `forth_compile` overloads (lines 397-421):

Before (with env):
```cpp
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
```

After:
```cpp
template <auto expr, auto env>
    requires requires {
        env.count;
        env.lookup("");
    }
consteval auto forth_compile() {
    using namespace refmacro;
    constexpr auto result = forth_type_check<expr, env>();
    static_assert(result.valid, "forth_compile: type check failed");
    constexpr auto stripped = reftype::strip_types(expr);
    constexpr auto rewritten = rewrite_forth(stripped);
    return compile<rewritten, MFNew, MFPush, MFDup, MFDrop, MFSwap, MFAdd,
                   MFSub, MFMul, MAdd, MSub, MMul, MDiv, MNeg, MCond, MLand,
                   MLor, MLnot, MEq, MLt, MGt, MLe, MGe, MProgn>();
}
```

Before (no env):
```cpp
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
```

After:
```cpp
template <auto expr> consteval auto forth_compile() {
    using namespace refmacro;
    constexpr auto result = forth_type_check<expr>();
    static_assert(result.valid, "forth_compile: type check failed");
    constexpr auto stripped = reftype::strip_types(expr);
    constexpr auto rewritten = rewrite_forth(stripped);
    return compile<rewritten, MFNew, MFPush, MFDup, MFDrop, MFSwap, MFAdd,
                   MFSub, MFMul, MAdd, MSub, MMul, MDiv, MNeg, MCond, MLand,
                   MLor, MLnot, MEq, MLt, MGt, MLe, MGe, MProgn>();
}
```

**Step 3: Replace standalone `type_check<10 rules>()` calls in demos**

Demo 3 (line 460-461):
```cpp
// Before:
constexpr auto r3 = type_check<TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
                               TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(prog3);
// After:
constexpr auto r3 = forth_type_check<prog3>();
```

Demo 4 (line 477-478):
```cpp
// Before:
constexpr auto r4 = type_check<TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
                               TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(prog4);
// After:
constexpr auto r4 = forth_type_check<prog4>();
```

Demo 6 commented (lines 496-498):
```cpp
// Before:
//   constexpr auto err1_r = type_check<
//       TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
//       TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(err1);
// After:
//   constexpr auto err1_r = forth_type_check<err1>();
```

Demo 7 commented (lines 511-513):
```cpp
// Before:
//   constexpr auto err2_r = type_check<
//       TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
//       TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(err2);
// After:
//   constexpr auto err2_r = forth_type_check<err2>();
```

Demo 8 commented (lines 524-526):
```cpp
// Before:
//   constexpr auto err3_r = type_check<
//       TRFNew, TRFPush, TRFDup, TRFDrop, TRFSwap,
//       TRFAdd, TRFSub, TRFMul, TRFIf, TRFTimes>(err3);
// After:
//   constexpr auto err3_r = forth_type_check<err3>();
```

**Step 4: Remove unused `type_check` import**

Line 32: `using reftype::type_check;` — remove this line since all calls now go through `forth_type_check` which uses the fully qualified `type_check` internally.

**Step 5: Build and run all tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure 2>&1 | tail -10`
Expected: all 563 tests pass

**Step 6: Run the example**

Run: `./build/types/examples/04_forth_dsl`
Expected: same output as before (all demos pass)

**Step 7: Commit**

```bash
git add types/examples/04_forth_dsl.cpp
git commit -m "refactor: extract forth_type_check to DRY type rule list"
```

---

## Execution: Subagent-Driven

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> to execute this plan task-by-task.

**Task Order:** Sequential, dependency-respecting order listed below.

1. Task 1: Add `compile_from` + `typed_strip` to `strip.hpp` — no dependencies
2. Task 2: Add tests — depends on Task 1
3. Task 3: Simplify `04_forth_dsl.cpp` — depends on Task 1

Each task is self-contained with full context. Execute one at a time with
fresh subagent per task and two-stage review (spec compliance, then code
quality).
