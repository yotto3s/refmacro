# Phase 6e: FM Solver — Validity & Satisfiability Interface

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Provide the public API for checking satisfiability and validity of linear arithmetic formulas.

**File:** `types/include/refmacro/types/fm/solver.hpp`

**Depends on:** Phase 6a-d (data structures, parser, core, rounding)

---

## Public API

```cpp
namespace refmacro::types::fm {

// Check if a system of inequalities has no solution
template <int MaxIneqs = 64, int MaxVars = 16>
consteval bool is_unsat(InequalitySystem<MaxIneqs, MaxVars> sys) {
    for (int v = 0; v < sys.vars.count; ++v) {
        sys = eliminate_variable(sys, v);
        if (sys.vars.is_integer[v])
            sys = apply_integer_rounding(sys, v);
    }
    return has_contradiction(sys);
}

// Check if formula is satisfiable
template <int MaxIneqs = 64, int MaxVars = 16>
consteval bool is_sat(InequalitySystem<MaxIneqs, MaxVars> sys) {
    return !is_unsat(sys);
}

// Check if P => Q is valid (P && !Q is UNSAT)
// Takes Expression ASTs, parses them internally
template <std::size_t Cap>
consteval bool is_valid_implication(Expression<Cap> premise, Expression<Cap> conclusion) {
    auto negated_conclusion = negate_formula(conclusion);
    auto combined = conjunction(premise, negated_conclusion);
    auto system = parse_to_system(combined);
    return is_unsat(system);
}

// Convenience: check if a formula is always true
template <std::size_t Cap>
consteval bool is_valid(Expression<Cap> formula) {
    auto negated = negate_formula(formula);
    auto system = parse_to_system(negated);
    return is_unsat(system);
}

} // namespace refmacro::types::fm
```

## Usage from Type Checker

```cpp
// In subtype.hpp:
// {#v : Int | P} <: {#v : Int | Q}  iff  P => Q is valid
bool ok = fm::is_valid_implication(P, Q);

// In check.hpp:
// Constraint validation
for (auto& c : constraints) {
    if (!fm::is_valid(c.formula)) {
        result.valid = false;
        // report error with c.origin
    }
}
```

## Testing Strategy

- `is_sat({x > 0 && x < 5})` → true
- `is_unsat({x > 0 && x < 0})` → true
- `is_valid_implication({x > 0 && x < 10}, {x >= 0})` → true
- `is_valid_implication({x > 0}, {x > 5})` → false
- `is_valid({x >= 0 || x < 0})` → true (tautology) — requires Phase 6f
- `is_valid({x > 0})` → false (not always true)
- Integration: parse Expression AST end-to-end
