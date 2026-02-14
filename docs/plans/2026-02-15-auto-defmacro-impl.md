# Auto-tracking defmacro Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make `defmacro<"tag">(fn)` the single source of truth so `compile<e>()` auto-discovers macros without explicit passing.

**Architecture:** Expression gains `auto... Macros` NTTP pack. MacroCaller (returned by new defmacro) embeds itself in Expression's type on each call. compile<e> extracts embedded macros via partial specialization. MacroCaller has `tag`/`fn` fields matching old Macro layout so existing compile_node works unmodified.

**Tech Stack:** C++26, GCC 16+, `-freflection`, GoogleTest, CMake

**Build/test commands:**
```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

---

### Task 1: Rename FixedString in pretty_print.hpp

`pretty_print.hpp` already has a `FixedString<N>` type (print output buffer). We need the name `FixedString` for the new NTTP string wrapper. Rename the pretty-print one to `PrintBuffer`.

**Files:**
- Modify: `include/refmacro/pretty_print.hpp`

**Step 1: Write a failing test that uses the name PrintBuffer**

In `tests/test_math.cpp`, add at the end:

```cpp
TEST(PrettyPrint, PrintBufferRename) {
    constexpr auto e = Expr::lit(1.0);
    constexpr refmacro::PrintBuffer<256> s = pretty_print(e);
    static_assert(s == "1");
}
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build 2>&1 | head -20`
Expected: FAIL — `PrintBuffer` is not defined.

**Step 3: Rename FixedString to PrintBuffer in pretty_print.hpp**

In `include/refmacro/pretty_print.hpp`:
- Replace all occurrences of `FixedString` with `PrintBuffer` (the class template, local usages, return type of `pretty_print`, return type of `pp_node`).
- There are ~10 occurrences: the struct definition, all `FixedString<256>` usages, and the `pretty_print` return type.

**Step 4: Run tests to verify everything passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS — PrintBuffer is functionally identical.

**Step 5: Commit**

```bash
git add include/refmacro/pretty_print.hpp tests/test_math.cpp
git commit -m "refactor: rename FixedString to PrintBuffer in pretty_print"
```

---

### Task 2: Add FixedString, MacroSpec, MacroCaller, new defmacro to macro.hpp

Add the new types alongside the old Macro/defmacro (which will be removed later).

**Files:**
- Modify: `include/refmacro/macro.hpp`
- Create: `tests/test_new_macro.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

Create `tests/test_new_macro.cpp`:

```cpp
#include <gtest/gtest.h>
#include <refmacro/macro.hpp>

using namespace refmacro;

// New-style defmacro
constexpr auto MyNeg = defmacro<"neg">([](auto x) {
    return [=](auto... a) constexpr { return -x(a...); };
});

constexpr auto MyAdd = defmacro<"add">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) + rhs(a...); };
});

constexpr auto MyCond = defmacro<"cond">([](auto test, auto then_, auto else_) {
    return [=](auto... a) constexpr {
        return test(a...) ? then_(a...) : else_(a...);
    };
});

TEST(NewDefmacro, BuildsUnaryNode) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = MyNeg(x);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
    static_assert(e.ast.nodes[e.id].child_count == 1);
}

TEST(NewDefmacro, BuildsBinaryNode) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = MyAdd(x, y);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
    static_assert(e.ast.nodes[e.id].child_count == 2);
}

TEST(NewDefmacro, BuildsTernaryNode) {
    constexpr auto c = Expr::var("c");
    constexpr auto t = Expr::lit(1.0);
    constexpr auto f = Expr::lit(2.0);
    constexpr auto e = MyCond(c, t, f);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "cond"));
    static_assert(e.ast.nodes[e.id].child_count == 3);
}

TEST(NewDefmacro, NestedConstruction) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = MyNeg(MyAdd(x, Expr::lit(1.0)));
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
}

TEST(NewDefmacro, HasTagAndFnFields) {
    // MacroCaller has tag/fn fields for backward compat with old compile
    static_assert(str_eq(MyAdd.tag, "add"));
    static_assert(str_eq(MyNeg.tag, "neg"));
}

TEST(NewDefmacro, OldStyleStillWorks) {
    // Old defmacro should still compile during transition
    constexpr auto OldAdd = defmacro("add", [](auto lhs, auto rhs) {
        return [=](auto... a) constexpr { return lhs(a...) + rhs(a...); };
    });
    constexpr auto x = Expr::var("x");
    constexpr auto e = OldAdd(x, Expr::lit(1.0));
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
}
```

Add to `tests/CMakeLists.txt`:
```cmake
add_executable(test_new_macro test_new_macro.cpp)
target_link_libraries(test_new_macro PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_new_macro PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_new_macro PROPERTIES TIMEOUT 60)
```

