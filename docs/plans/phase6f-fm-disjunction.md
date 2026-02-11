# Phase 6f: FM Solver — Disjunction Handling

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Provide algorithms for checking satisfiability and validity of disjunctive formulas (DNF).

**File:** `types/include/reftype/fm/disjunction.hpp`

**Depends on:** Phase 6a-e (all prior FM sub-phases)

---

## Design Note

The parser (Phase 6b) already produces `ParseResult` in DNF form — an array of `InequalitySystem` clauses representing disjunction of conjunctions. Phase 6f provides **algorithms over `ParseResult`**, not structural changes to the parser.

DNF conversion (distribution of `&&` over `||`, De Morgan) is handled during parsing in Phase 6b via `conjoin()` (cross-product) and `disjoin()` (concatenation).

## Strategy

FM natively handles conjunctions. For disjunctions (multiple clauses in `ParseResult`):

### Checking UNSAT of DNF
- `A || B` is UNSAT iff both `A` is UNSAT and `B` is UNSAT
- Iterate clauses: if any clause is SAT, the whole formula is SAT

### Checking validity of implication with DNF
- `(A || B) => C` is valid iff `(A => C)` and `(B => C)` are both valid
- For each premise clause, check implication against conclusion independently

### Checking validity of `P => (Q || R)`
- Equivalent to `P && !Q && !R` is UNSAT
- `!(Q || R)` = `!Q && !R` (De Morgan) → conjunction, handled by parser

## Implementation

```cpp
namespace reftype::fm {

// Check if a ParseResult (DNF) is unsatisfiable
// UNSAT iff every clause is UNSAT
template <std::size_t MaxClauses, std::size_t MaxIneqs, std::size_t MaxVars>
consteval bool is_unsat(const ParseResult<MaxClauses, MaxIneqs, MaxVars>& result) {
    for (std::size_t i = 0; i < result.clause_count; ++i) {
        if (!fm_is_unsat(result.clauses[i]))
            return false;
    }
    return true;
}

// Check if a ParseResult is satisfiable
template <std::size_t MaxClauses, std::size_t MaxIneqs, std::size_t MaxVars>
consteval bool is_sat(const ParseResult<MaxClauses, MaxIneqs, MaxVars>& result) {
    return !is_unsat(result);
}

} // namespace reftype::fm
```

## DNF Explosion Control

- `MaxClauses = 8` caps the number of disjuncts
- Refinement predicates are typically small (2-4 atoms)
- If DNF exceeds MaxClauses during parsing, parser throws: "DNF clause limit exceeded"

## Testing Strategy

- `x > 0 || x < -1` → 2 clauses, both SAT → SAT
- `x > 0 && x < 0` → 1 clause, UNSAT → UNSAT
- `(x > 0 && x < -1) || (x > 5 && x < 3)` → 2 clauses, both UNSAT → UNSAT
- `!(x > 0 || y > 0)` → De Morgan → conjunction → 1 clause
- DNF overflow: deeply nested `||` chains → parser throws error
