# Phase 6a: FM Solver — Data Structures

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Define the structural types for linear inequalities and inequality systems used by the FM solver.

**File:** `types/include/refmacro/types/fm/types.hpp`

**Depends on:** Nothing (standalone)

---

## Data Structures

```cpp
namespace refmacro::types::fm {

// A single term in a linear expression: coeff * var
struct LinearTerm {
    int var_id{-1};
    double coeff{0.0};
};

// A linear inequality: sum(terms) + constant OP 0
struct LinearInequality {
    LinearTerm terms[8]{};
    int term_count{0};
    double constant{0.0};
    bool strict{false};  // true for < and >, false for <= and >=
    // Normalized form: all inequalities are "expr >= 0" or "expr > 0"
    // For "expr <= 0": negate all terms and constant, keep strictness
    // For "expr = 0": represented as two inequalities (>= 0 and <= 0)
};

// Variable metadata: name + integer/real type
template <int MaxVars = 16>
struct VarInfo {
    char names[MaxVars][16]{};
    bool is_integer[MaxVars]{};
    int count{0};

    consteval int find_or_add(const char* name, bool integer = true) {
        for (int i = 0; i < count; ++i)
            if (str_eq(names[i], name)) return i;
        copy_str(name, names[count]);
        is_integer[count] = integer;
        return count++;
    }

    consteval int find(const char* name) const {
        for (int i = 0; i < count; ++i)
            if (str_eq(names[i], name)) return i;
        return -1;
    }
};

// A system of linear inequalities
template <int MaxIneqs = 64, int MaxVars = 16>
struct InequalitySystem {
    LinearInequality ineqs[MaxIneqs]{};
    int count{0};
    VarInfo<MaxVars> vars{};

    consteval InequalitySystem add(LinearInequality ineq) const {
        InequalitySystem result = *this;
        result.ineqs[result.count++] = ineq;
        return result;
    }
};

} // namespace refmacro::types::fm
```

## Design Notes

- All types are structural (NTTP-compatible) — fixed-size arrays, no pointers
- `LinearInequality` normalized to `expr >= 0` or `expr > 0` form
- Equality (`expr = 0`) represented as two inequalities
- `VarInfo` tracks integer vs real per variable — needed for Phase 6d (rounding)
- `MaxIneqs=64` is generous; typical refinement checks use 2-10 inequalities
- `terms[8]` limits to 8 variables per inequality (more than enough for refinements)

## Testing Strategy

- Construct `LinearTerm`, verify fields
- Construct `LinearInequality` with various terms
- `VarInfo`: find_or_add new names, find existing names, verify integer flag
- `InequalitySystem`: add inequalities, verify count
- Verify all types are structural (can be used as NTTP)
