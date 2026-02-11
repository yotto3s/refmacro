# Phase 6f: FM Solver — Disjunction Handling

> **Status:** Implemented. All planned functions (`clause_implies`, `remove_unsat_clauses`, `remove_subsumed_clauses`, `simplify_dnf`) and the optimized clause-by-clause `is_valid_implication` are complete with tests.

**Goal:** Provide advanced disjunction-handling algorithms beyond the basic DNF UNSAT/SAT checks.

**File:** `types/include/reftype/fm/disjunction.hpp`

**Depends on:** Phase 6a-e (all prior FM sub-phases)

---

## What moved to Phase 6e

The following functions are now in `solver.hpp` (Phase 6e), not here:
- `is_unsat(ParseResult)` — DNF: all clauses UNSAT
- `is_sat(ParseResult)` — negation of is_unsat
- `is_valid_implication(Expr, Expr)` — builds `P && !Q`, parses, checks UNSAT on ParseResult
- `is_valid(Expr)` — builds `!formula`, parses, checks UNSAT on ParseResult

These naturally belong in the solver's public API since `is_valid_implication` and `is_valid` already need DNF handling (negation can produce disjunctions via De Morgan).

## Design Note

The parser (Phase 6b) already produces `ParseResult` in DNF form — an array of `InequalitySystem` clauses representing disjunction of conjunctions. Phase 6f provides **advanced algorithms over `ParseResult`**, not structural changes to the parser.

DNF conversion (distribution of `&&` over `||`, De Morgan) is handled during parsing in Phase 6b via `conjoin()` (cross-product) and `disjoin()` (concatenation).

### VarInfo Consistency

`parse_to_system()` propagates the final `VarInfo` to all clauses after parsing. This guarantees that every clause in a `ParseResult` shares the same variable registry — `var_id` N always refers to the same variable across all clauses. Algorithms in this phase can safely compare or merge clauses without variable unification.

## Remaining Scope

Phase 6f focuses on disjunction-specific algorithms that go beyond basic UNSAT/SAT:

### Clause simplification
- Remove clauses that are subsumed by other clauses (if C1 => C2, drop C1 — in a disjunction the narrower clause is redundant since its solutions are already covered by the broader one)
- Detect and remove trivially UNSAT clauses early

### Implication with DNF premises (optimization)
- `(A || B) => C` is valid iff `(A => C)` and `(B => C)` are both valid
- This is an optimization over the brute-force `(A || B) && !C` approach
- Useful when the conclusion is a conjunction (avoids DNF explosion from negation)

### DNF Explosion Control (already implemented in Phase 6b)

- `MaxClauses = 8` caps the number of disjuncts (default template parameter in ParseResult)
- `add_clause()`, `conjoin()`, `disjoin()` all throw "DNF clause limit exceeded" when capacity is hit
- Refinement predicates are typically small (2-4 atoms)

## Testing Strategy

- Clause subsumption: `(x > 0 && x < 10) || (x > 0 && x < 5)` — second subsumed by first (dropped, 1 clause remains)
- UNSAT clause removal: `(x > 0 && x < 0) || (x >= 0)` — first clause UNSAT, removed
- Multi-clause implication: `(x > 0 || x < -1) => (x != 0)` — each clause implies conclusion
- Correctness equivalence: brute-force and clause-level implication give the same results
- simplify_dnf: combined removal of UNSAT + subsumed clauses
