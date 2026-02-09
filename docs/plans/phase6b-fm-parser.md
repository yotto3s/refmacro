# Phase 6b: FM Solver — Expression Parser

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Parse refmacro Expression ASTs (boolean formulas) into linear inequality systems.

**File:** `types/include/refmacro/types/fm/parser.hpp`

**Depends on:** Phase 6a (data structures), core refmacro (Expression, NodeView)

---

## Parsing Rules

### Arithmetic expressions → coefficient vectors

| AST Node | Parsing |
|----------|---------|
| `lit(n)` | constant term: `{terms: [], constant: n}` |
| `var("x")` | single term: `{terms: [{var: x, coeff: 1}], constant: 0}` |
| `add(a, b)` | sum coefficients of a and b |
| `sub(a, b)` | a + negate(b) |
| `mul(a, b)` | if one side is constant: scale other side's coefficients. Otherwise: reject (non-linear) |
| `neg(a)` | negate all coefficients and constant |

### Comparison operators → inequalities

| AST Node | Inequality (normalized to `expr >= 0` or `expr > 0`) |
|----------|------------------------------------------------------|
| `gt(a, b)` | `a - b > 0` (strict) |
| `ge(a, b)` | `a - b >= 0` |
| `lt(a, b)` | `b - a > 0` (strict) |
| `le(a, b)` | `b - a >= 0` |
| `eq(a, b)` | `a - b >= 0` AND `b - a >= 0` (two inequalities) |

### Logical connectives → system structure

| AST Node | Handling |
|----------|----------|
| `land(a, b)` | add both sides' inequalities to the same system (conjunction) |
| `lor(a, b)` | split into separate systems — handled by Phase 6f (disjunction) |
| `lnot(a)` | push negation inward (De Morgan), flip comparisons |

### Negation rules (for `lnot`)

| Original | Negated |
|----------|---------|
| `gt(a,b)` | `le(a,b)` |
| `ge(a,b)` | `lt(a,b)` |
| `lt(a,b)` | `ge(a,b)` |
| `le(a,b)` | `gt(a,b)` |
| `eq(a,b)` | `neq(a,b)` → `lt(a,b) \|\| gt(a,b)` (disjunction!) |
| `land(a,b)` | `lor(!a, !b)` (De Morgan) |
| `lor(a,b)` | `land(!a, !b)` (De Morgan) |

### Error cases

| AST Node | Error |
|----------|-------|
| `mul(var, var)` | "non-linear term: variable * variable" |
| `div(a, var)` | "division by variable not supported" |
| Unknown tag | "unsupported node in refinement predicate" |

## Implementation

```cpp
namespace refmacro::types::fm {

// Intermediate: linear expression (before comparison)
template <int MaxVars = 16>
struct LinearExpr {
    double coeffs[MaxVars]{};  // indexed by var_id
    double constant{0.0};
};

// Parse an arithmetic sub-expression into a LinearExpr
template <std::size_t Cap, int MaxVars>
consteval LinearExpr<MaxVars> parse_arith(
    NodeView<Cap> node, VarInfo<MaxVars>& vars) { ... }

// Parse a boolean formula into an InequalitySystem
template <std::size_t Cap, int MaxIneqs, int MaxVars>
consteval InequalitySystem<MaxIneqs, MaxVars> parse_formula(
    NodeView<Cap> node, VarInfo<MaxVars>& vars) { ... }

// Top-level: Expression AST → InequalitySystem
template <std::size_t Cap>
consteval auto parse_to_system(Expression<Cap> formula) {
    VarInfo<> vars{};
    return parse_formula(NodeView<Cap>{formula.ast, formula.id}, vars);
}

} // namespace refmacro::types::fm
```

## Testing Strategy

- `lit(5)` → LinearExpr with constant=5, no terms
- `var("x")` → LinearExpr with coeff[x]=1
- `var("x") + lit(3)` → LinearExpr with coeff[x]=1, constant=3
- `lit(2) * var("x")` → LinearExpr with coeff[x]=2
- `var("x") > lit(0)` → system with one inequality: x > 0
- `var("x") > lit(0) && var("x") < lit(5)` → system with two inequalities
- `var("x") * var("y")` → throws "non-linear"
- `!(var("x") > lit(0))` → `x <= 0`
- `!(a && b)` → De Morgan: `!a || !b` (produces disjunction)
