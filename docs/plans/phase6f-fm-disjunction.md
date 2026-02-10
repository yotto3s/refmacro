# Phase 6f: FM Solver — Disjunction Handling

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Handle disjunctions (||) in formulas by splitting into independent FM queries.

**File:** `types/include/reftype/fm/disjunction.hpp`

**Depends on:** Phase 6a-e (all prior FM sub-phases)

---

## Strategy

FM natively handles conjunctions. For disjunctions, we split:

### Case 1: Checking UNSAT of `A || B`
- `A || B` is UNSAT iff both `A` is UNSAT and `B` is UNSAT
- Split into two independent FM queries

### Case 2: Checking validity of `(A || B) => C`
- Equivalent to `(A => C) && (B => C)`
- Split: check `A => C` and `B => C` independently

### Case 3: Checking validity of `A => (B || C)`
- Equivalent to `A && !B && !C` is UNSAT
- Negate: `!(B || C)` = `!B && !C` (De Morgan)
- Result is a conjunction — standard FM

### Case 4: Nested disjunctions
- `(A || B) && (C || D)` — convert to DNF:
  - `(A && C) || (A && D) || (B && C) || (B && D)`
  - Check each disjunct independently

## Implementation

```cpp
namespace reftype::fm {

// Normalize formula: push negations inward, distribute to DNF
// Returns a list of conjunctive clauses (the disjuncts of DNF)
template <std::size_t Cap, int MaxClauses = 8>
struct DNF {
    Expression<Cap> clauses[MaxClauses]{};
    int count{0};
};

template <std::size_t Cap>
consteval DNF<Cap> to_dnf(Expression<Cap> formula) {
    // If conjunction: cross-product of children's DNFs
    // If disjunction: concatenate children's DNFs
    // If atom: single clause
    ...
}

// Check UNSAT of a formula that may contain disjunctions
template <std::size_t Cap>
consteval bool is_unsat_dnf(Expression<Cap> formula) {
    auto dnf = to_dnf(formula);
    // UNSAT iff every disjunct is UNSAT
    for (int i = 0; i < dnf.count; ++i) {
        auto sys = parse_to_system(dnf.clauses[i]);
        if (!is_unsat(sys))
            return false;  // at least one clause is SAT
    }
    return true;
}

} // namespace reftype::fm
```

## DNF Explosion Control

DNF conversion can explode exponentially. Mitigations:
- `MaxClauses = 8` caps the number of disjuncts
- Refinement predicates are typically small (2-4 atoms)
- If DNF exceeds MaxClauses, throw error: "formula too complex for solver"

## Testing Strategy

- `x > 0 || x < -1` — 2 disjuncts
- `(x > 0 || x < 0) => x != 0` — split: both branches valid
- `(x > 0 && y > 0) || (x < 0 && y < 0)` — 2 disjuncts with multiple vars
- `!(x > 0 || y > 0)` → `x <= 0 && y <= 0` (De Morgan → conjunction)
- DNF overflow: deeply nested `||` chains → error
