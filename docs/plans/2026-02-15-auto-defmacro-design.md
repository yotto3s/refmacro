# Auto-tracking defmacro Design

## Problem

Defining a macro and its AST creation function are redundant from the user's perspective. Currently users must:

1. Call `defmacro("tag", lowering_fn)` to define compilation
2. Pass the macro to `compile<e, M1, M2, ...>()` explicitly
3. Operator overloads separately call `make_node("tag", ...)` with the same tag

The tag string links them but appears in multiple places. `Macro::operator()` already creates AST nodes, so the operator overloads duplicate that capability.

## Solution

Make `defmacro` the single source of truth. The returned callable tracks macros in the Expression **type**, enabling `compile<e>()` to auto-discover all macros used to build `e`.

## API Change

```cpp
// Before:
constexpr auto MAdd = defmacro("add", [](auto l, auto r) {
    return [=](auto... a) constexpr { return l(a...) + r(a...); };
});
constexpr auto e = MAdd(x, y);
constexpr auto fn = compile<e, MAdd>();

// After:
constexpr auto MAdd = defmacro<"add">([](auto l, auto r) {
    return [=](auto... a) constexpr { return l(a...) + r(a...); };
});
constexpr auto e = MAdd(x, y);
constexpr auto fn = compile<e>();      // macros found from e's type
```

This is a clean break. The old `defmacro("tag", fn)` form is removed.

## New Types

### FixedString

Structural wrapper for string literals as NTTP:

```cpp
template <std::size_t N>
struct FixedString {
    char data[N]{};
    consteval FixedString(const char (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i) data[i] = s[i];
    }
};
```

### MacroSpec

Type-level macro identity. Carries tag (in the type via FixedString) and lowering function (via CompileFn type, default-constructible stateless lambda):

```cpp
template <FixedString Tag, typename CompileFn>
struct MacroSpec {
    static constexpr auto tag = Tag;
    static consteval auto compile_fn() { return CompileFn{}; }
};
```

### MacroCaller

Callable returned by `defmacro<"tag">(fn)`. Has `tag[16]` and `fn` fields for backward compatibility with existing `apply_macro` in compile_node. Builds AST nodes and embeds itself in Expression's NTTP value parameter pack:

```cpp
template <typename Spec>
struct MacroCaller {
    using spec_type = Spec;

    // Backward-compatible fields (same layout as old Macro)
    char tag[16]{};
    decltype(Spec::compile_fn()) fn{};

    consteval MacroCaller() { copy_str(tag, Spec::tag.data); }

    // Unary — embeds self in Expression's Macros pack
    template <std::size_t Cap, auto... Ms>
    consteval auto operator()(Expression<Cap, Ms...> c0) const {
        constexpr MacroCaller self{};
        Expression<Cap, self, Ms...> result;
        result.ast = c0.ast;
        result.id = result.ast.add_tagged_node(tag, {c0.id});
        return result;
    }

    // Binary — merges Macros from both children
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

    // Ternary and higher arities follow the same pattern
};
```

### defmacro

```cpp
template <FixedString Tag, typename CompileFn>
consteval auto defmacro(CompileFn) {
    return MacroCaller<MacroSpec<Tag, CompileFn>>{};
}
```

## Expression Type Evolution

Expression gains `auto... Macros` — a value NTTP parameter pack of MacroCaller instances:

```cpp
template <std::size_t Cap = 64, auto... Macros> struct Expression {
    AST<Cap> ast{};
    int id{-1};

    // Converting constructor: bridges Expression types with different Macros
    template <auto... OtherMs>
    consteval Expression(const Expression<Cap, OtherMs...>& other)
        : ast(other.ast), id(other.id) {}

    consteval Expression() = default;

    static consteval Expression lit(double v) { /* unchanged logic */ }
    static consteval Expression var(const char* name) { /* unchanged logic */ }
};

using Expr = Expression<64>;  // no macros, backward compat
```

The converting constructor is critical: it enables transforms and rule callbacks to create `Expression<Cap>` internally, which implicitly converts to/from `Expression<Cap, Ms...>` as needed.

