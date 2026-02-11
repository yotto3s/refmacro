# Phase 6f: FM Solver — Disjunction Handling

> **Status:** Work in progress — this plan should be refined before implementation.

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
- Remove clauses that are subsumed by other clauses (if C1 => C2, drop C2)
- Detect and remove trivially UNSAT clauses early

### Implication with DNF premises (optimization)
- `(A || B) => C` is valid iff `(A => C)` and `(B => C)` are both valid
- This is an optimization over the brute-force `(A || B) && !C` approach
- Useful when the conclusion is a conjunction (avoids DNF explosion from negation)

### DNF Explosion Control

- `MaxClauses = 8` caps the number of disjuncts
- Refinement predicates are typically small (2-4 atoms)
- If DNF exceeds MaxClauses during parsing, parser throws: "DNF clause limit exceeded"

## Testing Strategy

- Clause subsumption: `(x > 0 && x < 10) || (x > 0 && x < 5)` — second subsumed by first
- Multi-clause implication: `(x > 0 || x < -1) => (x != 0)` — each clause implies conclusion
- DNF overflow: deeply nested `||` chains → parser throws error
- Performance: compare brute-force vs clause-level implication checking
