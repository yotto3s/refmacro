# Phase 6d: FM Solver — Integer Rounding

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** After FM variable elimination, tighten bounds for integer-typed variables.

**File:** `types/include/refmacro/types/fm/rounding.hpp`

**Depends on:** Phase 6a (data structures), Phase 6c (FM core)

---

## Algorithm

After eliminating an integer-typed variable, the resulting constant bounds may be non-integer. Tighten them:

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

## Integration with FM Core

Two approaches:
1. **Post-elimination rounding:** After eliminating variable x, if x was integer, round the resulting constant bounds
2. **Pre-combination rounding:** Before combining bounds in the elimination step, round each bound

Option 1 is simpler: apply rounding to the combined inequalities after each elimination step.

## Implementation

```cpp
namespace refmacro::types::fm {

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

// Apply integer rounding to an inequality's constant
consteval LinearInequality integer_round(LinearInequality ineq, bool is_lower_bound) {
    if (is_lower_bound) {
        // x >= c or x > c → x >= ceil(c)
        ineq.constant = ceil_val(ineq.constant);
        ineq.strict = false;
    } else {
        // x <= c or x < c → x <= floor(c)
        ineq.constant = floor_val(ineq.constant);
        ineq.strict = false;
    }
    return ineq;
}

} // namespace refmacro::types::fm
```

## Testing Strategy

- `x > 2.5 && x < 3.5` with x integer → rounds to `x >= 3 && x <= 3` → SAT (x=3)
- `x > 2 && x < 3` with x integer → rounds to `x >= 3 && x <= 2` → UNSAT
- `x >= 0 && x <= 0` with x integer → SAT (x=0)
- `x > 0 && x < 1` with x integer → rounds to `x >= 1 && x <= 0` → UNSAT
- `x > -0.5 && x < 0.5` with x integer → rounds to `x >= 0 && x <= 0` → SAT (x=0)
- Real variable: no rounding applied, `x > 0 && x < 1` → SAT
