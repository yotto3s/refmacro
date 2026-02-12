# Refinement Type System — Implementation Order

> **Status:** Work in progress — this plan should be refined before implementation.

## Repository Structure

The refinement type system lives in `types/` subdirectory, completely separate from core refmacro:

```
refmacro/
├── include/refmacro/       # core library (UNTOUCHED)
├── tests/                   # core tests (UNTOUCHED)
├── examples/                # core examples (UNTOUCHED)
├── CMakeLists.txt           # core build (add_subdirectory(types) gated by option)
│
└── types/                   # refinement type system (NEW)
    ├── include/reftype/
    │   ├── types.hpp            # Phase 1: type AST nodes
    │   ├── pretty.hpp           # Phase 1: type pretty-print
    │   ├── type_env.hpp         # Phase 2: TypeEnv
    │   ├── constraints.hpp      # Phase 3: Constraint/ConstraintSet
    │   ├── type_rules.hpp       # Phase 4: TypeRule, def_typerule
    │   ├── check.hpp            # Phase 4: bidirectional type checker
    │   ├── subtype.hpp          # Phase 5: subtype checking + join
    │   ├── strip.hpp            # Phase 7: strip_types
    │   ├── refinement.hpp       # umbrella include
    │   └── fm/                  # Phase 6: FM solver
    │       ├── types.hpp        # 6a: data structures
    │       ├── parser.hpp       # 6b: expression parser
    │       ├── eliminate.hpp    # 6c: FM core
    │       ├── rounding.hpp     # 6d: integer rounding
    │       ├── solver.hpp       # 6e: validity/sat API
    │       ├── disjunction.hpp  # 6f: DNF disjunction
    │       └── fm.hpp           # FM umbrella include
    ├── tests/
    │   ├── CMakeLists.txt
    │   ├── test_fm_types.cpp        # 6a
    │   ├── test_fm_core.cpp         # 6c
    │   ├── test_fm_rounding.cpp     # 6d
    │   ├── test_fm_parser.cpp       # 6b
    │   ├── test_fm_solver.cpp       # 6e
    │   ├── test_fm_disjunction.cpp  # 6f
    │   ├── test_types.cpp           # Phase 1
    │   ├── test_type_env.cpp        # Phase 2
    │   ├── test_constraints.cpp     # Phase 3
    │   ├── test_type_checker.cpp    # Phase 4
    │   ├── test_subtype.cpp         # Phase 5
    │   ├── test_strip.cpp           # Phase 7
    │   └── test_integration.cpp     # Phase 7+8: end-to-end
    ├── examples/
    │   ├── CMakeLists.txt
    │   └── 01_basic_refinement.cpp
    └── CMakeLists.txt           # links against refmacro::refmacro
```

## Dependency Graph

```
                    Phase 6a (FM data structures)
                         │
                    Phase 6c (FM core)
                         │
                    Phase 6d (integer rounding)
                         │
Phase 1 (types) ── Phase 6b (FM parser) ── needs core Expression
     │                   │
Phase 2 (env)       Phase 6e (validity API)
Phase 3 (constraints)    │
     │              Phase 6f (disjunction)
     │                   │
Phase 5 (subtype) ───────┘  uses FM for refinement implication
     │
Phase 4 (checker) ── uses all above
     │
Phase 7 (pipeline) ── uses Phase 4 + core compile
     │
Phase 8 (errors) ── integrated
```

## Implementation Order (14 steps)

### Track 1: FM Solver (standalone, hardest, validates feasibility)

| Step | Phase | What | Test file |
|------|-------|------|-----------|
| 1 | 6a | FM data structures: LinearInequality, InequalitySystem, VarInfo | test_fm_types.cpp |
| 2 | 6c | FM core: eliminate_variable, has_contradiction | test_fm_core.cpp |
| 3 | 6d | Integer rounding: ceil/floor tightening | test_fm_rounding.cpp |
| 4 | 6b | Expression → inequality parser | test_fm_parser.cpp |
| 5 | 6e | Validity/SAT public API: is_unsat, is_valid_implication | test_fm_solver.cpp |
| 6 | 6f | Disjunction handling: DNF splitting | test_fm_disjunction.cpp |

