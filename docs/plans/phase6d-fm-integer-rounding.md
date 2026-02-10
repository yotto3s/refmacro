# Phase 6d: FM Solver — Integer Rounding

> **Status:** Implemented

**Goal:** Tighten bounds for integer-typed variables before combining in FM elimination.

**Files:**
- `types/include/reftype/fm/rounding.hpp` — ceil/floor/tighten utilities
- `types/include/reftype/fm/eliminate.hpp` — FM core with pre-combination rounding

**Depends on:** Phase 6a (data structures), Phase 6c (FM core)

---

## Algorithm

Before combining bounds during variable elimination, tighten each bound for integer variables:

| Inequality | Integer rounding |
|-----------|-----------------|
| `x > 2.5` | `x >= 3` (ceil, make non-strict) |
| `x >= 2.5` | `x >= 3` (ceil) |
| `x < 2.5` | `x <= 2` (floor, make non-strict) |
| `x <= 2.5` | `x <= 2` (floor) |
| `x > 2` | `x >= 3` (ceil of 2+ε = 3, make non-strict) |
| `x < 2` | `x <= 1` (floor of 2-ε = 1, make non-strict) |
| `x >= 2` | unchanged |
| `x <= 2` | unchanged |

General rule:
- Lower bound (coefficient > 0): round constant up (ceil), make non-strict
- Upper bound (coefficient < 0): round constant down (floor), make non-strict
- Strict integer inequalities always become non-strict (integers have no "between" values)

## Design Constraint: No Int/Float Mixing

The refinement type system enforces that integer and floating-point variables cannot
be mixed in operations or comparisons. All variables in a given constraint system
share the same type. This is validated at compile time by `VarInfo::find_or_add`.

Consequence: when the eliminated variable is integer, ALL variables are integer,
so the LHS of any inequality is always an integer value. We can safely round the
constant of ALL bounds (including multi-variable) without checking for mixed terms.

## Integration with FM Core

**Pre-combination rounding** — tighten each bound BEFORE combining in `eliminate_variable`.

Post-elimination rounding does NOT work: after combining bounds, the eliminated
variable is gone. The resulting constant inequality cannot distinguish integer
vs real feasibility. Example: `x > 2 && x < 3` combines to `1 > 0` (SAT for
reals), but should be UNSAT for integers. Pre-combination rounding tightens to
`x >= 3 && x <= 2` first, then combines to `-1 >= 0` (UNSAT). ✓

## Implementation

```cpp
namespace reftype::fm {

consteval double ceil_val(double x) {
    double truncated = static_cast<double>(static_cast<long long>(x));
    if (x > truncated) return truncated + 1.0;
    return truncated;
}

consteval double floor_val(double x) {
    double truncated = static_cast<double>(static_cast<long long>(x));
    if (x < truncated) return truncated - 1.0;
    return truncated;
}

consteval bool is_integer_val(double x) {
    return x == static_cast<double>(static_cast<long long>(x));
}

// Round a bound inequality's constant for integer variable elimination.
// is_lower: true for lower bound (coeff > 0), false for upper bound (coeff < 0).
// Only modifies constant and strict flag; coefficients unchanged.
consteval LinearInequality round_integer_bound(
    LinearInequality ineq, bool is_lower) {
    if (is_lower) {
        // LHS + constant >= 0 means LHS >= -constant
        double bound = -ineq.constant;
        if (ineq.strict && is_integer_val(bound))
            ineq.constant = -(bound + 1.0);  // x > 2 → x >= 3
        else
            ineq.constant = -ceil_val(bound); // x >= 2.5 → x >= 3
    } else {
        // -LHS + constant >= 0 means LHS <= constant
        if (ineq.strict && is_integer_val(ineq.constant))
            ineq.constant -= 1.0;             // x < 3 → x <= 2
        else
            ineq.constant = floor_val(ineq.constant); // x <= 3.5 → x <= 3
    }
    ineq.strict = false;
    return ineq;
}

} // namespace reftype::fm
```

## Testing Strategy

- `x > 2.5 && x < 3.5` with x integer → rounds to `x >= 3 && x <= 3` → SAT (x=3)
- `x > 2 && x < 3` with x integer → rounds to `x >= 3 && x <= 2` → UNSAT
- `x >= 0 && x <= 0` with x integer → SAT (x=0)
- `x > 0 && x < 1` with x integer → rounds to `x >= 1 && x <= 0` → UNSAT
- `x > -0.5 && x < 0.5` with x integer → rounds to `x >= 0 && x <= 0` → SAT (x=0)
- Real variable: no rounding applied, `x > 0 && x < 1` → SAT
- `x + y > 4, x + y < 6`, both integer → SAT (x+y = 5)
- `x + y > 4, x + y < 5`, both integer → UNSAT
