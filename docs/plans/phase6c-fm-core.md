# Phase 6c: FM Solver — Fourier-Motzkin Core

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Implement the core Fourier-Motzkin variable elimination algorithm.

**File:** `types/include/reftype/fm/eliminate.hpp`

**Depends on:** Phase 6a (data structures)

---

## Algorithm

### Variable Elimination

For each variable x to eliminate from the system:

1. **Partition** inequalities into three groups:
   - **Lower bounds** (L): inequalities where x has positive coefficient → `x >= expr` or `x > expr`
   - **Upper bounds** (U): inequalities where x has negative coefficient → `x <= expr` or `x < expr`
   - **Unrelated** (N): inequalities where x doesn't appear

2. **Combine** each (lower, upper) pair:
   - From `x >= L_i` and `x <= U_j`: derive `L_i <= U_j`
   - Strictness: if either bound is strict, the combined result is strict
   - Normalize coefficients: divide through by |coeff of x| so x has coeff ±1

3. **Result**: new system = N ∪ {all combined pairs}

4. **Repeat** for next variable

### Contradiction Check

After all variables eliminated, only constant inequalities remain:
- `c >= 0` where c < 0 → contradiction → UNSAT
- `c > 0` where c <= 0 → contradiction → UNSAT
- No contradictions → SAT

### Normalization

Before elimination, normalize each inequality so the variable being eliminated has coefficient exactly +1 or -1:
- If `3x + 2y >= 5`, divide all by 3: `x + (2/3)y >= 5/3`
- This ensures clean combination of bounds

## Implementation

```cpp
namespace reftype::fm {

// Eliminate a single variable from the system
template <std::size_t MaxIneqs, std::size_t MaxVars>
consteval InequalitySystem<MaxIneqs, MaxVars> eliminate_variable(
    InequalitySystem<MaxIneqs, MaxVars> sys, int var_id) {

    InequalitySystem<MaxIneqs, MaxVars> result{};
    result.vars = sys.vars;

    // Partition into lower bounds, upper bounds, unrelated
    // ... (indices into sys.ineqs)

    // Copy unrelated inequalities
    // ...

    // For each (lower, upper) pair: combine
    // ...

    return result;
}

// Eliminate all variables and check for contradiction
template <std::size_t MaxIneqs, std::size_t MaxVars>
consteval bool fm_is_unsat(InequalitySystem<MaxIneqs, MaxVars> sys) {
    for (std::size_t v = 0; v < sys.vars.count; ++v)
        sys = eliminate_variable(sys, static_cast<int>(v));
    return has_contradiction(sys);
}

// Check constant-only system for contradictions
template <std::size_t MaxIneqs, std::size_t MaxVars>
consteval bool has_contradiction(InequalitySystem<MaxIneqs, MaxVars> sys) {
    for (std::size_t i = 0; i < sys.count; ++i) {
        auto& ineq = sys.ineqs[i];
        // All terms should have been eliminated — only constant remains
        double val = ineq.constant;
        if (ineq.strict && val <= 0.0) return true;   // 0 > c where c <= 0
        if (!ineq.strict && val < 0.0) return true;    // 0 >= c where c < 0
    }
    return false;
}

} // namespace reftype::fm
```

## Complexity Notes

- Each elimination step: O(|L| * |U|) new inequalities
- k variables: worst case O(n^(2^k))
- For 1-3 variables (typical refinements): very fast
- System capacity (MaxIneqs=64) provides a natural bound

## Testing Strategy

### Single variable
- `x >= 0 && x <= 5` → eliminate x → `0 <= 5` → SAT
- `x >= 5 && x <= 3` → eliminate x → `5 <= 3` → UNSAT (contradiction)
- `x > 0 && x < 0` → eliminate x → `0 < 0` → UNSAT

### Two variables
- `x >= 0 && y >= 0 && x + y <= 10` → SAT
- `x >= y && y >= x + 1` → eliminate y → `x >= x + 1` → UNSAT

### Strict vs non-strict
- `x > 0 && x <= 0` → UNSAT
- `x >= 0 && x <= 0` → SAT (x = 0)
- `x > 0 && x < 1` → SAT (for reals), need integer rounding for ints

### Edge cases
- Empty system → SAT
- Single inequality → SAT (unless constant contradiction)
- All unrelated inequalities → SAT
