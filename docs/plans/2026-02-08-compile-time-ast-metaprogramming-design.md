# Compile-Time AST Metaprogramming via C++26 Reflection

**Date:** 2026-02-08
**Status:** Exploration / Design Draft

## Vision

Lisp-like metaprogramming in C++26: build, transform, and compile custom DSL ASTs entirely at compile time. Uses reflection for ergonomic variable binding, structural types for compile-time AST propagation, and template-driven codegen (or future token injection) for zero-overhead splicing back into real C++ code.

The system is **general-purpose** — not limited to math. Users define their own AST node types (via `defmacro`) and their own compilation targets (via Backends). The framework provides the build/transform/compile pipeline and uses reflection to eliminate boilerplate.

## Core Analogy

| Lisp | This System | C++26 Mechanism |
|---|---|---|
| `'(+ x (* 3 y))` (quote) | `expr([](auto x, auto y) { return x + 3 * y; })` | Operator overloading builds AST |
| Variable names | Lambda parameter names | `parameters_of()` + `identifier_of()` reflection |
| `(eval ...)` (unquote/splice) | `compile<ast, Backend>()` | Reflection-dispatched backend methods |
| `(defmacro ...)` | `defmacro<fn>()` | Reflection extracts name, arity, expansion rule |
| Macro hygiene | Scope tagging + `gensym` | Auto-scoped introduced variables |
| Macro composition | `f \| differentiate("x") \| simplify` | Pipe operator chaining `consteval` transforms |

## Architecture Overview

```
defmacro (reflection)     Backend (reflection)
    │                          │
    ▼                          ▼
┌────────┐   ┌────────────┐   ┌──────────┐
│ Build  │──▶│macroexpand │──▶│ compile  │──▶ zero-overhead code
│ (expr) │   │(fixed-point)│  │(dispatch)│    or string / eval
└────────┘   └────────────┘   └──────────┘
    ▲              ▲               ▲
    │              │               │
 operator      scope-based     tag → method
 overload +    hygiene +       via reflection
 reflection    gensym          + splice
                  │
            ┌─────┴──────┐
            │  validate  │ ← DSL-level error diagnostics
            └────────────┘
```

## AST Representation

### Unified Node Type (S-expression style)

Everything is a tagged form with children — no special cases. Primitives and macro calls have the same shape. This is the general-purpose foundation.

```cpp
struct ASTNode {
    char tag[16]{};          // "lit", "var", "add", "when", "pipeline", ...
    char type_tag[16]{};     // "double", "int", "bool", "Vec2", ... (see Type System)
    double payload{};        // optional numeric payload (for lit, etc.)
    char name[16]{};         // optional string payload (for var, etc.)
    int scope{0};            // hygiene scope tag
    int children[8]{-1};
    int child_count{0};

    // Source location tracking for diagnostics
    char file[64]{};
    int line{0};
};
```

### Flat Structural Array (hidden from users)

Structural type requirement for NTTPs means no pointers. The tree is stored as a flat array with index-based children. Users never see this.

```cpp
template<std::size_t Cap = 64>
struct AST {
    ASTNode nodes[Cap]{};
    std::size_t count{0};
    int root{-1};

    consteval int add_node(ASTNode n) {
        nodes[count] = n;
        return root = static_cast<int>(count++);
    }

    consteval int add_tagged_node(const char* tag, std::initializer_list<int> children);
    consteval int merge(const AST& other); // append other's nodes, return index offset
};
```

**Note on pointers:** `unique_ptr`/`shared_ptr` cannot be used in the final AST because structural types require scalar members. However, they CAN be used internally during `consteval` construction (C++20 transient constexpr allocation), then flattened before the result escapes.

## Stage 1: Build — AST Construction

### Expr Builder (functional, each operation returns new Expr)

```cpp
template<std::size_t Cap = 64>
struct Expr {
    AST<Cap> ast{};
    int id{-1};

    static consteval Expr lit(double v);
    static consteval Expr var(const char* name,
                              std::source_location loc = std::source_location::current());

    consteval Expr operator+(Expr rhs) const { return bin_op("add", rhs); }
    consteval Expr operator*(Expr rhs) const { return bin_op("mul", rhs); }
    consteval Expr operator-(Expr rhs) const { return bin_op("sub", rhs); }
    consteval Expr operator/(Expr rhs) const { return bin_op("div", rhs); }
    consteval Expr operator>(Expr rhs) const { return bin_op("gt", rhs); }

private:
    consteval Expr bin_op(const char* tag, Expr rhs) const {
        Expr result;
        result.ast = ast;                          // copy my nodes
        int offset = result.ast.merge(rhs.ast);    // append rhs nodes, get index offset
        result.id = result.ast.add_tagged_node(tag, {this->id, rhs.id + offset});
        return result;
    }
};

// Implicit conversion from numeric literals
consteval Expr operator+(double lhs, Expr rhs) { return Expr::lit(lhs) + rhs; }
consteval Expr operator*(double lhs, Expr rhs) { return Expr::lit(lhs) * rhs; }
// ... etc
```

