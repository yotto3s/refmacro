# Design: Update Refinement Type Examples to New defmacro API

## Context

The `feature/auto-defmacro` changes (now merged to `main` as `fdd65b9`) introduced:
- `defmacro<"tag">(lambda)` — tag as NTTP template argument (replaces `defmacro("tag", lambda)`)
- `Expression<Cap, auto... Macros>` — macros auto-tracked in expression type
- `compile<e>()` — auto-discovers macros from expression type
- `MacroCaller` — calling a macro auto-embeds it in the expression's macro pack
- `detail::compile_with_macros_from<stripped>(original_expr)` — recovers macros from original expression after strip_types

The `feat/add-refinement-type-examples` branch has 3 example files using the old API:
- `types/examples/02_builtin_type_rules.cpp`
- `types/examples/03_custom_type_rules.cpp`
- `types/examples/04_forth_dsl.cpp`

## Changes

### Step 1: Rebase onto main
`git rebase main` — branch only adds files under `types/examples/`, no overlap with main's changes. Clean rebase expected.

### Step 2: `02_builtin_type_rules.cpp` — No changes
Uses `typed_full_compile` which already handles auto-tracked macros on main.

### Step 3: `03_custom_type_rules.cpp`

1. **defmacro syntax**: `defmacro("clamp", ...)` -> `defmacro<"clamp">(...)`
2. **Replace `clamp()` helper** (raw `make_node("clamp", ...)`) with direct `MClamp(x, lo, hi)` calls. This auto-tracks `MClamp` in the expression type.
3. **Simplify `clamp_compile()`**: Use `reftype::detail::compile_with_macros_from<stripped>(expr)` instead of `compile<stripped, MClamp, MAdd, MSub, ...>`. The auto-tracked macros from operators and `MClamp` are recovered from the original expression.
4. **Remove unused imports**: `using refmacro::defmacro`, `using refmacro::make_node` (no longer needed for clamp).

### Step 4: `04_forth_dsl.cpp`

1. **defmacro syntax**: All 8 `refmacro::defmacro("f_xxx", ...)` -> `refmacro::defmacro<"f_xxx">(...)`
2. **Remove helper functions**: `f_new`, `f_push`, `f_dup`, `f_drop`, `f_swap`, `f_add`, `f_sub`, `f_mul` — use `MFXxx(...)` directly in program construction (auto-tracks macros).
3. **Keep `f_if` and `f_times`**: These stay as `make_node` helpers since they're handled by `rewrite_forth`, not by macros.
4. **Keep explicit macro list** in `forth_compile()`'s `compile<>` call — `rewrite_forth` returns `Expression<Cap>` which loses macro tracking.

## Non-changes
- `types/examples/CMakeLists.txt` — no changes needed
- `rewrite_forth` signature stays as `Expression<Cap>` — refactoring it to propagate macros is out of scope
