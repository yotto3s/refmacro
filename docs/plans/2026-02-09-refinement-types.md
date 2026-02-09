# Refinement Type System — Lisp-Style Macro Approach

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a refinement type system as a user-space macro library on top of refmacro's core primitives, following the Lisp philosophy: types are data (AST nodes), type checking is a function over ASTs, everything is built from minimal core primitives.

**Architecture:** The type system is NOT baked into the core. Instead:
- Type annotations are ordinary AST nodes (tags like `"tint"`, `"tbool"`, `"tref"`, `"tarr"`)
- Type checking is a `fold()` that walks the AST bottom-up, accumulating a type environment and constraint set
- An SMT solver (constexpr DPLL(T) for QF_LIA) validates the collected constraints
- A stripping transform removes type annotations before `compile()` runs
- Pipeline: `expr | type_check | strip_types | compile`

**Tech Stack:** C++26 consteval, GCC 16+. All type system code lives in user-space headers (not modifying core transforms/compile). Depends on `fold()` from Plan 1.

---

## Phase 0: Prerequisites

Implement the `fold()` primitive (see `2026-02-09-fold-primitive.md`). Everything below assumes `fold` exists.

---

## Phase 1: Type Representation as AST Nodes

Types are just more nodes in the tree. No new core types needed.

### Base types

| Type | Tag | Children | Meaning |
|------|-----|----------|---------|
| Int | `"tint"` | 0 | Integer base type |
| Bool | `"tbool"` | 0 | Boolean base type |
| Real | `"treal"` | 0 | Real/double base type |

### Refinement type

| Type | Tag | Children | Meaning |
|------|-----|----------|---------|
| Refined | `"tref"` | 2: (base_type, predicate_expr) | `{v : base \| pred(v)}` |

The predicate is an ordinary refmacro expression over a distinguished variable `"v"`. Example: `{v : Int \| v > 0}` is `make_node("tref", make_node("tint"), var("v") > lit(0))`.

### Function type (dependent arrow)

| Type | Tag | Children | Meaning |
|------|-----|----------|---------|
| Arrow | `"tarr"` | 3: (param_name_node, input_type, output_type) | `(x : T) -> U` where U may mention x |

### Type annotation node

| Node | Tag | Children | Meaning |
|------|-----|----------|---------|
| Annotate | `"ann"` | 2: (expr, type) | `expr : type` |

### Convenience constructors

```cpp
// types.hpp — all user-space, no core changes
namespace refmacro::types {

consteval Expr tint()  { return make_node("tint"); }
consteval Expr tbool() { return make_node("tbool"); }
consteval Expr treal() { return make_node("treal"); }

consteval Expr tref(Expr base, Expr pred) {
    return make_node("tref", base, pred);
}

// {v : Int | v > 0}
consteval Expr pos_int() {
    return tref(tint(), Expr::var("v") > Expr::lit(0));
}

consteval Expr tarr(const char* param, Expr in, Expr out) {
    // param name stored as a var node (reuses existing infrastructure)
    return make_node("tarr", Expr::var(param), in, out);
}

consteval Expr ann(Expr e, Expr type) {
    return make_node("ann", e, type);
}

} // namespace refmacro::types
```

### Testing strategy

- Construct type nodes, verify structure via `NodeView` and `pretty_print`
- `static_assert` on tag names, child counts, nested structure
- Verify type ASTs compose with expression ASTs (merge works)

---

## Phase 2: Type Environment

A compile-time map from variable names to their types (as Expression ASTs).

```cpp
// type_env.hpp
namespace refmacro::types {

template <int MaxBindings = 16>
struct TypeEnv {
    char names[MaxBindings][16]{};
    Expression<128> types[MaxBindings]{};  // larger Cap for type exprs
    int count{0};

    consteval TypeEnv bind(const char* name, Expression<128> type) const { ... }
    consteval Expression<128> lookup(const char* name) const { ... }
    consteval bool has(const char* name) const { ... }
};

} // namespace refmacro::types
```

