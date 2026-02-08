# CLAUDE.md

## Build & Test

```bash
# Configure with GCC build-tree xg++ (uninstalled)
cmake -B build --toolchain cmake/xg++-toolchain.cmake \
      -DGCC_BUILD_DIR=/path/to/gcc/build/gcc

# Or with an installed GCC 16+
cmake -B build -DCMAKE_CXX_COMPILER=g++-16

# Build and test
cmake --build build
ctest --test-dir build --output-on-failure
```

Requires GCC 16+ with C++26 reflection support. `-freflection` added automatically.

## Architecture

Header-only C++26 compile-time AST metaprogramming framework. `refmacro::` namespace.

Core primitive: `defmacro(tag, lower_fn)` — defines an AST node type and its compilation.
`compile<expr, macros...>()` walks the AST and applies macro lowerings bottom-up.
Framework built-ins: `var` (argument lookup), `lit` (constant value).
Everything else is user-defined via macros.

### Headers

- `ast.hpp` — ASTNode, AST<Cap>, consteval string utilities
- `expr.hpp` — Expr, lit(), var(), make_node(), arithmetic/comparison/logical operator overloads
- `macro.hpp` — defmacro(), Macro type
- `compile.hpp` — compile<expr, macros...>(), VarMap, Scope, TagStr
- `control.hpp` — Control-flow macros (MCond, comparisons, logical), lambda(), apply(), let_(), full_compile<>()
- `node_view.hpp` — NodeView cursor for tree walking
- `transforms.hpp` — rewrite(), transform() primitives
- `pretty_print.hpp` — consteval AST rendering
- `math.hpp` — Math macros, operators, simplify, differentiate
- `refmacro.hpp` — umbrella include