**Step 2: Run test to verify it fails**

Run: `cmake --build build 2>&1 | head -20`
Expected: FAIL — `defmacro<"neg">` doesn't exist yet.

**Step 3: Implement FixedString, MacroSpec, MacroCaller, new defmacro**

In `include/refmacro/macro.hpp`, add ABOVE the existing `Macro` struct:

```cpp
// --- NTTP string wrapper for tag literals ---

template <std::size_t N> struct FixedString {
    char data[N]{};
    consteval FixedString(const char (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = s[i];
    }
};

// --- MacroSpec: type-level macro identity ---

template <FixedString Tag, typename CompileFn> struct MacroSpec {
    static constexpr auto tag = Tag;
    static consteval auto compile_fn() { return CompileFn{}; }
};

// --- MacroCaller: callable with AST creation + macro tracking ---

template <typename Spec> struct MacroCaller {
    using spec_type = Spec;

    // Backward-compatible fields (same as old Macro)
    char tag[16]{};
    decltype(Spec::compile_fn()) fn{};

    consteval MacroCaller() { copy_str(tag, Spec::tag.data); }

    // Nullary
    consteval auto operator()() const {
        Expression<64> result;
        result.id = result.ast.add_tagged_node(tag, {});
        return result;
    }

    // Unary
    template <std::size_t Cap, auto... Ms>
    consteval auto operator()(Expression<Cap, Ms...> c0) const {
        constexpr MacroCaller self{};
        Expression<Cap, self, Ms...> result;
        result.ast = c0.ast;
        result.id = result.ast.add_tagged_node(tag, {c0.id});
        return result;
    }

    // Binary
    template <std::size_t Cap, auto... Ms1, auto... Ms2>
    consteval auto operator()(Expression<Cap, Ms1...> c0,
                              Expression<Cap, Ms2...> c1) const {
        constexpr MacroCaller self{};
        Expression<Cap, self, Ms1..., Ms2...> result;
        result.ast = c0.ast;
        int off = result.ast.merge(c1.ast);
        result.id = result.ast.add_tagged_node(tag, {c0.id, c1.id + off});
        return result;
    }

    // Ternary
    template <std::size_t Cap, auto... Ms1, auto... Ms2, auto... Ms3>
    consteval auto operator()(Expression<Cap, Ms1...> c0,
                              Expression<Cap, Ms2...> c1,
                              Expression<Cap, Ms3...> c2) const {
        constexpr MacroCaller self{};
        Expression<Cap, self, Ms1..., Ms2..., Ms3...> result;
        result.ast = c0.ast;
        int off1 = result.ast.merge(c1.ast);
        int off2 = result.ast.merge(c2.ast);
        result.id = result.ast.add_tagged_node(
            tag, {c0.id, c1.id + off1, c2.id + off2});
        return result;
    }

    // Quaternary
    template <std::size_t Cap, auto... Ms1, auto... Ms2, auto... Ms3,
              auto... Ms4>
    consteval auto operator()(Expression<Cap, Ms1...> c0,
                              Expression<Cap, Ms2...> c1,
                              Expression<Cap, Ms3...> c2,
                              Expression<Cap, Ms4...> c3) const {
        constexpr MacroCaller self{};
        Expression<Cap, self, Ms1..., Ms2..., Ms3..., Ms4...> result;
        result.ast = c0.ast;
        int off1 = result.ast.merge(c1.ast);
        int off2 = result.ast.merge(c2.ast);
        int off3 = result.ast.merge(c3.ast);
        result.id = result.ast.add_tagged_node(
            tag, {c0.id, c1.id + off1, c2.id + off2, c3.id + off3});
        return result;
    }
};

// --- New defmacro: tag as NTTP ---

template <FixedString Tag, typename CompileFn>
consteval auto defmacro(CompileFn) {
    return MacroCaller<MacroSpec<Tag, CompileFn>>{};
}
```

**IMPORTANT:** This requires Expression to already have `auto... Macros`. Since task 3 hasn't happened yet, the MacroCaller's unary/binary/ternary overloads won't compile. To make task 2 work independently, add these overloads AFTER task 3. For now, only add FixedString, MacroSpec, and the new defmacro overload signature. Defer MacroCaller's full body to task 3.

**Alternative approach (recommended):** Merge tasks 2 and 3 — implement Expression<Cap, auto... Macros> and MacroCaller together since they're codependent. See combined steps below.

**Step 4: Run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS

**Step 5: Commit**

```bash
git add include/refmacro/macro.hpp tests/test_new_macro.cpp tests/CMakeLists.txt
git commit -m "feat: add FixedString, MacroSpec, MacroCaller, new defmacro<\"tag\">"
```

---