**Key design decision:** TypeEnv stores types as Expression ASTs, not as some separate type representation. This is the Lisp way — types are data, same as everything else.

### Testing strategy

- Bind variables, look them up, verify correct types returned
- Test shadowing (rebind same name)
- Test lookup failure (throw in consteval = compile error)

---

## Phase 3: Constraint Representation

Constraints are what the type checker emits for the SMT solver. A constraint is a logical formula that must be satisfiable/valid.

```cpp
// constraints.hpp
namespace refmacro::types {

// A single constraint: an Expression AST representing a boolean formula
// plus context about where it came from (for error messages)
struct Constraint {
    Expression<128> formula{};
    char origin[64]{};  // e.g., "add: operands must be numeric"
};

template <int MaxConstraints = 32>
struct ConstraintSet {
    Constraint constraints[MaxConstraints]{};
    int count{0};

    consteval ConstraintSet add(Expression<128> formula, const char* origin) const { ... }
    consteval ConstraintSet merge(const ConstraintSet& other) const { ... }
};

} // namespace refmacro::types
```

Constraints are boolean expressions in the existing refmacro expression language. The SMT solver interprets them. Examples:
- `var("v") > lit(0)` — value must be positive
- `var("x") == var("y")` — two values must be equal
- `(var("a") > lit(0)) && (var("b") > lit(0))` — conjunction

### Testing strategy

- Build constraint sets, verify count and contents
- Merge two sets, verify union
- Verify constraints are valid Expression ASTs

---

## Phase 4: Type Checker (fold-based)

The type checker is a `fold()` over the AST. For each node, it:
1. Receives children's types (already folded)
2. Pattern-matches on the node tag
3. Checks compatibility / emits constraints
4. Returns this node's type

```cpp
// type_check.hpp
namespace refmacro::types {

struct TypeResult {
    Expression<128> type{};      // the inferred type of this node
    ConstraintSet<> constraints; // accumulated verification conditions
    bool valid{true};
};

// The fold visitor
consteval TypeResult check_node(NodeView<128> node, FoldChildren<TypeResult> children) {
    // lit => Real (or Int if integer-valued)
    if (node.tag() == "lit") { ... }

    // var => lookup in environment (environment threaded via ???)
    // NOTE: environment threading is a design challenge — see below
    if (node.tag() == "var") { ... }

    // add(lhs, rhs) => both must be numeric, result is numeric
    // emit constraint: if refined, propagate bounds
    if (node.tag() == "add") { ... }

    // ann(expr, type) => check expr's inferred type <: declared type
    // emit subtyping constraint for SMT
    if (node.tag() == "ann") { ... }

    // cond(test, then, else) => test must be Bool,
    // result type = join of then/else types
    if (node.tag() == "cond") { ... }

    ...
}

} // namespace refmacro::types
```

### Design challenge: environment threading

`fold()` passes children results bottom-up, but the type environment flows top-down (parent binds variables that children see). Two approaches:

**Option A: Two-pass.** First pass (top-down `transform`) annotates each `var` node with its type from the environment. Second pass (`fold`) checks constraints bottom-up. This separates concerns cleanly.

**Option B: Embed environment in fold result.** Each node's `TypeResult` carries the environment it was checked in. Let-bindings extend the env in their body child. This requires `fold` children to be processed left-to-right with threading, which the current `fold` doesn't support (it folds children independently).

**Option C: Pre-pass to resolve names.** Walk the AST with `transform` to resolve all variable references to their binding sites (de Bruijn indices or direct type annotations). Then `fold` only needs local information.

**Recommended: Option A** — simplest, uses existing primitives, separates name resolution from type checking.

### Testing strategy

