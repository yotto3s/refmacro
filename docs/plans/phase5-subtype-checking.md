# Phase 5: Subtype Checking

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Implement full subtype checking: same-base refinement implication, base widening, arrow contravariance, and type join.

**File:** `types/include/reftype/subtype.hpp`

**Depends on:** Phase 3 (constraints), Phase 6 (FM solver for refinement implication)

---

## Decisions Made

- Full subtyping: same-base refinement, unrefined-as-trivial, base widening (Int<:Real, Bool<:Int), arrow contravariance
- `#v` prefix for refinement variables (hygiene)
- `join({#v:T|P}, {#v:T|Q}) = {#v:T|P||Q}` — disjunction of predicates (principled)
- Subtype checker is recursive: structural for arrows, solver-based for refinements

## Subtyping Rules

### 1. Reflexivity
```
T <: T
```

### 2. Base Type Widening
```
Bool <: Int
Int  <: Real
Bool <: Real   (transitive)
```

### 3. Unrefined = Trivially Refined
```
T  ≡  {#v : T | true}
```

### 4. Same-Base Refinement Implication
```
{#v : T | P} <: {#v : T | Q}   iff   P(#v) => Q(#v) is valid
                                 iff   P(#v) && !Q(#v) is UNSAT
```

### 5. Refinement with Base Widening
```
{#v : Int | P} <: {#v : Real | Q}   iff   P(#v) => Q(#v) is valid
```

### 6. Arrow Contravariance/Covariance
```
(x : A₂) → B₁  <:  (x : A₁) → B₂
  iff  A₁ <: A₂        (contravariant in input)
  and  B₁ <: B₂        (covariant in output)
```
For dependent arrows where B mentions x: check that for all x satisfying A₁, B₁[x] <: B₂[x].

## Join (Least Upper Bound)

Used by MCond to compute result type from two branches.

```
join(T, T)                           = T                    (same type)
join(Int, Real) = join(Real, Int)    = Real                 (widening)
join(Bool, Int) = join(Int, Bool)    = Int                  (widening)
join(Bool, Real) = join(Real, Bool)  = Real                 (widening)
join({#v:T|P}, {#v:T|Q})            = {#v:T | P || Q}      (disjunction)
join({#v:Int|P}, {#v:Real|Q})       = {#v:Real | P || Q}   (widen + disjunction)
join(T, {#v:T|Q})                   = T                    (drop refinement)
```

## Implementation

```cpp
namespace reftype {

template <std::size_t Cap>
consteval bool is_subtype(Expression<Cap> sub, Expression<Cap> super) {
    auto sub_tag = get_type_tag(sub);
    auto super_tag = get_type_tag(super);

    // Reflexivity
    if (types_equal(sub, super)) return true;

    // Both base types → check widening
    if (is_base(sub) && is_base(super))
        return base_widens(sub_tag, super_tag);

    // Unrefined <: refined → treat unrefined as {#v:T|true}
    if (is_base(sub) && is_refined(super))
        return base_compatible(sub_tag, get_refined_base(super))
            && fm_is_valid(get_refined_pred(super));  // true => Q must mean Q is valid

    // Refined <: unrefined → always true if base compatible
    if (is_refined(sub) && is_base(super))
        return base_compatible(get_refined_base(sub), super_tag);

    // Refined <: refined
    if (is_refined(sub) && is_refined(super)) {
        if (!base_compatible(get_refined_base(sub), get_refined_base(super)))
            return false;
        // P => Q: check P && !Q is UNSAT
        auto vc = get_refined_pred(sub) && !get_refined_pred(super);
        return fm_is_unsat(vc);
    }

    // Arrow <: arrow
    if (is_arrow(sub) && is_arrow(super)) {
        return is_subtype(get_arrow_input(super), get_arrow_input(sub))   // contra
            && is_subtype(get_arrow_output(sub), get_arrow_output(super)); // co
    }

    return false;
}

template <std::size_t Cap>
consteval Expression<Cap> join(Expression<Cap> t1, Expression<Cap> t2) {
    if (types_equal(t1, t2)) return t1;

    // Base type widening
    if (is_base(t1) && is_base(t2))
        return wider_base(t1, t2);

    // Refined types: disjunction of predicates
    if (is_refined(t1) && is_refined(t2)) {
        auto base = wider_base(get_refined_base(t1), get_refined_base(t2));
        auto pred = get_refined_pred(t1) || get_refined_pred(t2);
        return tref(base, pred);
    }

    // Refined + unrefined: drop refinement
    if (is_refined(t1) && is_base(t2))
        return wider_base(get_refined_base(t1), t2);
    if (is_base(t1) && is_refined(t2))
        return wider_base(t1, get_refined_base(t2));

    throw "type error: incompatible types for join";
}

} // namespace reftype
```

## FM Solver Implications

The disjunction in join means the solver must handle `||` in predicates:
- For validity check `(P||Q) => R`: equivalent to `(P => R) && (Q => R)` — split into two independent checks
- Each independent check is a standard conjunction-based FM query
- This is manageable without full disjunctive FM

## Testing Strategy

- `{#v > 0 && #v < 10} <: {#v >= 0}` → pass
- `{#v > 0} <: {#v > 5}` → fail
- `{#v == 5} <: {#v > 0}` → pass
- `Int <: Real` → pass
- `Real <: Int` → fail
- `Bool <: Int` → pass
- `(Nat → Int) <: (Int → Int)` → pass (Nat = {#v:Int|#v>=0}, contravariant)
- `(Int → Nat) <: (Int → Int)` → pass (covariant)
- `(Int → Int) <: (Int → Nat)` → fail
- `join(TInt, TReal)` = `TReal`
- `join({#v>0}, {#v<0})` = `{#v : Int | #v>0 || #v<0}`
- `join(TInt, {#v>0})` = `TInt`
