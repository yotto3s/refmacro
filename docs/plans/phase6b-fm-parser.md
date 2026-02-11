# Phase 6b: FM Solver — Expression Parser

> **Status:** Implemented.

**Goal:** Parse refmacro Expression ASTs (boolean formulas) into linear inequality systems, producing DNF (disjunction of conjunctions) for formulas containing `||` or `!(==)`.

**File:** `types/include/reftype/fm/parser.hpp`

**Depends on:** Phase 6a (data structures), core refmacro (Expression, NodeView)

---

## Design Decision: DNF-Producing Parser (Option B)

The parser returns a `ParseResult` — an array of `InequalitySystem` clauses representing DNF. This means:
- Purely conjunctive formulas → single clause
- Disjunctions (`||`) → multiple clauses
- De Morgan negation of `&&` → multiple clauses
- Negation of `==` → two clauses (`<` or `>`)

This avoids refactoring the parser return type in Phase 6f. Phase 6f adds algorithms *over* `ParseResult` (e.g., `is_unsat_dnf`), not structural changes.

## Parsing Rules

### Arithmetic expressions → coefficient vectors

| AST Node | Parsing |
|----------|---------|
| `lit(n)` | constant term: `{coeffs: all zero, constant: n}` |
| `var("x")` | single coefficient: `{coeffs[var_id("x")] = 1, constant: 0}` |
| `add(a, b)` | sum coefficients and constants of a and b |
| `sub(a, b)` | a + negate(b) |
| `mul(a, b)` | if one side is constant: scale other side's coefficients. Otherwise: reject (non-linear) |
| `div(a, lit(n))` | scale a's coefficients by `1/n`. Reject if `n == 0` |
| `div(a, var)` | reject (non-linear) |
| `neg(a)` | negate all coefficients and constant |

### Comparison operators → inequalities

All comparisons normalize to `sum(terms) + constant >= 0` or `> 0`:

| AST Node | Inequality |
|----------|------------|
| `gt(a, b)` | `a - b > 0` (strict) |
| `ge(a, b)` | `a - b >= 0` |
| `lt(a, b)` | `b - a > 0` (strict) |
| `le(a, b)` | `b - a >= 0` |
| `eq(a, b)` | `a - b >= 0` AND `b - a >= 0` (two inequalities, one clause) |

### Logical connectives → DNF structure

| AST Node | Handling |
|----------|----------|
| `land(a, b)` | Conjoin: cross-product of left/right clauses, merging inequalities within each pair |
| `lor(a, b)` | Disjoin: concatenate left/right clauses |
| `lnot(a)` | Push negation inward (De Morgan), flip comparisons |

### Negation rules (for `lnot`)

| Original | Negated |
|----------|---------|
| `gt(a,b)` | `le(a,b)` |
| `ge(a,b)` | `lt(a,b)` |
| `lt(a,b)` | `ge(a,b)` |
| `le(a,b)` | `gt(a,b)` |
| `eq(a,b)` | `lt(a,b) \|\| gt(a,b)` (disjunction — 2 clauses) |
| `land(a,b)` | `lor(!a, !b)` (De Morgan → disjunction) |
| `lor(a,b)` | `land(!a, !b)` (De Morgan → conjunction) |
| `lnot(a)` | `a` (double negation elimination) |

### Error cases

| AST Node | Error |
|----------|-------|
| `mul(var, var)` | "non-linear: variable * variable" |
| `div(a, var)` | "non-linear: division by variable" |
| `div(a, lit(0))` | "division by zero" |
| Unknown tag | "unsupported node in refinement predicate" |

## Data Structures

```cpp
namespace reftype::fm {

// Intermediate: linear expression as coefficient vector (before comparison)
template <std::size_t MaxVars = 16>
struct LinearExpr {
    double coeffs[MaxVars]{};  // indexed by var_id
    double constant{0.0};
};

// Parser output: DNF of InequalitySystem clauses
template <std::size_t MaxClauses = 8, std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
struct ParseResult {
    InequalitySystem<MaxIneqs, MaxVars> clauses[MaxClauses]{};
    std::size_t clause_count{0};

    consteval bool is_conjunctive() const { return clause_count == 1; }

    consteval const InequalitySystem<MaxIneqs, MaxVars>& system() const {
        if (clause_count != 1) throw "not a conjunctive formula";
        return clauses[0];
    }

    consteval ParseResult add_clause(InequalitySystem<MaxIneqs, MaxVars> sys) const {
        if (clause_count >= MaxClauses) throw "DNF clause limit exceeded";
        ParseResult r = *this;
        r.clauses[r.clause_count++] = sys;
        return r;
    }
};

} // namespace reftype::fm
```

