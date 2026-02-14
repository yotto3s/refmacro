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
    consteval std::string_view sv() const { return {data, N - 1}; }
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

Callable returned by `defmacro<"tag">(fn)`. Builds AST nodes and propagates macro specs through the Expression type:

```cpp
template <typename Spec>
struct MacroCaller {
    using spec_type = Spec;

    // Unary
    template <std::size_t Cap, typename... Ms>
    consteval auto operator()(Expression<Cap, Ms...> child) const {
        Expression<Cap, Spec, Ms...> result;
        result.ast = child.ast;
        result.id = result.ast.add_tagged_node(Spec::tag.data, {child.id});
        return result;
    }

    // Binary
    template <std::size_t Cap, typename... Ms1, typename... Ms2>
    consteval auto operator()(
        Expression<Cap, Ms1...> lhs,
        Expression<Cap, Ms2...> rhs) const
    {
        Expression<Cap, Spec, Ms1..., Ms2...> result;
        result.ast = lhs.ast;
        int offset = result.ast.merge(rhs.ast);
        result.id = result.ast.add_tagged_node(
            Spec::tag.data, {lhs.id, rhs.id + offset});
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

Expression gains a variadic type parameter for tracked macros:

```cpp
template <std::size_t Cap = 64, typename... MacroSpecs>
struct Expression {
    AST<Cap> ast{};
    int id{-1};

    static consteval Expression lit(double v) { /* unchanged logic */ }
    static consteval Expression var(const char* name) { /* unchanged logic */ }
};

using Expr = Expression<64>;  // no macros, backward compat
```

MacroSpecs are **type parameters**, not value parameters. They don't affect structural-ness. `Expression<64, MacroSpec<"add", Fn>>` is still a valid NTTP.

Duplicates in the macro list are harmless — compile picks the first tag match.

## Auto-compile

```cpp
namespace detail {
    template <auto e, typename ExprType>
    struct auto_compiler;

    template <auto e, std::size_t Cap, typename... Specs>
    struct auto_compiler<e, Expression<Cap, Specs...>> {
        static consteval auto run() {
            constexpr auto vm = extract_var_map(e.ast, e.id);
            return compile_node_typed<e.ast, e.id, vm, Scope{}, Specs...>(
                std::tuple{});
        }
    };
}

// No-arg: auto-discover macros from expression type
template <auto e>
consteval auto compile() {
    return detail::auto_compiler<e, decltype(e)>::run();
}

// With extra macros: combine type-level + explicit
template <auto e, auto... ExtraMacros>
consteval auto compile() {
    // Convert ExtraMacros to their spec types, merge with e's type-level specs
    return detail::combined_compiler<e, ExtraMacros...>::run();
}
```

### compile_node dispatch (type-based)

```cpp
template <TagStr Tag, typename FirstSpec, typename... RestSpecs>
consteval auto apply_macro_typed(auto children_tuple) {
    if constexpr (tag_matches<Tag, FirstSpec>()) {
        return std::apply(FirstSpec::compile_fn(), children_tuple);
    } else {
        static_assert(sizeof...(RestSpecs) > 0, "no macro for tag");
        return apply_macro_typed<Tag, RestSpecs...>(children_tuple);
    }
}
```

## Operator Overloads — Auto-tracking

Operators auto-add their corresponding macro's spec to the return type.

### Math operators (in math.hpp)

```cpp
// MAdd defined first:
inline constexpr auto MAdd = defmacro<"add">([](auto l, auto r) { ... });

// operator+ references MAdd's spec:
template <std::size_t Cap, typename... Ms1, typename... Ms2>
consteval auto operator+(
    Expression<Cap, Ms1...> lhs,
    Expression<Cap, Ms2...> rhs)
{
    using Spec = typename decltype(MAdd)::spec_type;
    Expression<Cap, Spec, Ms1..., Ms2...> result;
    result.ast = lhs.ast;
    int offset = result.ast.merge(rhs.ast);
    result.id = result.ast.add_tagged_node("add", {lhs.id, rhs.id + offset});
    return result;
}

// Same pattern for -, *, /, unary -
// Double-on-LHS/RHS overloads delegate to the main overload
```

### Comparison + logical operators (moved to control.hpp)

Comparison (`==`, `<`, `>`, `<=`, `>=`) and logical (`&&`, `||`, `!`) operator overloads **move from `expr.hpp` to `control.hpp`**, where their macros (MEq, MLt, MLand, etc.) are defined.

```cpp
// In control.hpp, after MEq is defined:
inline constexpr auto MEq = defmacro<"eq">([](auto l, auto r) { ... });

template <std::size_t Cap, typename... Ms1, typename... Ms2>
consteval auto operator==(
    Expression<Cap, Ms1...> lhs,
    Expression<Cap, Ms2...> rhs)
{
    using Spec = typename decltype(MEq)::spec_type;
    Expression<Cap, Spec, Ms1..., Ms2...> result;
    // ... build "eq" node ...
    return result;
}
```

## Header Reorganization

| Header | Before | After |
|--------|--------|-------|
| `expr.hpp` | Expression, make_node, lit/var, comparisons, logicals, expr(), pipe | Expression, make_node, lit/var, expr(), pipe |
| `macro.hpp` | Macro, defmacro | FixedString, MacroSpec, MacroCaller, defmacro |
| `math.hpp` | Math macros + math operators | Math macros + math operators (now auto-tracking) |
| `control.hpp` | Control macros | Control macros + comparison/logical operators (moved from expr.hpp, now auto-tracking) |

## Transform Compatibility

All transforms gain `typename... Ms` in their signatures. Logic is unchanged.

```cpp
template <std::size_t Cap, typename... Ms>
consteval Expression<Cap, Ms...> simplify(Expression<Cap, Ms...> e) {
    // Same rewrite logic, Ms... propagates through return types
}

template <std::size_t Cap, typename... Ms>
consteval Expression<Cap, Ms...> differentiate(
    Expression<Cap, Ms...> e, const char* var) { ... }
```

`rewrite()`, `transform()`, `make_node()`, `to_expr()` all follow the same pattern.

`AST<Cap>`, `ASTNode`, `NodeView`, `VarMap`, `Scope`, `TagStr` are **unchanged**.

## Unchanged Components

- `AST<Cap>` — flat node storage
- `ASTNode` — structural node type
- `NodeView<Cap>` — read-only cursor
- `VarMap`, `Scope`, `TagStr` — compile infrastructure
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

constexpr auto Clamp = defmacro<"clamp">([](auto val, auto lo, auto hi) {
    return [=](auto... a) constexpr {
        auto v = val(a...);
        auto l = lo(a...);
        auto h = hi(a...);
        return v < l ? l : (v > h ? h : v);
    };
});

// Build expression — macros auto-tracked
constexpr auto x = Expr::var("x");
constexpr auto e = Clamp(Abs(x), Expr::lit(0.0), Expr::lit(100.0));

// Compile — no macro list needed
constexpr auto fn = compile<e>();
static_assert(fn(-5.0) == 5.0);
static_assert(fn(200.0) == 100.0);

// Operators also auto-track
constexpr auto y = Expr::var("y");
constexpr auto f = x * x + y;          // tracks MAdd, MMul
constexpr auto fn2 = compile<f>();       // auto-discovers MAdd, MMul
static_assert(fn2(3.0, 1.0) == 10.0);

// Mix custom macros with operators
constexpr auto g = Abs(x * x - 10.0);  // tracks Abs, MMul, MSub, MAdd(for lit?)
constexpr auto fn3 = compile<g>();
```