### Track 2: Type System (depends on core refmacro)

| Step | Phase | What | Test file |
|------|-------|------|-----------|
| 7 | 1 | Type AST nodes + constants + pretty-print | test_types.cpp |
| 8 | 2 | TypeEnv (bind, lookup, shadowing) | test_type_env.cpp |
| 9 | 3 | Constraint / ConstraintSet | test_constraints.cpp |
| 10 | 5 | Subtype checking (uses FM from Track 1) | test_subtype.cpp |
| 11 | 4 | Bidirectional type checker + def_typerule | test_type_checker.cpp |

### Track 3: Integration

| Step | Phase | What | Test file |
|------|-------|------|-----------|
| 12 | 7 | strip_types + pipeline wiring | test_strip.cpp |
| 13 | 8 | Error reporting (integrated into checker) | — |
| 14 | — | End-to-end integration test + example | test_integration.cpp |

## CMake Setup

### `types/CMakeLists.txt`

```cmake
add_library(reftype INTERFACE)
add_library(reftype::reftype ALIAS reftype)

target_include_directories(reftype INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(reftype INTERFACE refmacro::refmacro)

if(REFMACRO_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(REFMACRO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

### Root `CMakeLists.txt` addition

```cmake
option(REFMACRO_BUILD_TYPES "Build refinement type system" ON)
if(REFMACRO_BUILD_TYPES)
    add_subdirectory(types)
endif()
```

## Key Principle

**Zero modifications to core refmacro.** The `types/` directory depends on `refmacro::refmacro` (INTERFACE library) but never modifies it. This means:
- Core tests always pass unchanged
- Core examples always work unchanged
- Type system is a pure add-on, like a user-space library
- Could be extracted to a separate repo in the future if desired

## Branch Strategy

Each step is a logical unit of work suitable for a single branch/PR:
- `feature/fm-data-structures` (step 1)
- `feature/fm-core` (steps 2-3)
- `feature/fm-parser-api` (steps 4-6)
- `feature/type-representation` (steps 7-9)
- `feature/type-checker` (steps 10-11)
- `feature/type-pipeline` (steps 12-14)

**All development branches must be based on `feature/refinement-types`.**
**Development should be done in git worktrees.**

## TODO

- [x] Step 1 — FM data structures ([phase6a-fm-data-structures.md](phase6a-fm-data-structures.md))
- [x] Step 2 — FM core: eliminate_variable, has_contradiction ([phase6c-fm-core.md](phase6c-fm-core.md))
- [x] Step 3 — Integer rounding: ceil/floor tightening ([phase6d-fm-integer-rounding.md](phase6d-fm-integer-rounding.md))
- [x] Step 4 — Expression → inequality parser ([phase6b-fm-parser.md](phase6b-fm-parser.md))
- [x] Step 5 — Validity/SAT public API ([phase6e-fm-validity.md](phase6e-fm-validity.md))
- [x] Step 6 — Disjunction handling: DNF splitting ([phase6f-fm-disjunction.md](phase6f-fm-disjunction.md))
- [x] Step 7 — Type AST nodes + constants + pretty-print ([phase1-type-representation.md](phase1-type-representation.md))
- [ ] Step 8 — TypeEnv (bind, lookup, shadowing) ([phase2-type-environment.md](phase2-type-environment.md))
- [ ] Step 9 — Constraint / ConstraintSet ([phase3-constraints.md](phase3-constraints.md))
- [ ] Step 10 — Subtype checking ([phase5-subtype-checking.md](phase5-subtype-checking.md))
- [ ] Step 11 — Bidirectional type checker + def_typerule ([phase4-type-checker.md](phase4-type-checker.md))
- [ ] Step 12 — strip_types + pipeline wiring ([phase7-strip-compile-pipeline.md](phase7-strip-compile-pipeline.md))
- [ ] Step 13 — Error reporting ([phase8-error-reporting.md](phase8-error-reporting.md))
- [ ] Step 14 — End-to-end integration test + example