### Task 3: Evolve Expression to Expression<Cap, auto... Macros>

Expression gains NTTP value parameter pack for tracking macros. Old code using `Expression<Cap>` or `Expr` continues to work (empty Macros pack).

**Files:**
- Modify: `include/refmacro/expr.hpp`

**Step 1: Write a failing test**

Add to `tests/test_new_macro.cpp` (or `tests/test_expr.cpp`):

```cpp
TEST(ExprMacros, ExpressionWithMacros) {
    // Expressions with macros embedded in type
    constexpr auto x = Expr::var("x");
    constexpr auto e = MyNeg(x);
    // e should have MyNeg embedded — verify AST is correct
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
    static_assert(e.ast.nodes[e.id].child_count == 1);
}

TEST(ExprMacros, NestedMacroTracking) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = MyNeg(MyAdd(x, y));
    // Should embed both MyNeg and MyAdd
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
}
```

**Step 2: Run test to verify it fails**

Expected: FAIL — MacroCaller templates need Expression<Cap, auto...> which doesn't exist yet.

**Step 3: Update Expression definition**

In `include/refmacro/expr.hpp`, change:

```cpp
// Before:
template <std::size_t Cap = 64> struct Expression {

// After:
template <std::size_t Cap = 64, auto... Macros> struct Expression {
```

Update `lit()` and `var()` return types to use the full type:

```cpp
static consteval Expression lit(double v) {
    Expression e;
    ASTNode n{};
    copy_str(n.tag, "lit");
    n.payload = v;
    e.id = e.ast.add_node(n);
    return e;
}

static consteval Expression var(const char* name) {
    Expression e;
    ASTNode n{};
    copy_str(n.tag, "var");
    copy_str(n.name, name);
    e.id = e.ast.add_node(n);
    return e;
}
```

These already return `Expression` (which now means `Expression<Cap, Macros...>`). Since `lit`/`var` are called on `Expr` (= `Expression<64>`), they return `Expression<64>` with empty Macros. Correct.

Update the `Expr` alias:

```cpp
using Expr = Expression<64>;
```

This is unchanged — `Expression<64>` means `Cap=64, Macros=()`.

Update `make_node` signatures — propagate Macros for uniform-type children:

```cpp
// Nullary (leaf) — no change needed, returns Expression<Cap> (no macros)
template <std::size_t Cap = 64>
consteval Expression<Cap> make_node(const char* tag) {
    Expression<Cap> result;
    result.id = result.ast.add_tagged_node(tag, {});
    return result;
}

// N-ary (1-8 children) — propagate uniform Macros
template <std::size_t Cap = 64, auto... Ms,
          std::same_as<Expression<Cap, Ms...>>... Rest>
    requires(sizeof...(Rest) <= 7)
consteval Expression<Cap, Ms...> make_node(const char* tag,
                                           Expression<Cap, Ms...> c0,
                                           Rest... rest) {
    Expression<Cap, Ms...> result;
    result.ast = c0.ast;
    int ids[1 + sizeof...(Rest)]{c0.id};
    [[maybe_unused]] std::size_t i = 1;
    ((ids[i++] = rest.id + result.ast.merge(rest.ast)), ...);
    result.id = result.ast.add_tagged_node(tag, ids, 1 + sizeof...(Rest));
    return result;
}
```

Update `expr()` helpers — these create Expression from lambdas applied to `Expr::var()`. Since `Expr::var()` returns `Expression<64>` (no macros), and the lambda builds via operators (which will eventually track macros), the return type is deduced via `auto`. No changes needed to `expr()` signatures.

Update pipe operator:

```cpp
template <std::size_t Cap, auto... Ms, typename F>
consteval auto operator|(Expression<Cap, Ms...> e, F transform_fn) {
    return transform_fn(e);
}
```

**Step 4: Run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS — existing code uses `Expression<64>` (empty Macros), deduction works.

**Step 5: Commit**

```bash
git add include/refmacro/expr.hpp
git commit -m "feat: evolve Expression<Cap, auto... Macros> for macro tracking"
```

---

### Task 4: Update transforms.hpp for auto... Macros

**Files:**
- Modify: `include/refmacro/transforms.hpp`

**Step 1: Verify existing transform tests still pass**

Run: `cmake --build build && ctest --test-dir build -R test_transforms --output-on-failure`
Expected: PASS (transforms use Expr = Expression<64>, no macros)

**Step 2: Update function signatures**

In `include/refmacro/transforms.hpp`:

Update `to_expr`:
```cpp
template <std::size_t Cap, auto... Ms>
consteval Expression<Cap, Ms...> to_expr(NodeView<Cap> root,
                                         NodeView<Cap> subtree) {
    Expression<Cap, Ms...> result;
    result.id = detail::copy_subtree(result.ast, root.ast, subtree.id);
    return result;
}
```

