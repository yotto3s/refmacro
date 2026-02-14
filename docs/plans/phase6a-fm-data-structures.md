# Phase 6a: FM Solver — Data Structures

> **Status:** Implemented on `feature/fm-data-structures`.

**Goal:** Define the structural types for linear inequalities and inequality systems used by the FM solver.

**File:** `types/include/reftype/fm/types.hpp`

**Depends on:** Nothing (standalone)

---

## Data Structures

See `types/include/reftype/fm/types.hpp` for the actual implementation.

Key differences from the original plan:
- Namespace: `reftype::fm` (not `refmacro::types::fm`)
- Template params use `std::size_t` (not `int`)
- `VarInfo::find()` returns `std::optional<int>` (not `int` with -1 sentinel)
- `LinearInequality::make()` factory enforces term_count invariant
- Bounds checks with `throw` in consteval context
- `MaxTermsPerIneq` named constant replaces magic number 8

## Design Notes

- All types are structural (NTTP-compatible) — fixed-size arrays, no pointers
- `LinearInequality` normalized to `expr >= 0` or `expr > 0` form
- Equality (`expr = 0`) represented as two inequalities
- `VarInfo` tracks integer vs real per variable — needed for Phase 6d (rounding)
- `MaxIneqs=64` is generous; typical refinement checks use 2-10 inequalities
- `terms[8]` limits to 8 variables per inequality (more than enough for refinements)

## Testing Strategy

- Construct `LinearTerm`, verify fields
- Construct `LinearInequality` with various terms
- `VarInfo`: find_or_add new names, find existing names, verify integer flag
- `InequalitySystem`: add inequalities, verify count
- Verify all types are structural (can be used as NTTP)