### Lambda-Based Variable Binding with Reflection

Reflection on the lambda's `operator()` extracts parameter names automatically:

```cpp
template<typename F>
consteval auto expr(F f) {
    constexpr auto params = parameters_of(^^decltype(&F::operator()));
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return f(Expr::var(identifier_of(params[Is]))...);
    }(std::make_index_sequence<params.size()>{});
}

// Usage — parameter names become AST variable names:
constexpr auto formula = expr([](auto x, auto y) {
    return x * x + 2 * y;
});
// AST: add(mul(var("x"), var("x")), mul(lit(2), var("y")))
```

### Substitution and Composition

```cpp
constexpr auto norm = expr([](auto x, auto y) {
    return x * x + y * y;
});

constexpr auto shifted = subst(norm, {
    {"x", expr([](auto a) { return a - 1; })},  // x → (a - 1)
    {"y", expr([](auto b) { return b + 1; })},   // y → (b + 1)
});
// Result AST: (a-1)² + (b+1)²
```

## Stage 2: Transform — Consteval AST Rewriting

### NodeView Cursor

Ergonomic tree walking over the flat array:

```cpp
template<std::size_t Cap>
struct NodeView {
    const AST<Cap>& ast;
    int id;

    consteval auto tag()   const -> std::string_view { return ast.nodes[id].tag; }
    consteval auto child(int i) const { return NodeView{ast, ast.nodes[id].children[i]}; }
    consteval auto child_count() const { return ast.nodes[id].child_count; }
    consteval double payload()   const { return ast.nodes[id].payload; }
    consteval auto name()  const -> std::string_view { return ast.nodes[id].name; }
    consteval int scope()  const { return ast.nodes[id].scope; }
};
```

### Two Core Primitives

**1. `rewrite(expr, rule)` — bottom-up rule application until fixed point:**

```cpp
consteval Expr simplify(Expr e) {
    return rewrite(e, [](NodeView n) -> std::optional<Expr> {
        // x + 0 → x
        if (n.tag() == "add" && n.child(1).tag() == "lit"
            && n.child(1).payload() == 0)
            return to_expr(n.child(0));
        // 0 * x → 0
        if (n.tag() == "mul" && n.child(0).tag() == "lit"
            && n.child(0).payload() == 0)
            return Expr::lit(0);
        // 1 * x → x
        if (n.tag() == "mul" && n.child(0).tag() == "lit"
            && n.child(0).payload() == 1)
            return to_expr(n.child(1));
        // constant fold: lit op lit → lit
        if (n.child(0).tag() == "lit" && n.child(1).tag() == "lit")
            return Expr::lit(eval_op(n.tag(), n.child(0).payload(), n.child(1).payload()));
        return std::nullopt;
    });
}
```

**2. `transform(expr, visitor)` — structural recursion:**

```cpp
consteval Expr differentiate(Expr e, std::string_view var) {
    return transform(e, [&](NodeView n, auto recurse) -> Expr {
        if (n.tag() == "lit")
            return Expr::lit(0);
        if (n.tag() == "var")
            return Expr::lit(n.name() == var ? 1 : 0);
        if (n.tag() == "add")
            return recurse(n.child(0)) + recurse(n.child(1));
        if (n.tag() == "mul")  // product rule
            return to_expr(n.child(0)) * recurse(n.child(1))
                 + to_expr(n.child(1)) * recurse(n.child(0));
        // ...
    });
}
```

### Pipeline Composition via Pipe Operator

```cpp
consteval Expr operator|(Expr e, auto transform) { return transform(e); }

constexpr auto f  = expr([](auto x) { return x * x + 2 * x + 1; });
constexpr auto df = f | differentiate("x") | simplify;
```

## Macro System (Deferred Expansion)

### `defmacro` — Reflection-Powered Macro Definition

A `consteval` function taking `Expr` and returning `Expr` defines a macro. Reflection extracts name, arity, and expansion rule. Calling the macro builds an **unexpanded** AST node; expansion is deferred to `macroexpand`.

```cpp
// Expansion rules are plain functions
consteval Expr when_expand(Expr cond, Expr body) {
    return if_(cond, body, unit());
}

consteval Expr unless_expand(Expr cond, Expr body) {
    return if_(not_(cond), body, unit());
}

consteval Expr swap_expand(Expr a, Expr b) {
    auto tmp = var("tmp");
    return seq(assign(tmp, a), seq(assign(a, b), assign(b, tmp)));
}

// defmacro reflects on the function to produce a deferred builder
template<auto Fn>
consteval auto defmacro() {
    constexpr auto info   = ^^Fn;
    constexpr auto name   = identifier_of(info);
    constexpr auto params = parameters_of(info);

    // Detect hygiene needs: if first param is not Expr, it's `fresh`/`capture`
    constexpr bool needs_hygiene = !type_is<Expr>(type_of(params[0]));

    struct Macro {
        // Builder: creates unexpanded AST node (does NOT call expansion)
        consteval Expr operator()(auto... children) const {
            static_assert(sizeof...(children) == params.size());
            Expr result;
            // Merge all children ASTs, add tagged node
            result.id = result.ast.add_tagged_node(name, {children.id...});
            return result;
        }

        // Expansion: called by macroexpand
        static consteval Expr expand(auto... children) {
            return Fn(children...);
        }

        static constexpr auto arity = params.size();
        static constexpr auto tag   = name;
    };
    return Macro{};
}

constexpr auto when   = defmacro<when_expand>();
constexpr auto unless = defmacro<unless_expand>();
constexpr auto swap   = defmacro<swap_expand>();
```