**Note:** `to_expr` is called in simplify/differentiate callbacks where the Expression type is `Expression<Cap, Ms...>`. The callback knows the Ms... from its closure. Since callbacks use `auto` return type, the compiler deduces Ms... from the call site.

**Actually**, `to_expr` has a problem: it's called inside rewrite/transform callbacks where Ms... isn't directly available. The callbacks return `Expression<Cap>` (via `Expr::lit(...)` or `to_expr(...)`). With the new type system, callbacks inside `rewrite`/`transform` need to return `Expression<Cap, Ms...>`.

Let me reconsider. The `rewrite` function:
```cpp
template <std::size_t Cap, auto... Ms, typename Rule>
consteval Expression<Cap, Ms...> rewrite(Expression<Cap, Ms...> e, Rule rule,
                                         int max_iters = 100);
```

The `rule` callback returns `std::optional<Expression<Cap, Ms...>>`. But rule is a lambda that doesn't know about Ms... — it uses concrete types like `Expr::lit(0.0)`.

**Solution:** Keep `to_expr` and internal detail functions working with `Expression<Cap>` (no macros). The outer `rewrite`/`transform` functions accept `Expression<Cap, Ms...>` but strip macros for internal processing, then re-add them to the result.

Updated approach:
```cpp
template <std::size_t Cap, auto... Ms, typename Rule>
consteval Expression<Cap, Ms...> rewrite(Expression<Cap, Ms...> e, Rule rule,
                                         int max_iters = 100) {
    // Strip macros for internal processing
    Expression<Cap> plain;
    plain.ast = e.ast;
    plain.id = e.id;

    auto result = detail::rewrite_impl(plain, rule, max_iters);

    // Re-wrap with macros
    Expression<Cap, Ms...> out;
    out.ast = result.ast;
    out.id = result.id;
    return out;
}
```

Similarly for `transform`, `fold`, `simplify`, `differentiate`.

This means internal detail functions (`rebuild_bottom_up`, `copy_subtree`, `to_expr`, etc.) stay unchanged — they work with `Expression<Cap>`. Only the public API wrappers handle the Macros pack.

Update all public functions:

```cpp
// to_expr: stays Expression<Cap> (used in callbacks)
// No change needed — callbacks create Expression<Cap> internally

// rewrite: wrap/unwrap
template <std::size_t Cap = 64, auto... Ms, typename Rule>
consteval Expression<Cap, Ms...> rewrite(Expression<Cap, Ms...> e, Rule rule,
                                         int max_iters = 100) {
    Expression<Cap> plain;
    plain.ast = e.ast;
    plain.id = e.id;
    for (int iter = 0; iter < max_iters; ++iter) {
        auto result = detail::rebuild_bottom_up<Cap>(plain.ast, plain.id, rule);
        if (detail::trees_equal(plain.ast, plain.id, result.ast, result.id)) {
            Expression<Cap, Ms...> out;
            out.ast = result.ast;
            out.id = result.id;
            return out;
        }
        plain = result;
    }
    Expression<Cap, Ms...> out;
    out.ast = plain.ast;
    out.id = plain.id;
    return out;
}

// transform: wrap/unwrap
template <std::size_t Cap = 64, auto... Ms, typename Visitor>
consteval Expression<Cap, Ms...> transform(Expression<Cap, Ms...> e,
                                           Visitor visitor) {
    Expression<Cap> plain;
    plain.ast = e.ast;
    plain.id = e.id;
    auto result = [&](auto self, int id) consteval -> Expression<Cap> {
        NodeView<Cap> view{plain.ast, id};
        auto rec = [&](NodeView<Cap> child) consteval -> Expression<Cap> {
            return self(self, child.id);
        };
        return visitor(view, rec);
    }([&](auto self, int id) consteval -> Expression<Cap> {
        // same lambda
    }, plain.id);
    // Actually this needs restructuring...
}
```

**Simpler approach:** Instead of wrapping/unwrapping, just add a converting constructor or assignment between `Expression<Cap>` and `Expression<Cap, Ms...>`. Since the data layout is identical (just AST + id), we can convert freely:

Add to Expression in `expr.hpp`:
```cpp
// Convert from any Expression with same Cap (strips or adds Macros)
template <auto... OtherMs>
consteval Expression(const Expression<Cap, OtherMs...>& other)
    : ast(other.ast), id(other.id) {}
```

This allows implicit conversion between Expression types with different Macros. Then transforms don't need wrapping — they accept `Expression<Cap, Ms...>`, internally work with it (the callbacks create `Expression<Cap>` which converts back), and return the right type.

Wait, this could cause ambiguity issues. Let me think...

