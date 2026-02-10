# Refinement Type System — Lisp-Style Macro Approach

> **Status:** Work in progress — individual phase plans should be refined before implementation.

**Goal:** Build a refinement type system as a user-space macro library on top of refmacro's core primitives, following the Lisp philosophy: types are data (AST nodes), type checking is a function over ASTs, everything is built from minimal core primitives.

**Architecture:** The type system is NOT baked into the core. Instead:
- Type annotations are ordinary AST nodes (tags like `"tint"`, `"tbool"`, `"tref"`, `"tarr"`)
- Type checking is a **bidirectional** recursive walk that synthesizes types bottom-up while threading a type environment top-down
- Type rules are defined via `def_typerule(tag, fn)` — parallel to `defmacro(tag, fn)`, the same Lisp-style tag dispatch
- A **Fourier-Motzkin** elimination solver (with integer rounding) validates refinement subtyping constraints
- A stripping transform removes type annotations before `compile()` runs
- Pipeline: `type_check | strip_types | compile` (composable stages)

**Tech Stack:** C++26 consteval, GCC 16+. All type system code lives in `types/` subdirectory (not modifying core). Depends on `fold()` from Plan 1.

**Separation:** The `types/` subdirectory has its own CMakeLists.txt, links `refmacro::refmacro` as a dependency. Zero modifications to core refmacro.

---

## Decisions

All open questions from the original design have been resolved:

| Topic | Decision |
|-------|----------|
| Capacity | Template-parametric `Cap` everywhere. `TypedExpr = Expression<128>` convenience alias |
| Type storage | Same `Expression<Cap>` AST — types are data, same as everything else |
| Base types | `inline constexpr` constants: `TInt`, `TBool`, `TReal` |
| Hygiene | `#v` prefix for refinement variables (e.g., `var("#v")`) |
| TypeEnv | Immutable, append-only with reverse-order lookup for shadowing |
| Constraints | Store `Expression<Cap>` boolean formulas. FM solver parses at solve time |
| Type checker | **Bidirectional** — single recursive walk synthesizing types up while threading env down |
| Inference scope | Simple synthesis: literals, arithmetic, let-bindings. Explicit annotations for refinement types |
| Type rules | `def_typerule(tag, fn)` parallel to `defmacro` — Lisp-style tag dispatch |
| Subtyping | Full: same-base refinement implication, unrefined-as-trivial, base widening (Bool<:Int<:Real), arrow contravariance |
| Join | `join({#v:T\|P}, {#v:T\|Q}) = {#v:T\|P\|\|Q}` — disjunction of predicates |
| Solver | **Fourier-Motzkin** elimination with integer rounding (not DPLL(T) + Simplex) |
| Disjunctions | Split `(P\|\|Q)=>R` into `(P=>R)&&(Q=>R)` |
| Pipeline | Composable stages: `type_check \| strip_types \| compile` |
| Cap reduction | Same Cap in, same Cap out for strip_types |
| Error messages | Structured with context, built via `FixedString<>` |
| Pretty-print | Type node rendering included in Phase 1 |
| File organization | `types/` subdirectory with own CMake target |

---

## Phase Plans

Detailed plans for each phase are in separate files:

| Phase | Plan file | What |
|-------|-----------|------|
| 1 | [phase1-type-representation.md](phase1-type-representation.md) | Type AST nodes, TInt/TBool/TReal constants, pretty-print |
| 2 | [phase2-type-environment.md](phase2-type-environment.md) | Immutable TypeEnv with shadowing |
| 3 | [phase3-constraints.md](phase3-constraints.md) | Constraint/ConstraintSet storing Expression ASTs |
| 4 | [phase4-type-checker.md](phase4-type-checker.md) | Bidirectional synth + def_typerule dispatch |
| 5 | [phase5-subtype-checking.md](phase5-subtype-checking.md) | Full subtyping with arrows, join with disjunction |
| 6 | [phase6-fm-solver.md](phase6-fm-solver.md) | Fourier-Motzkin solver (6 sub-phases: [6a](phase6a-fm-data-structures.md), [6b](phase6b-fm-parser.md), [6c](phase6c-fm-core.md), [6d](phase6d-fm-integer-rounding.md), [6e](phase6e-fm-validity.md), [6f](phase6f-fm-disjunction.md)) |
| 7 | [phase7-strip-compile-pipeline.md](phase7-strip-compile-pipeline.md) | strip_types + typed_compile convenience |
| 8 | [phase8-error-reporting.md](phase8-error-reporting.md) | Structured FixedString error messages |

---

## Implementation Order

See [implementation-order.md](implementation-order.md) for the full plan. Summary:

**FM solver first** (standalone, hardest, validates feasibility), then type system bottom-up:

```
Track 1 — FM Solver (steps 1-6):
  6a (data structures) → 6c (FM core) → 6d (rounding) → 6b (parser) → 6e (API) → 6f (disjunction)

Track 2 — Type System (steps 7-11):
  Phase 1 (types) → Phase 2 (env) → Phase 3 (constraints) → Phase 5 (subtype) → Phase 4 (checker)

Track 3 — Integration (steps 12-14):
  Phase 7 (strip+pipeline) → Phase 8 (errors) → end-to-end test
```

---

## Repository Structure

```
types/
├── include/reftype/
│   ├── types.hpp            # Phase 1
│   ├── pretty.hpp           # Phase 1
│   ├── type_env.hpp         # Phase 2
│   ├── constraints.hpp      # Phase 3
│   ├── type_rules.hpp       # Phase 4
│   ├── check.hpp            # Phase 4
│   ├── subtype.hpp          # Phase 5
│   ├── strip.hpp            # Phase 7
│   ├── refinement.hpp       # umbrella include
│   └── fm/                  # Phase 6
│       ├── types.hpp
│       ├── parser.hpp
│       ├── eliminate.hpp
│       ├── rounding.hpp
│       ├── solver.hpp
│       ├── disjunction.hpp
│       └── fm.hpp           # FM umbrella
├── tests/
├── examples/
└── CMakeLists.txt           # links refmacro::refmacro
```
