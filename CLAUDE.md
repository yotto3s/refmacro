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

Core primitive: `defmacro<"tag">(lower_fn)` — defines an AST node type, its compilation, and a callable that auto-tracks the macro in Expression's type.
`compile<expr>()` walks the AST, auto-discovers macros embedded in the Expression type, and applies macro lowerings bottom-up. Extra macros can still be passed explicitly via `compile<expr, macros...>()`.
Framework built-ins: `var` (argument lookup), `lit` (constant value).
Everything else is user-defined via macros.

### Headers

- `ast.hpp` — ASTNode, AST<Cap>, consteval string utilities
- `expr.hpp` — Expression<Cap, auto... Macros>, lit(), var(), make_node()
- `macro.hpp` — FixedString, MacroSpec, MacroCaller, defmacro<"tag">(fn)
- `compile.hpp` — compile<expr>(), unified_compiler, VarMap, Scope, TagStr
- `control.hpp` — Control-flow macros (MCond, comparisons, logical), lambda(), apply(), let_(), full_compile<>()
- `node_view.hpp` — NodeView cursor for tree walking
- `transforms.hpp` — rewrite(), transform(), fold() primitives (accept Expression<Cap, Ms...>)
- `pretty_print.hpp` — consteval AST rendering (PrintBuffer)
- `math.hpp` — Math macros, auto-tracking operators, simplify, differentiate
- `refmacro.hpp` — umbrella include