Actually, a simpler and safer approach: just have the public rewrite/transform/fold accept any Expression and return the same type. With the converting constructor, callbacks that create `Expression<Cap>` (no macros) can be converted to `Expression<Cap, Ms...>` when needed.

**Final decision:** Add a template converting constructor to Expression. Keep all internal detail functions unchanged. Public functions just work because the constructor handles type bridging.

**Step 3: Add converting constructor to Expression**

In `include/refmacro/expr.hpp`, inside the Expression struct:

```cpp
template <auto... OtherMs>
consteval Expression(const Expression<Cap, OtherMs...>& other)
    : ast(other.ast), id(other.id) {}
```

**Step 4: Update public transform functions**

In `transforms.hpp`, update signatures only (no logic change):

```cpp
template <std::size_t Cap = 64, auto... Ms>
consteval Expression<Cap, Ms...> to_expr(NodeView<Cap> root,
                                         NodeView<Cap> subtree) {
    Expression<Cap, Ms...> result;
    result.id = detail::copy_subtree(result.ast, root.ast, subtree.id);
    return result;
}

template <std::size_t Cap = 64, auto... Ms, typename Rule>
consteval Expression<Cap, Ms...> rewrite(Expression<Cap, Ms...> e, Rule rule,
                                         int max_iters = 100) {
    Expression<Cap> plain = e;  // strip macros
    // ... existing logic using plain ...
    return result;  // implicit conversion back to Expression<Cap, Ms...>
}

template <std::size_t Cap = 64, auto... Ms, typename Visitor>
consteval Expression<Cap, Ms...> transform(Expression<Cap, Ms...> e,
                                           Visitor visitor) {
    Expression<Cap> plain = e;
    // ... existing logic using plain ...
    return result;
}

template <std::size_t Cap = 64, auto... Ms, typename Visitor>
consteval auto fold(Expression<Cap, Ms...> e, Visitor visitor) {
    Expression<Cap> plain = e;
    // ... existing logic using plain ...
}
```

The key insight: strip macros on input (convert to `Expression<Cap>`), use existing logic, return value converts back via constructor.

**Step 5: Run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS

**Step 6: Commit**

```bash
git add include/refmacro/expr.hpp include/refmacro/transforms.hpp
git commit -m "feat: add converting constructor, update transforms for Macros pack"
```

---

### Task 5: Update pretty_print.hpp and compile.hpp for auto... Macros

**Files:**
- Modify: `include/refmacro/pretty_print.hpp`
- Modify: `include/refmacro/compile.hpp`

**Step 1: Update pretty_print**

Change `pretty_print` signature:

```cpp
template <std::size_t Cap = 64, auto... Ms>
consteval PrintBuffer<256> pretty_print(const Expression<Cap, Ms...>& e) {
    return detail::pp_node(e.ast, e.id);
}
```

**Step 2: Update compile with unified macro extraction**

Replace the existing `compile` function in `compile.hpp`:

```cpp
namespace detail {

template <auto e, typename ExprType, auto... ExtraMacros>
struct unified_compiler;

template <auto e, std::size_t Cap, auto... Embedded, auto... Extra>
struct unified_compiler<e, Expression<Cap, Embedded...>, Extra...> {
    static consteval auto run() {
        constexpr auto vm = extract_var_map(e.ast, e.id);
        return compile_node<e.ast, e.id, vm, Scope{}, Embedded..., Extra...>(
            std::tuple{});
    }
};

} // namespace detail

template <auto e, auto... ExtraMacros> consteval auto compile() {
    return detail::unified_compiler<e, decltype(e), ExtraMacros...>::run();
}
```

This replaces the old compile. When `ExtraMacros` is empty and `e` has no embedded macros, behavior is identical. When `e` has embedded macros, they're automatically included. When extra macros are passed, they're appended.

**Step 3: Run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS — old code uses `Expression<64>` (no embedded macros), explicit macros still passed via ExtraMacros.

**Step 4: Commit**

```bash
git add include/refmacro/pretty_print.hpp include/refmacro/compile.hpp
git commit -m "feat: unified compile with embedded macro extraction"
```

---

### Task 6: Add auto-compile test

**Files:**
- Modify: `tests/test_new_macro.cpp`

**Step 1: Write the failing test**

Add to `tests/test_new_macro.cpp`:

