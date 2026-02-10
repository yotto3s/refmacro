# Phase 6: Constexpr Fourier-Motzkin Solver (Overview)

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Build a consteval Fourier-Motzkin elimination solver for quantifier-free linear arithmetic with integer rounding.

**Directory:** `types/include/reftype/fm/`

**Depends on:** Standalone (only needs core Expression for parser)

---

## Sub-Phase Plans

| Sub-phase | File | Plan |
|-----------|------|------|
| 6a: Data Structures | `fm/types.hpp` | [phase6a-fm-data-structures.md](phase6a-fm-data-structures.md) |
| 6b: Expression Parser | `fm/parser.hpp` | [phase6b-fm-parser.md](phase6b-fm-parser.md) |
| 6c: FM Core | `fm/eliminate.hpp` | [phase6c-fm-core.md](phase6c-fm-core.md) |
| 6d: Integer Rounding | `fm/rounding.hpp` | [phase6d-fm-integer-rounding.md](phase6d-fm-integer-rounding.md) |
| 6e: Validity Interface | `fm/solver.hpp` | [phase6e-fm-validity.md](phase6e-fm-validity.md) |
| 6f: Disjunction | `fm/disjunction.hpp` | [phase6f-fm-disjunction.md](phase6f-fm-disjunction.md) |

## Implementation Order

```
6a (data structures) → 6c (FM core) → 6d (integer rounding) → 6b (parser) → 6e (validity API) → 6f (disjunction)
```

Note: 6c (FM core) can be tested with hand-built InequalitySystems before the parser (6b) exists.

## Decisions Made

- Fourier-Motzkin elimination (not DPLL(T) + Simplex)
- Integer rounding for integer-typed variables
- Disjunctions via DNF splitting
- Expression ASTs parsed into inequality systems at solve time
- All data structures are structural types (NTTP-compatible)