- Check `lit(5)` infers `tint` or `treal`
- Check `var("x")` with known env infers correct type
- Check `add(lit(1), lit(2))` infers numeric type
- Check `ann(expr, type)` emits subtyping constraint
- Check type error: `ann(lit(5), tbool())` emits unsatisfiable constraint
- Check let-binding: `let x = 5 in x + 1` threads type of x correctly

---

## Phase 5: Subtype Checking via Constraint Implication

Subtyping between refinement types reduces to logical implication:

`{v : T | P(v)} <: {v : T | Q(v)}` iff `P(v) => Q(v)` is valid (i.e., `P(v) && !Q(v)` is unsatisfiable).

```cpp
// subtype.hpp
namespace refmacro::types {

// Given two refinement predicates, emit the implication constraint
consteval Expression<128> subtype_vc(Expression<128> sub_pred, Expression<128> super_pred) {
    // P => Q is equivalent to !P || Q
    // For SMT: check that P && !Q is UNSAT
    // We emit the negation: the constraint that must be UNSAT
    return sub_pred && !super_pred;
    // SMT solver checks this is UNSAT => subtyping holds
}

} // namespace refmacro::types
```

### Testing strategy

- `{v > 0 && v < 10} <: {v >= 0}` — should pass (emit UNSAT constraint)
- `{v > 0} <: {v > 5}` — should fail (not all positives are > 5)
- `{v == 5} <: {v > 0}` — should pass

---

## Phase 6: Constexpr SMT Solver

The heavyweight piece. A consteval DPLL(T) solver for quantifier-free linear integer/real arithmetic.

### Architecture

```
Formula (Expression AST)
    |
    v
Tseitin encoding → CNF clauses
    |
    v
DPLL core (unit propagation, conflict-driven clause learning)
    |
    v
Theory solver (linear arithmetic: simplex or Fourier-Motzkin)
    |
    v
SAT / UNSAT
```

### Data structures (all constexpr structural types)

```cpp
// smt/types.hpp
struct Literal { int var; bool negated; };
struct Clause { Literal lits[16]; int count; };
struct CNF { Clause clauses[64]; int count; };
struct Assignment { int values[32]; int count; }; // -1=unset, 0=false, 1=true

// smt/arithmetic.hpp
// Linear expressions: a1*x1 + a2*x2 + ... + c OP 0
struct LinearTerm { int var_id; double coeff; };
struct LinearConstraint {
    LinearTerm terms[8];
    int term_count;
    double constant;
    enum { EQ, LT, LE, GT, GE } op;
};
```

### Implementation phases

1. **CNF conversion** — Tseitin encoding of boolean formula AST to CNF
2. **DPLL core** — Unit propagation + backtracking (no CDCL initially)
3. **Theory interface** — Extract arithmetic constraints from boolean assignment
4. **Simplex solver** — Basic primal simplex for linear arithmetic feasibility
5. **DPLL(T) integration** — Theory conflicts drive boolean backtracking

### Practical constraints

- GCC `-fconstexpr-ops-limit` may need increasing for non-trivial formulas
- Keep data structures small and fixed-size (structural types for NTTP)
- Start with simple cases: constant comparisons, linear inequalities
- Can always fall back to "assume valid" for formulas outside supported fragment

### Testing strategy

- Propositional: `(a || b) && (!a || c)` — SAT
- Propositional: `a && !a` — UNSAT
- Linear arithmetic: `x > 0 && x < 5` — SAT
- Linear arithmetic: `x > 0 && x < 0` — UNSAT
- Subtype queries: `(x > 0 && x < 10) => (x >= 0)` — VALID
- Integration: type check a simple expression end-to-end

---

## Phase 7: Strip-and-Compile Pipeline

After type checking passes, strip type annotations and compile normally.