```cpp
#include <refmacro/compile.hpp>

TEST(AutoCompile, UnaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = MyNeg(x);
    constexpr auto fn = compile<e>();  // no macro list!
    static_assert(fn(5.0) == -5.0);
}

TEST(AutoCompile, BinaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = MyAdd(x, y);
    constexpr auto fn = compile<e>();
    static_assert(fn(3.0, 4.0) == 7.0);
}

TEST(AutoCompile, NestedMacros) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = MyNeg(MyAdd(x, y));
    constexpr auto fn = compile<e>();
    static_assert(fn(3.0, 4.0) == -7.0);
}

TEST(AutoCompile, TernaryMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = MyCond(x, Expr::lit(1.0), Expr::lit(0.0));
    // MyCond has tag "cond" — but compile<e>() should find it
    constexpr auto fn = compile<e>();
    static_assert(fn(1.0) == 1.0);
    static_assert(fn(0.0) == 0.0);
}

TEST(AutoCompile, BackwardCompatExplicitMacros) {
    // Old style: pass macros explicitly — still works
    constexpr auto x = Expr::var("x");
    constexpr auto e = make_node("neg", x);  // raw node, no macro tracking
    constexpr auto fn = compile<e, MyNeg>();  // must pass explicitly
    static_assert(fn(5.0) == -5.0);
}
```

**Step 2: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build -R test_new_macro --output-on-failure`
Expected: ALL PASS (if tasks 2-5 are done correctly)

**Step 3: Commit**

```bash
git add tests/test_new_macro.cpp
git commit -m "test: add auto-compile tests for new defmacro"
```

---

### Task 7: Migrate math.hpp to new defmacro + auto-tracking operators

**Files:**
- Modify: `include/refmacro/math.hpp`
- Modify: `tests/test_math.cpp`

**Step 1: Update macro definitions in math.hpp**

Change all `defmacro("tag", fn)` to `defmacro<"tag">(fn)`:

```cpp
inline constexpr auto MAdd = defmacro<"add">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) + rhs(a...); };
});

inline constexpr auto MSub = defmacro<"sub">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) - rhs(a...); };
});

inline constexpr auto MMul = defmacro<"mul">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) * rhs(a...); };
});

inline constexpr auto MDiv = defmacro<"div">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) / rhs(a...); };
});

inline constexpr auto MNeg = defmacro<"neg">([](auto x) {
    return [=](auto... a) constexpr { return -x(a...); };
});
```

**Step 2: Update math operator overloads for auto-tracking**

Replace each operator overload. Example for `operator+`:

```cpp
template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator+(Expression<Cap, Ms1...> lhs,
                         Expression<Cap, Ms2...> rhs) {
    return MAdd(lhs, rhs);
}
```

This delegates to MAdd's `operator()` which handles AST merging AND macro tracking. Repeat for `-`, `*`, `/`, unary `-`.

For double-on-LHS/RHS overloads, delegate to the main overload:

```cpp
template <std::size_t Cap, auto... Ms>
consteval auto operator+(double lhs, Expression<Cap, Ms...> rhs) {
    return Expression<Cap>::lit(lhs) + rhs;
}
template <std::size_t Cap, auto... Ms>
consteval auto operator+(Expression<Cap, Ms...> lhs, double rhs) {
    return lhs + Expression<Cap>::lit(rhs);
}
```

**Step 3: Update simplify and differentiate**

Both return `Expression<Cap>` currently. Update to propagate Macros:

```cpp
template <std::size_t Cap = 64, auto... Ms>
consteval Expression<Cap, Ms...> simplify(Expression<Cap, Ms...> e) {
    // Convert to plain for internal rewrite (callbacks create Expression<Cap>)
    Expression<Cap> plain = e;
    auto is_lit = [](NodeView<Cap> v, double val) consteval {
        return v.tag() == "lit" && v.payload() == val;
    };
    auto result = rewrite(
        plain,
        [is_lit](NodeView<Cap> n) consteval -> std::optional<Expression<Cap>> {
            // ... same logic as before, unchanged ...
        });
    return result;  // converts back to Expression<Cap, Ms...>
}
```

Similarly for `differentiate`.

**Step 4: Update test_math.cpp**

Most tests should pass without changes (operators still create correct AST nodes). The key change: expressions built with operators now carry macro info in their type, but `math_compile<e>()` still works because it passes macros explicitly (and extra duplicates are harmless).

Verify tests pass. If any test does `constexpr Expr e = x + y;` (explicit Expr type), change to `constexpr auto e = x + y;` since the return type is now `Expression<64, MAdd>` not `Expression<64>`.

Check all test files for explicit `Expr` type annotations that need to become `auto`.

**Step 5: Run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS

**Step 6: Commit**

```bash
git add include/refmacro/math.hpp tests/test_math.cpp
git commit -m "feat: migrate math.hpp to new defmacro with auto-tracking operators"
```

---

### Task 8: Migrate control.hpp + move comparison/logical operators

**Files:**
- Modify: `include/refmacro/control.hpp`
- Modify: `include/refmacro/expr.hpp` (remove comparison/logical operators)
- Modify: `tests/test_control.cpp`
- Modify: `tests/test_expr.cpp` (if tests use comparison operators, needs include update)

**Step 1: Update macro definitions in control.hpp**

Change all `defmacro("tag", fn)` to `defmacro<"tag">(fn)`:

```cpp
inline constexpr auto MCond = defmacro<"cond">([](auto test, auto then_, auto else_) {
    return [=](auto... a) constexpr {
        return test(a...) ? then_(a...) : else_(a...);
    };
});