### `macroexpand` — Fixed-Point Expansion

Walks the AST bottom-up, replacing macro nodes with their expansions. Repeats until no macros remain.

```cpp
template<auto... Macros>
consteval Expr macroexpand(Expr e) {
    bool changed = true;
    while (changed) {
        changed = false;
        e = bottom_up(e, [&](NodeView n) -> std::optional<Expr> {
            // Try each registered macro
            return [&]<auto M>() -> std::optional<Expr> {
                if (n.tag() == M.tag && n.child_count() == M.arity) {
                    changed = true;
                    return M.expand(children_as_exprs(n)...);
                }
                return std::nullopt;
            }.template operator()<Macros>()...;
        });
    }
    return e;
}
```

### Usage

```cpp
constexpr auto program = expr([](auto x, auto y) {
    return seq(
        unless(x > y, swap(x, y)),   // unexpanded macro nodes in AST
        x - y
    );
});

constexpr auto expanded = macroexpand<unless, swap>(program);
// unless → if_(not_(...), ...), swap → seq(assign, seq(assign, assign))
```

## Macro Hygiene

### The Problem

Macros that introduce internal variables can collide with user variables:

```cpp
consteval Expr swap_expand(Expr a, Expr b) {
    auto tmp = var("tmp");  // What if user also has "tmp"?
    return seq(assign(tmp, a), seq(assign(a, b), assign(b, tmp)));
}

expr([](auto tmp, auto y) {
    return swap(tmp, y);  // BUG: user's tmp collides with swap's tmp
});
```

### Solution: Scope Tagging (Automatic, Default)

Every variable carries a `scope` ID. During `macroexpand`, each expansion gets a unique scope. Variables introduced by the macro (not from input children) are auto-tagged with the expansion's scope.

```cpp
consteval Expr macroexpand(Expr e, int& scope_counter) {
    return bottom_up(e, [&](NodeView n) -> std::optional<Expr> {
        if (auto* M = find_macro(n.tag())) {
            int scope = scope_counter++;
            Expr expanded = M->expand(children...);
            // Tag NEW variables (not from children) with this scope
            return tag_introduced_vars(expanded, children, scope);
        }
        return std::nullopt;
    });
}

consteval Expr tag_introduced_vars(Expr expanded,
                                    ExprList children,
                                    int scope) {
    auto input_vars = collect_vars(children);
    return rewrite(expanded, [&](NodeView n) -> std::optional<Expr> {
        if (n.tag() == "var" && !input_vars.contains(n.name())) {
            return var_with_scope(n.name(), scope);  // auto-rename
        }
        return std::nullopt;
    });
}
```

Result: `swap`'s internal `tmp` gets `scope=7`, user's `tmp` keeps `scope=0`. No collision — different scopes, different variables. Zero burden on macro authors.

### Escape Hatch: Intentional Capture

For anaphoric macros that deliberately introduce variables into user scope (e.g., `aif` introducing `it`), the expansion function takes a `capture` parameter. Reflection detects this:

```cpp
// Anaphoric if: deliberately injects "it" into user scope
consteval Expr aif_expand(auto capture, Expr cond, Expr body) {
    auto it = capture("it");  // intentionally visible to user
    return let_in(it, cond, if_(it, body, unit()));
}

// Normal macro: auto-hygiene (no capture param)
consteval Expr swap_expand(Expr a, Expr b) { ... }

// defmacro detects the difference via reflection:
// - First param is Expr → automatic hygiene
// - First param is auto/callable → intentional capture mode
```

### `gensym` Alternative

For macros that need guaranteed-unique names without scope tagging, a `fresh` function generates unique variable names within a single `macroexpand` pass:

```cpp
consteval Expr swap_expand(auto fresh, Expr a, Expr b) {
    auto tmp = fresh("tmp");  // → var("__swap_tmp_0007")
    return seq(assign(tmp, a), seq(assign(a, b), assign(b, tmp)));
}
```

**Recommendation:** Scope tagging as the default (zero burden). `capture` for intentional hygiene-breaking. `fresh`/`gensym` available as explicit alternative.

## Stage 3: Compile — Backend-Driven Codegen

### The Problem

In the math-only design, codegen was a hardcoded `if constexpr` chain (`Add` → `+`, `Mul` → `*`). For general-purpose, the user defines their own compilation targets.

### Solution: Backend Structs with Reflection-Based Dispatch

