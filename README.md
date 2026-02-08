# refmacro

A header-only C++26 compile-time AST metaprogramming framework with Lisp-like macros, powered by GCC reflection.

## Features

- **Lisp-like macro system**: `defmacro(tag, lower_fn)` defines AST node types with deferred lowering
- **Compile-time AST construction**: Build expression trees with `var()`, `lit()`, operator overloads, and `make_node()`
- **Symbolic differentiation**: `differentiate(expr, "x")` with algebraic `simplify()`
- **Pretty printing**: Consteval AST-to-string rendering
- **Tree transforms**: `rewrite()` (fixed-point) and `transform()` (structural recursion) primitives
- **Pipe operator**: Chain transforms with `expr | differentiate | simplify`
- **Extensible**: Define custom DSL nodes — everything beyond `var` and `lit` is user-defined via macros

## Requirements

- GCC 16+ with C++26 reflection support (`-freflection`)
- CMake 3.20+

## Quick Start

```cpp
#include <refmacro/refmacro.hpp>
using namespace refmacro;

// Build an expression: f(x) = x^2 + 2x + 1
constexpr auto x = Expr<>::var("x");
constexpr auto f = x * x + 2.0 * x + 1.0;

// Pretty-print at compile time
static_assert(pretty_print(f) == "(((x * x) + (2 * x)) + 1)");

// Compile to a callable lambda
constexpr auto fn = math_compile<f>();
static_assert(fn(3.0) == 16.0);  // (3+1)^2 = 16
```

## Defining Custom Macros

```cpp
constexpr auto Abs = defmacro("abs", [](auto x) {
    return [=](auto... a) constexpr {
        auto v = x(a...);
        return v < 0 ? -v : v;
    };
});

constexpr auto e = Abs(Expr<>::var("x"));
constexpr auto fn = compile<e, Abs>();
static_assert(fn(-3.0) == 3.0);
```

## Symbolic Differentiation

```cpp
constexpr auto x = Expr<>::var("x");
constexpr auto f = x * x * x;

constexpr auto diff_x = [](Expr<> e) consteval { return differentiate(e, "x"); };
constexpr auto simp   = [](Expr<> e) consteval { return simplify(e); };

constexpr auto df  = f | diff_x | simp;   // f'(x)  = 3x^2
constexpr auto d2f = df | diff_x | simp;  // f''(x) = 6x
```

## Building a Custom DSL

```cpp
constexpr auto Gt = defmacro("gt", [](auto lhs, auto rhs) {
    return [=](auto... a) constexpr {
        return lhs(a...) > rhs(a...) ? 1.0 : 0.0;
    };
});

constexpr auto If = defmacro("if_", [](auto cond, auto then_br, auto else_br) {
    return [=](auto... a) constexpr {
        return cond(a...) != 0.0 ? then_br(a...) : else_br(a...);
    };
});

consteval Expr<> gt(Expr<> lhs, Expr<> rhs) { return make_node("gt", lhs, rhs); }
consteval Expr<> if_(Expr<> c, Expr<> t, Expr<> e) { return make_node("if_", c, t, e); }

// relu(x) = if_(gt(x, 0), x, 0)
constexpr auto x = Expr<>::var("x");
constexpr auto relu_expr = if_(gt(x, Expr<>::lit(0.0)), x, Expr<>::lit(0.0));
constexpr auto relu = compile<relu_expr, MAdd, MSub, MMul, MDiv, MNeg, Gt, If>();
static_assert(relu(-5.0) == 0.0);
static_assert(relu(3.0) == 3.0);
```

## Architecture

| Header | Description |
|--------|-------------|
| `ast.hpp` | `ASTNode`, `AST<Cap>`, consteval string utilities |
| `expr.hpp` | `Expr<Cap>`, `lit()`, `var()`, `make_node()`, pipe operator |
| `macro.hpp` | `defmacro()`, `Macro` type |
| `compile.hpp` | `compile<expr, macros...>()`, `VarMap` |
| `node_view.hpp` | `NodeView` cursor for tree walking |
| `transforms.hpp` | `rewrite()`, `transform()` primitives |
| `pretty_print.hpp` | Consteval AST rendering |
| `math.hpp` | Math macros, operators, `simplify()`, `differentiate()` |
| `refmacro.hpp` | Umbrella include |

## Building

With a GCC build tree (uninstalled):

```bash
cmake -B build --toolchain cmake/xg++-toolchain.cmake \
      -DGCC_BUILD_DIR=/path/to/gcc/build/gcc
cmake --build build
ctest --test-dir build --output-on-failure
```

With an installed GCC 16+:

```bash
cmake -B build -DCMAKE_CXX_COMPILER=g++-16
cmake --build build
ctest --test-dir build --output-on-failure
```

With Docker:

```bash
docker build -t refmacro .
docker run refmacro
```

## License

MIT License