inline constexpr auto MLand = defmacro<"land">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) && rhs(a...); };
});
// ... etc for MLor, MLnot, MEq, MLt, MGt, MLe, MGe, MProgn
```

**Step 2: Move comparison/logical operators from expr.hpp to control.hpp**

Remove from `include/refmacro/expr.hpp`:
- `operator==`, `operator<`, `operator>`, `operator<=`, `operator>=` (both Expr-Expr and double variants)
- `operator&&`, `operator||`, `operator!`

Add to `include/refmacro/control.hpp` (after macro definitions), with auto-tracking:

```cpp
// --- Comparison operators (auto-tracking) ---

template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator==(Expression<Cap, Ms1...> lhs,
                          Expression<Cap, Ms2...> rhs) {
    return MEq(lhs, rhs);
}
// ... double-on-LHS/RHS variants ...

// --- Logical operators (auto-tracking) ---

template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator&&(Expression<Cap, Ms1...> lhs,
                          Expression<Cap, Ms2...> rhs) {
    return MLand(lhs, rhs);
}
// ... etc
```

**Step 3: Update lambda, apply, let_ for mixed macro types**

```cpp
template <std::size_t Cap = 64, auto... Ms>
consteval Expression<Cap, Ms...> lambda(const char* param,
                                        Expression<Cap, Ms...> body) {
    Expression<Cap, Ms...> result;
    result.ast = body.ast;
    ASTNode param_node{};
    copy_str(param_node.tag, "var");
    copy_str(param_node.name, param);
    int param_id = result.ast.add_node(param_node);
    result.id = result.ast.add_tagged_node("lambda", {param_id, body.id});
    return result;
}

template <std::size_t Cap = 64, auto... Ms1, auto... Ms2>
consteval auto apply(Expression<Cap, Ms1...> fn, Expression<Cap, Ms2...> arg) {
    Expression<Cap, Ms1..., Ms2...> result;
    result.ast = fn.ast;
    int offset = result.ast.merge(arg.ast);
    result.id = result.ast.add_tagged_node("apply", {fn.id, arg.id + offset});
    return result;
}

template <std::size_t Cap = 64, auto... Ms1, auto... Ms2>
consteval auto let_(const char* name, Expression<Cap, Ms1...> val,
                    Expression<Cap, Ms2...> body) {
    return apply(lambda(name, body), val);
}
```

**Step 4: Update ctrl_compile and full_compile**

These still work — they pass macros explicitly. No changes needed to their logic, but the macro types changed from old Macro to MacroCaller. Since both have `tag`/`fn` fields, compile_node works.

**Step 5: Update test_control.cpp**

Add `#include <refmacro/control.hpp>` if not already present (comparison operators moved there). Tests that use `x == y` etc. need control.hpp included.

Update any explicit `Expr` type annotations to `auto` if the expression now carries macros.

**Step 6: Update test_expr.cpp**

Remove or move tests that test comparison/logical operators (they now live in control.hpp). Or add `#include <refmacro/control.hpp>` to test_expr.cpp.

**Step 7: Run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS

**Step 8: Commit**

```bash
git add include/refmacro/expr.hpp include/refmacro/control.hpp tests/test_control.cpp tests/test_expr.cpp
git commit -m "feat: migrate control.hpp to new defmacro, move operators from expr.hpp"
```

---

### Task 9: Update test_compile.cpp and test_integration.cpp

**Files:**
- Modify: `tests/test_compile.cpp`
- Modify: `tests/test_integration.cpp`

**Step 1: Update test_compile.cpp**

This file defines its OWN macros with old syntax. Migrate them:

```cpp
// Before:
constexpr auto Neg = defmacro("neg", [](auto x) { ... });
constexpr auto Add = defmacro("add", [](auto lhs, auto rhs) { ... });

// After:
constexpr auto Neg = defmacro<"neg">([](auto x) { ... });
constexpr auto Add = defmacro<"add">([](auto lhs, auto rhs) { ... });
```

Add auto-compile test cases:

```cpp
TEST(Compile, AutoCompileUnary) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = Neg(x);
    constexpr auto fn = compile<e>();  // auto-discovered!
    static_assert(fn(5.0) == -5.0);
}

TEST(Compile, AutoCompileNested) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto e = Neg(Add(x, y));
    constexpr auto fn = compile<e>();
    static_assert(fn(3.0, 4.0) == -7.0);
}
```

