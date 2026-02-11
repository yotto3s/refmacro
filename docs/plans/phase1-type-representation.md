# Phase 1: Type Representation as AST Nodes

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Define type AST nodes (tint, tbool, treal, tref, tarr, ann) as ordinary refmacro expressions, plus convenience constructors and pretty-print support.

**Files:**
- `types/include/reftype/types.hpp` — type constructors and constants
- Extend `pretty_print.hpp` or add `types/include/reftype/pretty.hpp` — type node rendering

**Depends on:** Nothing (Phase 0 fold already merged)

---

## Decisions Made

- Types are just more AST nodes in the same `Expression<Cap>` — "types are data" philosophy
- All composite type functions template-parametric on `Cap` (matching existing architecture)
- Base types (`TInt`, `TBool`, `TReal`) as `inline constexpr` constants (not functions)
- `TypedExpr = Expression<128>` convenience alias
- Refinement variables use `#v` prefix for hygiene
- Pretty-print support for type nodes included in Phase 1

## Type Nodes

| Type | Tag | Children | Meaning |
|------|-----|----------|---------|
| Int | `"tint"` | 0 | Integer base type |
| Bool | `"tbool"` | 0 | Boolean base type |
| Real | `"treal"` | 0 | Real/double base type |
| Refined | `"tref"` | 2: (base_type, predicate_expr) | `{#v : base \| pred(#v)}` |
| Arrow | `"tarr"` | 3: (param_name_node, input_type, output_type) | `(x : T) -> U` where U may mention x |
| Annotate | `"ann"` | 2: (expr, type) | `expr : type` — attaches type to expression |

## API

```cpp
namespace reftype {

// Base type constants (leaf nodes — Cap doesn't matter, merge handles it)
inline constexpr auto TInt  = make_node("tint");
inline constexpr auto TBool = make_node("tbool");
inline constexpr auto TReal = make_node("treal");

// Convenience alias for type-annotated expressions
using TypedExpr = Expression<128>;

// Composite type constructors (template on Cap)
template <std::size_t Cap = 64>
consteval Expression<Cap> tref(Expression<Cap> base, Expression<Cap> pred) {
    return make_node("tref", base, pred);
}

// Common refinement: positive integer
template <std::size_t Cap = 64>
consteval Expression<Cap> pos_int() {
    return tref<Cap>(TInt, Expression<Cap>::var("#v") > Expression<Cap>::lit(0));
}

// Arrow type: (param : In) -> Out
template <std::size_t Cap = 64>
consteval Expression<Cap> tarr(const char* param, Expression<Cap> in, Expression<Cap> out) {
    return make_node("tarr", Expression<Cap>::var(param), in, out);
}

// Type annotation: expr : type
template <std::size_t Cap = 64>
consteval Expression<Cap> ann(Expression<Cap> e, Expression<Cap> type) {
    return make_node("ann", e, type);
}

} // namespace reftype
```

## Pretty-Print Rules

| Tag | Rendered |
|-----|----------|
| `"tint"` | `Int` |
| `"tbool"` | `Bool` |
| `"treal"` | `Real` |
| `"tref"` | `{#v : Base \| pred}` |
| `"tarr"` | `(param : In) -> Out` |
| `"ann"` | `(expr : Type)` |

## Testing Strategy

- Construct `TInt`, `TBool`, `TReal` — verify tags and 0 children
- Construct `tref(TInt, var("#v") > lit(0))` — verify 2 children, correct tags
- Construct `tarr("x", TInt, TBool)` — verify 3 children, param stored as var node
- Construct `ann(var("x") + lit(1), TInt)` — verify 2 children
- Nested: `ann(var("x"), tref(TInt, var("#v") > lit(0)))` — verify deep structure
- `pretty_print` on each type node produces expected rendering
- `static_assert` on tag names and child counts
- Compose type ASTs with expression ASTs — merge works correctly
