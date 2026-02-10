# Phase 3: Constraint Representation

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Define the constraint data structures that the type checker emits and the solver consumes.

**File:** `include/reftype/constraints.hpp`

**Depends on:** Phase 1 (type representation)

---

## Decisions Made

- Constraints store `Expression<Cap>` boolean formulas (not pre-parsed linear inequalities)
- FM solver parses Expression ASTs when solving — clean separation of concerns
- ConstraintSet accumulates constraints with origin metadata for error reporting
- Template-parametric on `Cap` and `MaxConstraints`
- Immutable API (same pattern as TypeEnv)

## Data Structures

```cpp
namespace reftype {

template <std::size_t Cap = 128>
struct Constraint {
    Expression<Cap> formula{};   // boolean expression to validate
    char origin[64]{};            // e.g., "add: operands must be numeric"
};

template <std::size_t Cap = 128, int MaxConstraints = 32>
struct ConstraintSet {
    Constraint<Cap> constraints[MaxConstraints]{};
    int count{0};

    consteval ConstraintSet add(Expression<Cap> formula, const char* origin) const {
        ConstraintSet result = *this;
        result.constraints[result.count].formula = formula;
        copy_str(origin, result.constraints[result.count].origin);
        result.count++;
        return result;
    }

    consteval ConstraintSet merge(const ConstraintSet& other) const {
        ConstraintSet result = *this;
        for (int i = 0; i < other.count; ++i)
            result.constraints[result.count++] = other.constraints[i];
        return result;
    }
};

} // namespace reftype
```

## Constraint Semantics

Constraints are boolean expressions in the existing refmacro expression language:
- `var("#v") > lit(0)` — value must be positive
- `var("x") == var("y")` — two values must be equal
- `(var("a") > lit(0)) && (var("b") > lit(0))` — conjunction
- Subtyping generates: `P(#v) && !Q(#v)` which the solver checks for UNSAT

## Design Notes

- `origin` is a human-readable string for error reporting, not machine-parsed
- No duplicate detection — solver handles redundancy
- Capacity overflow throws in consteval (compile error)
- Constraints don't carry source location (node ID) — origin string is sufficient for Phase 8

## Testing Strategy

- Build constraint sets: add single, verify count=1
- Merge two sets: verify union, correct count
- Verify constraint formulas are valid Expression ASTs
- Test capacity overflow: adding beyond MaxConstraints throws
- Test origin strings preserved correctly