**Step 2: Update test_integration.cpp**

Migrate custom macros (Abs, Clamp) to new syntax:

```cpp
constexpr auto Abs = defmacro<"abs">([](auto x) { ... });
constexpr auto Clamp = defmacro<"clamp">([](auto val, auto lo, auto hi) { ... });
```

Add auto-compile integration test:

```cpp
TEST(Integration, AutoCompileCustomMacro) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = Abs(x);
    constexpr auto fn = compile<e>();
    static_assert(fn(5.0) == 5.0);
    static_assert(fn(-3.0) == 3.0);
}

TEST(Integration, AutoCompileOperators) {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");
    constexpr auto f = x * x + y;
    constexpr auto fn = compile<f>();
    static_assert(fn(3.0, 1.0) == 10.0);
}

TEST(Integration, AutoCompileMixed) {
    constexpr auto x = Expr::var("x");
    constexpr auto e = Abs(x * x - 10.0);
    constexpr auto fn = compile<e>();
    static_assert(fn(1.0) == 9.0);  // |1-10| = 9
    static_assert(fn(4.0) == 6.0);  // |16-10| = 6
}
```

Update any explicit `Expr` type annotations to `auto`.

**Step 3: Run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS

**Step 4: Commit**

```bash
git add tests/test_compile.cpp tests/test_integration.cpp
git commit -m "test: migrate tests to new defmacro, add auto-compile tests"
```

---

### Task 10: Update test_transforms.cpp

**Files:**
- Modify: `tests/test_transforms.cpp`

**Step 1: Update test_transforms.cpp**

This file uses `Expr` type explicitly in several places. Review and update:
- Rewrite callbacks return `std::optional<Expr>` — this is fine since `Expr = Expression<64>` (no macros)
- Transform callbacks return `Expr` — same, fine
- Helper functions `root_tag`, `root_payload` take `const Expr&` — update to template:

```cpp
template <auto... Ms>
consteval auto root_tag(const Expression<64, Ms...>& e) -> std::string_view {
    return e.ast.nodes[e.id].tag;
}
```

Or simpler: just use `auto`:
```cpp
consteval auto root_tag(const auto& e) -> std::string_view {
    return e.ast.nodes[e.id].tag;
}
```

Also check: if tests create expressions with operators (like `Expr::var("x") + Expr::lit(1.0)`), these now return `Expression<64, MAdd>` not `Expr`. Callbacks that match `std::optional<Expr>` as return type would be fine IF there's implicit conversion. The converting constructor handles this.

**Step 2: Run tests**

Run: `cmake --build build && ctest --test-dir build -R test_transforms --output-on-failure`
Expected: ALL PASS

**Step 3: Commit**

```bash
git add tests/test_transforms.cpp
git commit -m "test: update test_transforms for Expression<Cap, Macros...>"
```

---

### Task 11: Remove old Macro and defmacro from macro.hpp

**Files:**
- Modify: `include/refmacro/macro.hpp`

**Step 1: Verify no code uses old-style defmacro**

Run: `grep -r 'defmacro("' include/ tests/` — should return nothing (all migrated in prior tasks).

**Step 2: Remove old types**

Remove from `include/refmacro/macro.hpp`:
- The old `Macro<CompileFn>` struct
- The old `defmacro(const char* name, CompileFn)` function

**Step 3: Run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS

**Step 4: Commit**

```bash
git add include/refmacro/macro.hpp
git commit -m "refactor: remove old Macro struct and defmacro(\"tag\", fn)"
```

---

### Task 12: Remove test_new_macro.cpp (merge into existing tests)

**Files:**
- Delete: `tests/test_new_macro.cpp`
- Modify: `tests/CMakeLists.txt`

The test_new_macro.cpp was scaffolding. The auto-compile tests are now in test_compile.cpp and test_integration.cpp. Remove test_new_macro.cpp and its CMake entry.

**Step 1: Remove file and CMake entry**

**Step 2: Run tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: ALL PASS

**Step 3: Commit**

```bash
git add -u tests/
git commit -m "cleanup: remove scaffolding test_new_macro.cpp"
```

---

### Task 13: Update design doc with refined approach

**Files:**
- Modify: `docs/plans/2026-02-15-auto-defmacro-design.md`

Update the design doc to reflect the actual implementation:
- `auto... Macros` value NTTPs (not `typename... MacroSpecs`)
- MacroCaller has `tag`/`fn` fields for backward compat
- Converting constructor for Expression
- Transforms strip/re-add macros internally

**Step 1: Update doc**

**Step 2: Commit**

```bash
git add docs/plans/2026-02-15-auto-defmacro-design.md
git commit -m "docs: update design doc to reflect final implementation"
```