```cpp
// strip_types.hpp
namespace refmacro::types {

template <std::size_t Cap = 128>
consteval Expression<Cap> strip_types(Expression<Cap> e) {
    return transform(e, [](NodeView<Cap> n, auto rec) consteval -> Expression<Cap> {
        // ann(expr, type) => just return the expr, drop the type
        if (n.tag() == "ann")
            return rec(n.child(0));
        // type nodes shouldn't appear outside annotations
        // but if they do, they're erased
        if (n.tag() == "tint" || n.tag() == "tbool" ||
            n.tag() == "treal" || n.tag() == "tref" || n.tag() == "tarr")
            throw "bare type node outside annotation";
        // everything else: recurse normally
        // (reconstruct via make_node)
        ...
    });
}

} // namespace refmacro::types
```

### Full pipeline

```cpp
constexpr auto e = ann(Expr::var("x") + Expr::lit(1), tref(tint(), var("v") > lit(0)));

// Phase 1: type check (fold → constraints)
constexpr auto result = e | type_check;
static_assert(result.valid, "type error");  // or: throw in consteval

// Phase 2: strip annotations
constexpr auto stripped = e | strip_types;

// Phase 3: compile to callable
constexpr auto f = compile<stripped, MAdd, MSub, MMul, MDiv, MNeg>();
```

### Testing strategy

- `ann(lit(5), tint())` → strips to `lit(5)`, compiles to constant
- `ann(var("x") + lit(1), tref(tint(), var("v") > lit(0)))` → type checks with constraint `x + 1 > 0`, strips to `var("x") + lit(1)`, compiles to function
- Nested annotations strip cleanly
- Invalid programs fail at type check, never reach compile

---

## Phase 8: Error Reporting

When type checking fails, provide meaningful compile-time diagnostics.

```cpp
// In consteval context, throw with a descriptive string:
throw "type error: expression ann(x + 1, {v:Int|v>0}) requires x + 1 > 0, "
      "but constraint is not provable from context";
```

GCC displays `throw` strings in consteval error messages. Use `FixedString<>` to build structured error messages:

```cpp
consteval void assert_type_ok(const TypeResult& r) {
    if (!r.valid) {
        // GCC shows this string in the compile error
        throw "refinement type check failed";
        // Future: build detailed message with FixedString
    }
}
```

### Testing strategy

- Verify well-typed programs compile successfully
- Verify ill-typed programs produce compile errors (test via `static_assert` on the `valid` field)
- Error messages are readable (manual verification)

---

## Phase Summary

| Phase | What | Depends on | New files |
|-------|------|------------|-----------|
| 0 | `fold()` primitive | — | (core: `transforms.hpp`) |
| 1 | Type representation as AST nodes | Phase 0 | `types.hpp` |
| 2 | Type environment | Phase 1 | `type_env.hpp` |
| 3 | Constraint representation | Phase 1 | `constraints.hpp` |
| 4 | Type checker (fold-based) | Phases 1-3 | `type_check.hpp` |
| 5 | Subtype checking | Phase 3 | `subtype.hpp` |
| 6 | Constexpr SMT solver | Phase 3 | `smt/*.hpp` |
| 7 | Strip-and-compile pipeline | Phases 4-6 | `strip_types.hpp` |
| 8 | Error reporting | Phase 7 | Integrated |

## Open Questions

1. **Type inference scope** — How much inference? Start with fully-annotated (no inference), add local inference later.

2. **Environment threading** — Option A (two-pass) vs Option B (threaded fold). Recommend starting with Option A.

3. **SMT solver scope** — QF_LIA is a good starting point. How much theory is needed for useful refinements? Linear arithmetic covers most bounds-checking use cases.

4. **Interaction with existing macros** — Math macros (`MAdd`, etc.) and control-flow macros (`MCond`, etc.) need typing rules. These would be defined alongside the macros themselves, keeping the Lisp philosophy: each macro defines its own type rule.

5. **Capacity management** — Type-annotated ASTs are roughly 2x larger. Default `Cap=64` may need bumping to 128 or 256 for type-heavy programs.

6. **File organization** — All type system code under `include/refmacro/types/` subdirectory? Or flat alongside existing headers? Subdirectory keeps the "user-space library" feel.