A **Backend** is a struct with methods named after AST tags. Reflection dispatches by matching tag name → method name. No registration, no mapping tables.

```cpp
struct MathBackend {
    static consteval auto lit(double value) {
        return [=](auto...) { return value; };
    }
    static consteval auto add(auto lhs, auto rhs) {
        return [=](auto... args) { return lhs(args...) + rhs(args...); };
    }
    static consteval auto mul(auto lhs, auto rhs) {
        return [=](auto... args) { return lhs(args...) * rhs(args...); };
    }
    static consteval auto neg(auto x) {
        return [=](auto... args) { return -x(args...); };
    }
};
```

### Generic `compile` with Reflection Dispatch

```cpp
template<auto ast, int id, auto var_map, typename Backend>
consteval auto compile_node() {
    constexpr auto n = ast.nodes[id];

    // Base cases
    if constexpr (equals(n.tag, "lit")) {
        return Backend::lit(n.payload);
    }
    else if constexpr (equals(n.tag, "var")) {
        constexpr int idx = var_map.index_of(n.name);
        return [](auto... args) { return std::get<idx>(std::tie(args...)); };
    }
    else {
        // Compile children recursively
        auto compiled_children = [&]<int... Cs>(std::integer_sequence<int, Cs...>) {
            return std::tuple{
                compile_node<ast, n.children[Cs], var_map, Backend>()...
            };
        }(std::make_integer_sequence<int, n.child_count>{});

        // Reflection: find Backend method matching n.tag
        constexpr auto method = find_named_member(^^Backend, n.tag);

        // Splice + apply: Backend::add(compiled_lhs, compiled_rhs)
        return std::apply([:method:], compiled_children);
    }
}

template<auto e, typename Backend = LambdaBackend>
constexpr auto compile() {
    constexpr auto vm = extract_var_map(e.ast);

    // Run validation before codegen (see Compile-Time Diagnostics section)
    validate<e.ast, vm, Backend>();

    return compile_node<e.ast, e.id, vm, Backend>();
}
```

### Multiple Backends for the Same AST

```cpp
// Backend 1: Compile to lambdas (zero-overhead runtime code)
struct LambdaBackend {
    static consteval auto add(auto l, auto r) {
        return [=](auto... a) { return l(a...) + r(a...); };
    }
    static consteval auto mul(auto l, auto r) {
        return [=](auto... a) { return l(a...) * r(a...); };
    }
};

// Backend 2: Compile to string (pretty-printer / debugger)
struct PrintBackend {
    static consteval auto lit(double v) { return to_string(v); }
    static consteval auto var(const char* name) { return name; }
    static consteval auto add(auto l, auto r) { return concat("(", l, " + ", r, ")"); }
    static consteval auto mul(auto l, auto r) { return concat("(", l, " * ", r, ")"); }
};

// Backend 3: Direct consteval evaluation
struct EvalBackend {
    static consteval double add(double l, double r) { return l + r; }
    static consteval double mul(double l, double r) { return l * r; }
};
```

```cpp
constexpr auto f = expr([](auto x, auto y) { return x * x + 2 * y; });
constexpr auto df = f | differentiate("x") | simplify;

// Same AST, three different outputs:
constexpr auto fn  = compile<df, LambdaBackend>();       // runtime-callable lambda
constexpr auto str = compile<df, PrintBackend>();         // "(x + x)"
constexpr auto val = compile<df, EvalBackend>(3.0, 4.0); // 6.0
```

### Non-Math Backend Example: Control Flow DSL

```cpp
struct ControlFlowBackend {
    static consteval auto if_(auto cond, auto then_, auto else_) {
        return [=](auto... args) {
            return cond(args...) ? then_(args...) : else_(args...);
        };
    }
    static consteval auto seq(auto first, auto second) {
        return [=](auto... args) { first(args...); return second(args...); };
    }
    static consteval auto assign(auto target, auto value) {
        return [=](auto&... args) { target(args...) = value(args...); };
    }
    static consteval auto gt(auto l, auto r) {
        return [=](auto... args) { return l(args...) > r(args...); };
    }
};
```

### Future: P3294 Token Injection for Direct Splicing

```cpp
consteval auto splice(auto ast, int id) -> std::meta::info {
    constexpr auto n = ast.nodes[id];
    if constexpr (equals(n.tag, "lit"))
        return ^^{ [: n.payload :] };
    if constexpr (equals(n.tag, "add"))
        return ^^{ [: splice(ast, n.lhs) :] + [: splice(ast, n.rhs) :] };
    // ...
}
double result = [: splice(df.ast, df.id) :];
```

## Compile-Time Diagnostics

### Design Goal

When users write malformed DSL code, they should see **DSL-level errors** — not cryptic template backtraces. In `consteval` context, throwing produces a compile-time error with a clean message.

**Bad (template noise):**
```
error: no matching call to 'std::get<2>' ...
  in instantiation of 'codegen<AST{...}, 3, VarMap{...}>' ...
  ... 40 lines of template noise ...
```

