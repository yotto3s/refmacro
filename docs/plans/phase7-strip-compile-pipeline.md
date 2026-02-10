# Phase 7: Strip-and-Compile Pipeline

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Strip type annotations after type checking, then compile normally. Wire up the full composable pipeline.

**File:** `include/reftype/strip.hpp`

**Depends on:** Phases 4-6

---

## Decisions Made

- Composable pipeline: `type_check | strip_types | compile`
- Each stage independently testable and inspectable
- `strip_types` uses existing `transform` primitive
- Same `Cap` in and out (no capacity reduction)

## strip_types

```cpp
namespace reftype {

template <std::size_t Cap = 128>
consteval Expression<Cap> strip_types(Expression<Cap> e) {
    return transform(e, [](NodeView<Cap> n, auto rec) consteval -> Expression<Cap> {
        // ann(expr, type) => just return the expr, drop the type
        if (str_eq(n.tag(), "ann"))
            return rec(n.child(0));

        // Bare type nodes outside annotations => error
        if (str_eq(n.tag(), "tint") || str_eq(n.tag(), "tbool") ||
            str_eq(n.tag(), "treal") || str_eq(n.tag(), "tref") ||
            str_eq(n.tag(), "tarr"))
            throw "bare type node outside annotation";

        // Everything else: recurse normally (rebuild via make_node)
        // Handle each arity: 0, 1, 2, 3, 4 children
        ...
    });
}

} // namespace reftype
```

## Full Pipeline

```cpp
// 1. Build typed expression
constexpr auto e = ann(Expr::var("x") + Expr::lit(1),
                       tref(TInt, Expr::var("#v") > Expr::lit(0)));

// 2. Type check (bidirectional synthesis + FM solver)
constexpr auto result = type_check<TRAdd, TRSub, TRMul, TRDiv>(e);
static_assert(result.valid, "type error");

// 3. Strip annotations
constexpr auto stripped = strip_types(e);

// 4. Compile to callable
constexpr auto f = compile<stripped, MAdd, MSub, MMul, MDiv, MNeg>();
```

## Pipe Operator Integration

```cpp
// Pipeline as pipe chain
constexpr auto f = e
    | type_check<TRAdd, TRSub, ...>  // returns TypeCheckResult with .expr and .valid
    | assert_valid                     // static_assert on .valid, pass through .expr
    | strip_types
    | [](auto stripped) { return compile<stripped, MAdd, ...>(); };
```

Note: The pipe-to-compile step is tricky because `compile` needs the expression as a template parameter, not a runtime value. This may need a helper:

```cpp
// typed_compile: all-in-one convenience
template <auto expr, typename... TypeRules, typename... Macros>
consteval auto typed_compile() {
    constexpr auto result = type_check<TypeRules...>(expr);
    static_assert(result.valid, "type check failed");
    constexpr auto stripped = strip_types(expr);
    return compile<stripped, Macros...>();
}
```

## Convenience Wrappers

```cpp
// Full pipeline with math + control type rules and macros
template <auto expr>
consteval auto typed_full_compile() {
    return typed_compile<expr,
        TRAdd, TRSub, TRMul, TRDiv, TRNeg,
        TRCond, TREq, TRLt, TRGt, TRLe, TRGe,
        TRLand, TRLor, TRLnot,
        MAdd, MSub, MMul, MDiv, MNeg,
        MCond, MEq, MLt, MGt, MLe, MGe,
        MLand, MLor, MLnot>();
}
```

## Testing Strategy

- `ann(lit(5), TInt)` → strips to `lit(5)`, compiles to constant 5
- `ann(var("x") + lit(1), tref(TInt, var("#v") > lit(0)))` → type checks, strips, compiles to function
- Nested annotations: `ann(ann(lit(5), TInt) + lit(1), TInt)` → strips both layers
- Invalid programs fail at type check, never reach compile
- Pipeline produces same runtime results as untyped compile
- Bare type nodes outside annotations → throw
- `typed_compile` convenience works end-to-end