Duplicates in the macro list are harmless — compile picks the first tag match.

## Auto-compile

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

This unified compile handles all cases:
- `compile<e>()` — auto-discovers embedded macros
- `compile<e, M1, M2>()` — explicit macros (backward compat)
- Both embedded and explicit — combined as `Embedded..., Extra...`

The existing `apply_macro` and `compile_node` work unmodified because MacroCaller has the same `tag`/`fn` fields as the old Macro type.

## Operator Overloads — Auto-tracking

Operators delegate to the corresponding MacroCaller, which auto-tracks macros:

### Math operators (in math.hpp)

```cpp
inline constexpr auto MAdd = defmacro<"add">([](auto l, auto r) { ... });

template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator+(Expression<Cap, Ms1...> lhs,
                         Expression<Cap, Ms2...> rhs) {
    return MAdd(lhs, rhs);  // delegates to MacroCaller, embeds MAdd
}

// Double-on-LHS/RHS overloads:
template <std::size_t Cap, auto... Ms>
consteval auto operator+(double lhs, Expression<Cap, Ms...> rhs) {
    return MAdd(Expression<Cap>::lit(lhs), rhs);
}
```

### Comparison + logical operators (in control.hpp)

Comparison and logical operators live in `control.hpp` and delegate to their corresponding MacroCallers, preserving macro tracking:

```cpp
inline constexpr auto MEq = defmacro<"eq">([](auto l, auto r) { ... });

template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator==(Expression<Cap, Ms1...> lhs,
                          Expression<Cap, Ms2...> rhs) {
    return MEq(lhs, rhs);  // delegates to MacroCaller, embeds MEq
}
```

## Transform Compatibility

Transforms accept `Expression<Cap, Ms...>`, strip macros internally via the converting constructor, and return the original type:

```cpp
template <std::size_t Cap = 64, auto... Ms, typename Rule>
consteval Expression<Cap, Ms...> rewrite(Expression<Cap, Ms...> e, Rule rule,
                                         int max_iters = 100) {
    Expression<Cap> plain = e;  // strip macros
    // ... existing rewrite logic using plain ...
    return result;  // implicit conversion back to Expression<Cap, Ms...>
}
```

Similarly for `transform`, `fold`, `simplify`, `differentiate`.

Internal detail functions (`rebuild_bottom_up`, `copy_subtree`, `trees_equal`) work with `Expression<Cap>` and are unchanged.

## Unchanged Components

- `AST<Cap>` — flat node storage
- `ASTNode` — structural node type
- `NodeView<Cap>` — read-only cursor
- `VarMap`, `Scope`, `TagStr` — compile infrastructure
- `apply_macro`, `compile_node` — macro dispatch (MacroCaller has same tag/fn layout)
- `pretty_print` — AST rendering
- `math_compile<e>()`, `ctrl_compile<e>()`, `full_compile<e>()` — convenience helpers

## End-to-end Example

```cpp
using namespace refmacro;

// Define custom macros
constexpr auto Abs = defmacro<"abs">([](auto x) {
    return [=](auto... a) constexpr {
        auto v = x(a...);
        return v < 0 ? -v : v;
    };
});

// Build expression — macros auto-tracked
constexpr auto x = Expr::var("x");
constexpr auto e = Abs(x);

// Compile — no macro list needed
constexpr auto fn = compile<e>();
static_assert(fn(-5.0) == 5.0);

// Operators also auto-track
constexpr auto y = Expr::var("y");
constexpr auto f = x * x + y;          // tracks MAdd, MMul
constexpr auto fn2 = compile<f>();       // auto-discovers MAdd, MMul
static_assert(fn2(3.0, 1.0) == 10.0);

// Mix custom macros with operators
constexpr auto g = Abs(x * x - 10.0);  // tracks Abs, MMul, MSub, MAdd
constexpr auto fn3 = compile<g>();
static_assert(fn3(1.0) == 9.0);        // |1-10| = 9
```