**Good (DSL-level):**
```
error: unbound variable 'z' in expression:
  (x + (z * y))
        ^
  bound variables: {x, y}
  defined at: main.cpp:42
```

### Source Location Tracking

Every AST node records where in the user's source it was created via `std::source_location`:

```cpp
static consteval Expr var(const char* name,
                          std::source_location loc = std::source_location::current()) {
    Expr e;
    ASTNode node{};
    copy_str(node.tag, "var");
    copy_str(node.name, name);
    copy_str(node.file, loc.file_name());
    node.line = loc.line();
    e.id = e.ast.add_node(node);
    return e;
}
```

### AST Pretty-Printer

Foundation for all error messages — renders the AST as a human-readable expression:

```cpp
consteval auto pretty_print(const AST& ast, int id) -> FixedString<256> {
    auto n = ast.nodes[id];
    if (equals(n.tag, "lit"))  return to_fixed_string(n.payload);
    if (equals(n.tag, "var"))  return FixedString(n.name);
    if (is_infix(n.tag)) {     // add, mul, sub, div, gt
        return concat("(",
            pretty_print(ast, n.children[0]),
            " ", infix_symbol(n.tag), " ",  // "add"→"+", "mul"→"*"
            pretty_print(ast, n.children[1]),
        ")");
    }
    // Generic: tag(child, child, ...)
    return concat(n.tag, "(",
        join(", ", pretty_print_children(ast, n)),
    ")");
}
```

### Error Formatter

Combines category, detail, expression context, and source location:

```cpp
consteval auto format_error(
    const char* category,        // "unbound variable", "arity mismatch", etc.
    const char* detail,          // the specific problem
    const AST& ast, int node_id  // which node caused it
) -> FixedString<512> {
    auto n = ast.nodes[node_id];
    return concat(
        category, ": ", detail, "\n",
        "  in expression: ", pretty_print(ast, ast.root), "\n",
        "  at node: ", pretty_print(ast, node_id), "\n",
        "  defined at: ", n.file, ":", to_string(n.line)
    );
}
```

### Validation Passes

Validation runs **before codegen**, so errors speak the DSL language, not template language.

**1. Unbound variables:**

```cpp
template<auto ast, auto var_map>
consteval void check_unbound_vars() {
    for (int i = 0; i < ast.count; ++i) {
        if (equals(ast.nodes[i].tag, "var")) {
            if (!var_map.contains(ast.nodes[i].name)) {
                throw format_error(
                    "unbound variable",
                    concat("'", ast.nodes[i].name, "'"
                           " is not in scope. bound variables: ",
                           var_map.names_string()),
                    ast, i
                );
            }
        }
    }
}
```

**2. Macro arity mismatch (during `macroexpand`):**

```cpp
template<auto M>
consteval void check_macro_arity(NodeView n) {
    if (n.child_count() != M.arity) {
        throw format_error(
            "macro arity mismatch",
            concat("'", M.tag, "' expects ", to_string(M.arity),
                   " arguments, got ", to_string(n.child_count())),
            n.ast, n.id
        );
    }
}
```

**3. Backend completeness (during `compile`):**

```cpp
template<typename Backend, auto ast>
consteval void check_backend_coverage() {
    constexpr auto methods = members_of(^^Backend);
    for (int i = 0; i < ast.count; ++i) {
        auto tag = ast.nodes[i].tag;
        if (equals(tag, "lit") || equals(tag, "var")) continue;
        if (!has_named_member(methods, tag)) {
            throw format_error(
                "backend missing handler",
                concat("'", type_name<Backend>(), "' has no method '",
                       tag, "'. add: static consteval auto ",
                       tag, "(...) { ... }"),
                ast, i
            );
        }
    }
}
```

**4. Macro expansion depth (circular macros):**

```cpp
template<auto... Macros>
consteval Expr macroexpand(Expr e, int max_depth = 100) {
    int depth = 0;
    bool changed = true;
    while (changed) {
        if (++depth > max_depth) {
            throw concat(
                "macro expansion depth exceeded (", to_string(max_depth), ")\n",
                "  likely circular expansion involving: ",
                find_unexpanded_tags(e), "\n",
                "  current expression: ", pretty_print(e.ast, e.id)
            );
        }
        // ... expand ...
    }
    return e;
}
```

### Unified Validation Entry Point

Wired into `compile()` automatically — users never call it manually:

```cpp
template<auto ast, auto var_map, typename Backend>
consteval void validate() {
    check_unbound_vars<ast, var_map>();
    check_backend_coverage<Backend, ast>();
}

template<auto e, typename Backend = LambdaBackend>
constexpr auto compile() {
    constexpr auto vm = extract_var_map(e.ast);
    validate<e.ast, vm, Backend>();           // ← errors fire here, before codegen
    return compile_node<e.ast, e.id, vm, Backend>();
}
```

### What Users See — Concrete Error Examples

