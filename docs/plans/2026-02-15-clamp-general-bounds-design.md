# Design: General Expression Bounds for Clamp

## Goal

Modify the `clamp` type rule in `03_custom_type_rules.cpp` so that `lo` and `hi` can be any expression (variables, arithmetic expressions), not just literals.

## Key Insight

`to_expr()` from `transforms.hpp` extracts arbitrary sub-trees from an AST into standalone `Expression<Cap>` objects. We use this to pull the lo/hi sub-expressions and embed them directly into the refinement predicate. The FM solver's `parse_arith` already handles variables and linear arithmetic, so no solver changes are needed.

## Changes

### TRClamp Type Rule

Replace the `lit`-only validation:

1. Type-check all three children via `synth_rec` (already done)
2. Validate lo/hi are numeric using `get_base_kind` (replaces `lit`-tag checks)
3. Extract lo/hi sub-expressions using `to_expr(root, child_nv)`
4. Build predicate: `(#v >= lo_expr) && (#v <= hi_expr)`

### New Example Sections

- **Section 6 — Variable bounds:** `clamp(x, lo, hi)` with symbolic refinement type `{#v >= lo && #v <= hi}`. Compiled function takes `(x, lo, hi)`.
- **Section 7 — Expression bounds:** `clamp(x, lo + 1, hi - 1)` annotated with wider bounds. FM solver proves `(#v >= lo+1 && #v <= hi-1) => (#v >= lo && #v <= hi)`.

### Backward Compatibility

Existing literal-bound examples (Sections 3-5) work unchanged — `to_expr` extracting `lit(0)` produces the same result as the current `Expr::lit(0)`.

## No Changes Needed

- FM solver / subtype checker
- `clamp_compile` helper
- `MClamp` runtime macro (already evaluates any expression)