## Functions

```cpp
namespace reftype::fm {

// Parse arithmetic sub-expression into a LinearExpr
template <std::size_t Cap, std::size_t MaxVars>
consteval LinearExpr<MaxVars> parse_arith(
    NodeView<Cap> node, VarInfo<MaxVars>& vars);

// Parse a boolean formula into ParseResult (DNF)
template <std::size_t Cap, std::size_t MaxClauses, std::size_t MaxIneqs, std::size_t MaxVars>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars> parse_formula(
    NodeView<Cap> node, VarInfo<MaxVars>& vars);

// Parse negated formula (De Morgan, comparison flipping)
template <std::size_t Cap, std::size_t MaxClauses, std::size_t MaxIneqs, std::size_t MaxVars>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars> parse_negated(
    NodeView<Cap> node, VarInfo<MaxVars>& vars);

// Conjoin two ParseResults: cross-product of clauses
template <std::size_t MaxClauses, std::size_t MaxIneqs, std::size_t MaxVars>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars> conjoin(
    const ParseResult<MaxClauses, MaxIneqs, MaxVars>& left,
    const ParseResult<MaxClauses, MaxIneqs, MaxVars>& right);

// Disjoin two ParseResults: concatenate clauses
template <std::size_t MaxClauses, std::size_t MaxIneqs, std::size_t MaxVars>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars> disjoin(
    const ParseResult<MaxClauses, MaxIneqs, MaxVars>& left,
    const ParseResult<MaxClauses, MaxIneqs, MaxVars>& right);

// Convert a LinearExpr difference to a LinearInequality
template <std::size_t MaxVars>
consteval LinearInequality to_inequality(
    const LinearExpr<MaxVars>& lhs, const LinearExpr<MaxVars>& rhs, bool strict);

// Top-level: Expression AST → ParseResult
template <std::size_t Cap>
consteval auto parse_to_system(const Expression<Cap>& formula);

} // namespace reftype::fm
```

## Algorithm: `conjoin`

Cross-product distribution for DNF conjunction:
```
conjoin({A, B}, {C, D}) = {A∧C, A∧D, B∧C, B∧D}
```
Each pair merges inequalities: copy all inequalities from both systems into one system. Shared `VarInfo` ensures consistent var_ids across clauses.

## Algorithm: `disjoin`

Concatenation:
```
disjoin({A, B}, {C}) = {A, B, C}
```

## Testing Strategy

### Arithmetic parsing (`parse_arith`)
- `lit(5)` → LinearExpr with constant=5
- `var("x")` → LinearExpr with coeffs[x]=1
- `var("x") + lit(3)` → coeffs[x]=1, constant=3
- `lit(2) * var("x")` → coeffs[x]=2
- `var("x") / lit(2)` → coeffs[x]=0.5
- `var("x") * var("y")` → throws "non-linear"
- `var("x") / lit(0)` → throws "division by zero"

### Comparison parsing (single clause)
- `var("x") > lit(0)` → 1 clause, 1 inequality: `x > 0`
- `var("x") >= lit(3)` → 1 clause, 1 inequality: `x - 3 >= 0`
- `var("x") == lit(5)` → 1 clause, 2 inequalities: `x - 5 >= 0`, `5 - x >= 0`

### Conjunction
- `x > 0 && x < 5` → 1 clause, 2 inequalities

### Disjunction
- `x > 0 || x < -1` → 2 clauses, 1 inequality each

### Negation
- `!(x > 0)` → 1 clause: `x <= 0`
- `!(x == 5)` → 2 clauses: `x < 5` or `x > 5`
- `!(a && b)` → De Morgan: 2+ clauses

### Mixed
- `(x > 0 || x < -1) && y > 0` → 2 clauses (distributed): `{x>0, y>0}`, `{x<-1, y>0}`

### Error cases
- `mul(var, var)` → throws
- `div(a, var)` → throws
- Unknown tag → throws