```cpp
// 1. Unbound variable
constexpr auto f = expr([](auto x, auto y) { return x + z * y; });
constexpr auto fn = compile<f>();
// COMPILE ERROR:
//   unbound variable: 'z' is not in scope. bound variables: {x, y}
//     in expression: (x + (z * y))
//     at node: z
//     defined at: main.cpp:42

// 2. Macro arity mismatch
constexpr auto bad = when(x > lit(0));  // missing second arg
// COMPILE ERROR:
//   macro arity mismatch: 'when' expects 2 arguments, got 1
//     in expression: when((x > 0))
//     at node: when((x > 0))
//     defined at: main.cpp:45

// 3. Incomplete backend
struct MyBackend {
    static consteval auto add(auto l, auto r) { /*...*/ }
    // forgot mul!
};
compile<f, MyBackend>();
// COMPILE ERROR:
//   backend missing handler: 'MyBackend' has no method 'mul'.
//     add: static consteval auto mul(...) { ... }
//     in expression: ((x * x) + 1)
//     at node: (x * x)
//     defined at: main.cpp:48

// 4. Circular macros
consteval Expr a_expand(Expr x) { return b(x); }
consteval Expr b_expand(Expr x) { return a(x); }
macroexpand<a, b>(some_expr);
// COMPILE ERROR:
//   macro expansion depth exceeded (100)
//     likely circular expansion involving: {a, b}
//     current expression: a(b(a(b(...))))
```

Every error speaks the DSL language, points to the user's source location, and often suggests a fix. No template backtraces.

## Type System

### Design: Typed Surface, Untyped Middle

Three layers with different type granularity:

```
Typed builder surface          →  Expr<double>, Expr<bool>, Expr<Vec2>
    ↓ (erase to type_tag string)
Untyped AST storage + transforms  →  ASTNode with type_tag="double"
    ↓ (restore types via Backend)
Typed codegen output           →  (double, bool) -> double
```

**Rationale:** Transforms (simplification, differentiation, macro expansion) are structural — they don't care about types. Forcing types through them adds complexity for no benefit. But the user-facing API should be typed to catch errors at the point of construction, and codegen produces typed output.

### Typed Expr Builder

`Expr<T>` carries a phantom type parameter. C++ type checking enforces DSL type rules at build time:

```cpp
template<typename T>
struct Expr {
    AST ast{};
    int id{-1};

    static consteval Expr<T> lit(T v);
    static consteval Expr<T> var(const char* name,
                                  std::source_location loc = std::source_location::current());

    // Arithmetic: same type in, same type out
    consteval Expr<T> operator+(Expr<T> rhs) const
        requires std::is_arithmetic_v<T>
    { return bin_op("add", rhs); }

    consteval Expr<T> operator*(Expr<T> rhs) const
        requires std::is_arithmetic_v<T>
    { return bin_op("mul", rhs); }

    // Comparison: arithmetic in, bool out
    consteval Expr<bool> operator>(Expr<T> rhs) const
        requires std::is_arithmetic_v<T>
    { return comparison_op("gt", rhs); }

    consteval Expr<bool> operator==(Expr<T> rhs) const
    { return comparison_op("eq", rhs); }
};

// Cross-type promotion: int * double → double
consteval Expr<double> operator*(Expr<int> lhs, Expr<double> rhs) {
    return typed_bin_op<double>("mul", promote(lhs), rhs);
}

// Control flow: condition must be bool, branches must match
template<typename T>
consteval Expr<T> if_(Expr<bool> cond, Expr<T> then_, Expr<T> else_);
```

**Type errors the user sees:**

```cpp
auto x = Expr<double>::var("x");
auto flag = Expr<bool>::var("flag");

auto a = x + x;              // OK: Expr<double>
auto b = x > x;              // OK: Expr<bool>
auto c = x + flag;           // COMPILE ERROR: no operator+(Expr<double>, Expr<bool>)
auto d = if_(x, x, x);       // COMPILE ERROR: condition must be Expr<bool>
auto e = if_(flag, x, flag);  // COMPILE ERROR: branches have different types
```

### Typed `expr()` with Reflection

When parameters have explicit types, `expr()` reflects on both name and type:

```cpp
// Typed: parameter types specify DSL types
constexpr auto f = expr([](Expr<double> x, Expr<bool> flag) {
    return if_(flag, x * x, Expr<double>::lit(0.0));
});

// Untyped: auto parameters default to Expr<double> (backward compatible)
constexpr auto g = expr([](auto x, auto y) {
    return x * x + y * y;
});

// expr() reflects on both name and type:
template<typename F>
consteval auto expr(F f) {
    constexpr auto params = parameters_of(^^decltype(&F::operator()));
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return f(
            make_typed_var<[:type_of(params[Is]):]>(
                identifier_of(params[Is])
            )...
        );
    }(std::make_index_sequence<params.size()>{});
}
```

### Type Erasure for Transforms

Transforms operate on `ErasedExpr` (the untyped AST). Type information lives in `type_tag` strings but doesn't constrain the transform logic:

