# Phase 4: Type Checker

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Implement a bidirectional type checker: single recursive walk that synthesizes types bottom-up while threading TypeEnv top-down, collecting subtyping constraints for the FM solver.

**Files:**
- `include/reftype/type_rules.hpp` — TypeRule, def_typerule, built-in rules
- `include/reftype/check.hpp` — synth(), type_check pipeline function

**Depends on:** Phases 1-3

---

## Decisions Made

- **Bidirectional type checking** — single recursive walk, not two-pass
- **Simple synthesis** — infer types for literals, arithmetic, let-bindings; explicit annotations for refinement types
- **defmacro-parallel type rules** — `def_typerule(tag, fn)` with tag dispatch
- **Custom recursive walker** — similar to `compile_node` in compile.hpp; neither fold nor transform supports the combined top-down/bottom-up pattern needed
- **Composable pipeline** — `type_check` is a pipe-able stage

## Core Algorithm: Bidirectional Synthesis

Single recursive function: `synth(node, env) → (Type, ConstraintSet)`

```
synth(Γ, lit(n))       = (Int, ∅)                    if n is integer-valued
                        = (Real, ∅)                   if n is fractional
synth(Γ, var("x"))     = (Γ(x), ∅)                   lookup in env
synth(Γ, add(a, b))    = let (Ta, C1) = synth(Γ, a)
                              (Tb, C2) = synth(Γ, b)
                          in (Numeric, C1 ∪ C2 ∪ {Ta <: Numeric, Tb <: Numeric})
synth(Γ, ann(e, T))    = let (Te, C) = synth(Γ, e)
                          in (T, C ∪ {Te <: T})       annotation → check subtype
synth(Γ, let(x, v, b)) = let (Tv, C1) = synth(Γ, v)
                              (Tb, C2) = synth(Γ[x:Tv], b)
                          in (Tb, C1 ∪ C2)
synth(Γ, cond(t, a, b))= let (Tt, C1) = synth(Γ, t)
                              (Ta, C2) = synth(Γ, a)
                              (Tb, C3) = synth(Γ, b)
                          in (join(Ta,Tb), C1∪C2∪C3 ∪ {Tt <: Bool})
synth(Γ, lambda(x, b)) = requires arrow annotation from context (or infer from usage)
synth(Γ, eq/lt/gt(a,b))= let (Ta, C1) = synth(Γ, a)
                              (Tb, C2) = synth(Γ, b)
                          in (Bool, C1 ∪ C2 ∪ {Ta <: Numeric, Tb <: Numeric})
```

## TypeRule (parallel to Macro)

```cpp
namespace reftype {

template <typename RuleFn>
struct TypeRule {
    char tag[16];
    RuleFn fn;
};

consteval auto def_typerule(const char* name, auto fn) {
    // fn signature: (NodeView<Cap>, TypeEnv<Cap>, synth_children...) → TypeResult<Cap>
    // mirrors defmacro pattern
    TypeRule rule;
    copy_str(name, rule.tag);
    rule.fn = fn;
    return rule;
}

// Dispatch: apply_typerule<TagStr{tag}, TypeRules...>()
// Same pattern as apply_macro in compile.hpp

} // namespace reftype
```

## TypeResult

```cpp
template <std::size_t Cap = 128>
struct TypeResult {
    Expression<Cap> type{};       // synthesized type of this node
    ConstraintSet<Cap> constraints; // accumulated subtyping constraints
    bool valid{true};              // false if synthesis failed
};
```

## Built-in Type Rules

| Tag | Rule |
|-----|------|
| `"lit"` | integer-valued → `TInt`, fractional → `TReal` |
| `"var"` | lookup in TypeEnv |
| `"ann"` | synth child, emit `synth_type <: declared_type` |
| `"add"`, `"sub"` | both Numeric → Numeric |
| `"mul"` | both Numeric → Numeric |
| `"div"` | Numeric, {#v : Numeric \| #v ≠ 0} → Numeric |
| `"neg"` | Numeric → Numeric |
| `"cond"` | Bool, T, U → join(T, U) |
| `"eq"`, `"lt"`, `"gt"`, `"le"`, `"ge"` | Numeric, Numeric → Bool |
| `"land"`, `"lor"` | Bool, Bool → Bool |
| `"lnot"` | Bool → Bool |

## Pipeline Integration

```cpp
// type_check as a pipe-able function
template <typename... TypeRules>
consteval auto type_check(Expression<Cap> e) {
    TypeEnv<Cap> empty_env{};
    auto result = synth(NodeView<Cap>{e.ast, e.id}, empty_env);
    // Send constraints to FM solver (Phase 6)
    for (int i = 0; i < result.constraints.count; ++i) {
        if (!fm_check_valid(result.constraints.constraints[i].formula))
            result.valid = false;
    }
    return result;
}

// Usage: auto result = e | type_check<TRAdd, TRSub, ...>;
```

## Implementation Notes

- The walker is similar to `compile_node` in `compile.hpp` — recursive tag dispatch
- TypeEnv is threaded as parameter (immutable, new env per scope)
- Children are recursively processed, results merged
- apply/lambda (let-binding) extends env like `compile_node` extends Scope
- `join(T1, T2)` for MCond: returns LCA in type lattice (Real if Int+Real, etc.)

## Open Questions

- How to handle `lambda` without an arrow type annotation? (Require annotation, or infer from call site?)
- What is `join(T1, T2)` when refinements differ? e.g., `join({#v > 0}, {#v < 0})` → `Int` (drop refinement)?
- Should the walker annotate the AST with types (for debugging/pretty-print) or just return TypeResult?

## Testing Strategy

- `synth(lit(5), ∅)` → type = TInt, no constraints
- `synth(var("x"), {x:TInt})` → type = TInt, no constraints
- `synth(add(lit(1), lit(2)), ∅)` → type = Numeric, constraints: Int <: Numeric (x2)
- `synth(ann(lit(5), TBool), ∅)` → constraints include TInt <: TBool (will fail at solver)
- `synth(let(x, lit(5), var("x")), ∅)` → type = TInt, env threads correctly
- `synth(ann(var("x")+lit(1), tref(TInt, var("#v")>lit(0))), {x:TInt})` → emits refinement VC
- Test type error detection: ill-typed programs produce unsatisfiable constraints