```cpp
// Type-erasing wrapper
template<typename T>
consteval ErasedExpr erase(Expr<T> e) {
    return ErasedExpr{e.ast, e.id};  // type info already in type_tag
}

// Type-restoring wrapper (validates type_tag matches T)
template<typename T>
consteval Expr<T> restore(ErasedExpr e) {
    if (!equals(e.ast.nodes[e.id].type_tag, type_name<T>()))
        throw format_error("type mismatch after transform", ...);
    return Expr<T>{e.ast, e.id};
}

// Pipe operator handles erase/restore automatically:
template<typename T>
consteval auto operator|(Expr<T> e, auto transform) {
    return restore<T>(transform(erase(e)));
}

// Transforms are unchanged — they work on ErasedExpr:
consteval ErasedExpr simplify(ErasedExpr e) {
    return rewrite(e, [](NodeView n) -> std::optional<ErasedExpr> {
        // Same structural rules as before, type-agnostic
        if (n.tag() == "add" && n.child(1).tag() == "lit"
            && n.child(1).payload() == 0)
            return to_expr(n.child(0));
        // ...
    });
}
```

### Type Validation Pass

Catches type errors that slip through transforms (e.g., a rewrite rule produces inconsistent types):

```cpp
template<auto ast>
consteval void check_types() {
    for (int i = 0; i < ast.count; ++i) {
        auto n = ast.nodes[i];
        if (equals(n.tag, "add") || equals(n.tag, "mul")) {
            auto lhs_type = ast.nodes[n.children[0]].type_tag;
            auto rhs_type = ast.nodes[n.children[1]].type_tag;
            if (!equals(lhs_type, rhs_type)) {
                throw format_error(
                    "type mismatch",
                    concat("'", n.tag, "' operands: ",
                           lhs_type, " vs ", rhs_type),
                    ast, i
                );
            }
        }
        if (equals(n.tag, "if_")) {
            if (!equals(ast.nodes[n.children[0]].type_tag, "bool")) {
                throw format_error(
                    "type mismatch",
                    concat("'if_' condition must be bool, got ",
                           ast.nodes[n.children[0]].type_tag),
                    ast, i
                );
            }
        }
    }
}
```

### User-Defined DSL Types

Users register custom types for their domain. The type only needs a name (for `type_tag`) — the actual C++ representation is the Backend's concern.

```cpp
// Declare DSL types (tag types — no data, just identity)
struct Vec2 {};
struct Matrix {};
struct Color {};

// Extend Expr with domain-specific operations
consteval Expr<double> dot(Expr<Vec2> a, Expr<Vec2> b) {
    return typed_op<double>("dot", a, b);  // Vec2 × Vec2 → double
}

consteval Expr<Vec2> operator+(Expr<Vec2> a, Expr<Vec2> b) {
    return typed_op<Vec2>("vec_add", a, b);
}

consteval Expr<double> length(Expr<Vec2> v) {
    return typed_op<double>("length", v);  // Vec2 → double
}

consteval Expr<Vec2> normalize(Expr<Vec2> v) {
    return typed_op<Vec2>("normalize", v);  // Vec2 → Vec2
}

consteval Expr<Vec2> scale(Expr<double> s, Expr<Vec2> v) {
    return typed_op<Vec2>("scale", s, v);  // double × Vec2 → Vec2
}

// Type-safe usage:
constexpr auto reflect = expr([](Expr<Vec2> v, Expr<Vec2> normal) {
    return v - scale(2.0 * dot(v, normal), normal);
});
// Type errors:
// dot(v, Expr<double>::lit(1.0))  → ERROR: dot expects Vec2, got double
// v + Expr<double>::lit(1.0)      → ERROR: no operator+(Expr<Vec2>, Expr<double>)

// Backend provides the actual implementation:
struct GraphicsBackend {
    static consteval auto vec_add(auto l, auto r) {
        return [=](auto... args) {
            auto a = l(args...); auto b = r(args...);
            return Vec2{a.x + b.x, a.y + b.y};
        };
    }
    static consteval auto dot(auto l, auto r) {
        return [=](auto... args) {
            auto a = l(args...); auto b = r(args...);
            return a.x * b.x + a.y * b.y;
        };
    }
    static consteval auto scale(auto s, auto v) {
        return [=](auto... args) {
            auto sc = s(args...); auto vec = v(args...);
            return Vec2{sc * vec.x, sc * vec.y};
        };
    }
    static consteval auto normalize(auto v) {
        return [=](auto... args) {
            auto vec = v(args...);
            auto len = std::sqrt(vec.x * vec.x + vec.y * vec.y);
            return Vec2{vec.x / len, vec.y / len};
        };
    }
    // ...
};
```

### Reflection-Driven Type Registration

Instead of manually defining each operation, reflection can auto-generate typed DSL operations from a type descriptor:

```cpp
// User describes the type's operations declaratively:
struct Vec2Ops {
    using self = Vec2;

    // Binary ops: (self, self) -> result
    static auto add(Vec2 a, Vec2 b) -> Vec2;
    static auto sub(Vec2 a, Vec2 b) -> Vec2;
    static auto dot(Vec2 a, Vec2 b) -> double;

    // Unary ops: self -> result
    static auto length(Vec2 v) -> double;
    static auto normalize(Vec2 v) -> Vec2;

    // Mixed ops: (other, self) -> result
    static auto scale(double s, Vec2 v) -> Vec2;
};

// register_type reflects on Vec2Ops and auto-generates:
// - Expr<Vec2> operator+(Expr<Vec2>, Expr<Vec2>)  (from add)
// - Expr<double> dot(Expr<Vec2>, Expr<Vec2>)      (from dot)
// - Expr<Vec2> scale(Expr<double>, Expr<Vec2>)    (from scale)
// - etc.
constexpr auto vec2_ops = register_type<Vec2Ops>();
```

```cpp
template<typename TypeOps>
consteval auto register_type() {
    constexpr auto methods = members_of(^^TypeOps);
    // For each method, reflect on:
    //   - identifier_of(m) → operation name ("dot", "scale", ...)
    //   - parameters_of(m) → parameter types → Expr<T> wrappers
    //   - return_type_of(m) → result type → Expr<R>
    // Generate the typed Expr operation function
    // (via define_class or manual registration)
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return TypeOpSet{
            make_typed_op<methods[Is]>()...
        };
    }(std::make_index_sequence<methods.size()>{});
}
```

## End-to-End Example

```cpp
// --- Define primitives ---
consteval Expr if_(Expr c, Expr t, Expr f) { /* builds "if_" node */ }
consteval Expr seq(Expr a, Expr b)          { /* builds "seq" node */ }
consteval Expr assign(Expr var, Expr val)   { /* builds "assign" node */ }

// --- Define macros ---
consteval Expr unless_expand(Expr cond, Expr body) {
    return if_(not_(cond), body, unit());
}
consteval Expr swap_expand(Expr a, Expr b) {
    auto tmp = var("tmp");  // auto-scoped by hygiene
    return seq(assign(tmp, a), seq(assign(a, b), assign(b, tmp)));
}
constexpr auto unless = defmacro<unless_expand>();
constexpr auto swap   = defmacro<swap_expand>();

// --- Build ---
constexpr auto program = expr([](auto x, auto y) {
    return seq(
        unless(x > y, swap(x, y)),
        x - y
    );
});

// --- Expand macros ---
constexpr auto expanded = macroexpand<unless, swap>(program);

// --- Compile (validation runs automatically) ---
constexpr auto fn = compile<expanded, ControlFlowBackend>();

// --- Run (zero overhead) ---
double result = fn(3.0, 7.0);
```

## Key Design Decisions

- **Unified S-expression nodes**: all nodes are tagged forms with children — primitives and macros have the same shape, enabling a single generic pipeline
- **Flat array AST**: structural type requirement for NTTPs; pointer-based trees allowed internally during consteval, flattened before escaping
- **Functional Expr builder**: each operation returns a new Expr (no shared mutable state), works naturally in consteval
- **Reflection for variable names**: `parameters_of()` + `identifier_of()` on lambda's `operator()` eliminates explicit `Var("x")` boilerplate
- **Deferred macro expansion**: `defmacro` builds unexpanded nodes; `macroexpand` is a separate fixed-point pass, enabling inspection/transformation of unexpanded macros
- **Automatic hygiene via scope tagging**: introduced variables get unique scope IDs; intentional capture available as opt-in escape hatch
- **Backend-driven codegen**: tag → method dispatch via reflection; same AST compiles to lambdas, strings, or direct values
- **Two transform primitives**: `rewrite` (local pattern rules, bottom-up fixed-point) and `transform` (structural recursion with callback)
- **Validation before codegen**: DSL-level errors fire before template instantiation, producing clean messages with source locations and expression context instead of template backtraces
- **Typed surface, untyped middle**: `Expr<T>` catches type errors at build time; transforms work on type-erased AST with `type_tag` strings; codegen restores types via Backend
- **User-defined DSL types**: tag types + operation declarations; reflection auto-generates typed Expr operations from a type descriptor struct

## Open Questions

- **Capacity management**: fixed `Cap = 64` vs. growing strategy for large ASTs?
- **Multi-error collection**: fail on first error vs. collect all errors and report together?
- **Performance**: compile-time impact of fixed-point macro expansion and bottom-up rewriting on large ASTs?
- **P2996 coverage**: which parts of this design require reflection features beyond current P2996 (e.g., expression-level reflection, `define_class`, token injection)?
- **Debugging**: how to provide a step-through macro expansion debugger at compile time (trace each expansion step)?
- **Type inference**: can we infer result types of user-defined operations from the operation descriptor, rather than requiring explicit `typed_op<Result>` calls?
- **Generic types**: how to handle parameterized DSL types like `Array<T, N>` or `Optional<T>` in the type tag system?
- **Type classes / overloading**: how to express that multiple types support the same operation (e.g., `+` works for `double`, `Vec2`, `Matrix`) without per-type overloads?
