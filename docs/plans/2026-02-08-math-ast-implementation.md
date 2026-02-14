# Compile-Time AST Metaprogramming — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement a generic compile-time AST framework where `defmacro` is the core primitive — defining both how AST nodes are constructed and how they are lowered (compiled). Math is the first domain, plugged in via macros. Analysis/transform passes layer on after.

**Architecture:** Like Lisp: `defmacro(tag, lower_fn)` defines a new AST node type and its deferred lowering. Calling a macro during expression building creates an AST node (no lowering yet). `compile<expr, macros...>()` walks the AST bottom-up and applies each macro's lowering function to produce zero-overhead lambdas. The framework handles only two built-ins: `var` (argument lookup) and `lit` (constant value). Everything else — `add`, `mul`, `neg`, `if_`, `seq`, any DSL operation — is a user-defined macro.

**Tech Stack:** C++26, GCC 16+ with `-freflection`, CMake 3.20+, GoogleTest 1.15.2

**Reference project:** `/home/yotto/dev/github/yotto3s/refinery` — identical compiler/toolchain setup. Copy `cmake/xg++-toolchain.cmake` verbatim.

**Key design decisions:**
- `ASTNode` is structural (all scalar/array members) → works as NTTP
- `Macro<CompileFn>` stores a stateless lambda → structural → works as NTTP
- `compile<expr, macros...>()` dispatches by matching node tag to macro tag
- `var` and `lit` are built-in; all other tags go through macros
- Macro lowering functions receive compiled children (lambdas), return lambdas
- Non-terminal expansion (AST→AST macros) is a later layer on top

---

## Task 1: Project Scaffold

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/xg++-toolchain.cmake`
- Create: `include/refmacro/ast.hpp` (empty placeholder)
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_ast.cpp` (minimal smoke test)
- Create: `CLAUDE.md`

**Step 1: Create directory structure**

```bash
mkdir -p include/refmacro cmake tests docs/plans
```

**Step 2: Create `cmake/xg++-toolchain.cmake`**

Copy verbatim from `/home/yotto/dev/github/yotto3s/refinery/cmake/xg++-toolchain.cmake`.

**Step 3: Create `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20)
project(refmacro
    VERSION 0.1.0
    DESCRIPTION "C++26 Compile-Time AST Metaprogramming Framework"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(REFMACRO_BUILD_TESTS "Build test suite" ON)

add_library(refmacro INTERFACE)
add_library(refmacro::refmacro ALIAS refmacro)

target_include_directories(refmacro INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(refmacro INTERFACE -freflection)
endif()

if(REFMACRO_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()
```

**Step 4: Create `tests/CMakeLists.txt`**

```cmake
add_executable(test_ast test_ast.cpp)
target_link_libraries(test_ast PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_ast PRIVATE -Wall -Wextra -Werror)

include(GoogleTest)
gtest_discover_tests(test_ast PROPERTIES TIMEOUT 60)
```

**Step 5: Create minimal `include/refmacro/ast.hpp`**

```cpp
#ifndef REFMACRO_AST_HPP
#define REFMACRO_AST_HPP

namespace refmacro {
} // namespace refmacro

#endif // REFMACRO_AST_HPP
```

**Step 6: Create minimal `tests/test_ast.cpp`**

```cpp
#include <refmacro/ast.hpp>
#include <gtest/gtest.h>

TEST(Smoke, HeaderCompiles) {
    SUCCEED();
}
```

**Step 7: Create `CLAUDE.md`**

```markdown
# CLAUDE.md

## Build & Test

\```bash
# Configure with GCC build-tree xg++ (uninstalled)
cmake -B build --toolchain cmake/xg++-toolchain.cmake \
      -DGCC_BUILD_DIR=/path/to/gcc/build/gcc

# Or with an installed GCC 16+
cmake -B build -DCMAKE_CXX_COMPILER=g++-16

# Build and test
cmake --build build
ctest --test-dir build --output-on-failure
\```

Requires GCC 16+ with C++26 reflection support. `-freflection` added automatically.

## Architecture

Header-only C++26 compile-time AST metaprogramming framework. `refmacro::` namespace.

Core primitive: `defmacro(tag, lower_fn)` — defines an AST node type and its compilation.
`compile<expr, macros...>()` walks the AST and applies macro lowerings bottom-up.
Framework built-ins: `var` (argument lookup), `lit` (constant value).
Everything else is user-defined via macros.

### Headers

- `ast.hpp` — ASTNode, AST<Cap>, consteval string utilities
- `expr.hpp` — Expr, lit(), var(), make_node()
- `macro.hpp` — defmacro(), Macro type
- `compile.hpp` — compile<expr, macros...>(), VarMap
- `node_view.hpp` — NodeView cursor for tree walking
- `transforms.hpp` — rewrite(), transform() primitives
- `pretty_print.hpp` — consteval AST rendering
- `math.hpp` — Math macros, operators, simplify, differentiate
- `refmacro.hpp` — umbrella include
```

**Step 8: Build and verify**

Run: `cmake -B build --toolchain cmake/xg++-toolchain.cmake -DGCC_BUILD_DIR=<path> && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: Build succeeds, 1 test passes.

**Step 9: Commit**

```bash
git init
git add CMakeLists.txt cmake/ include/ tests/ CLAUDE.md docs/
git commit -m "feat: project scaffold with CMake, toolchain, and smoke test"
```

---

## Task 2: ASTNode, AST, and String Utilities

**Files:**
- Modify: `include/refmacro/ast.hpp`
- Modify: `tests/test_ast.cpp`

**Step 1: Write failing tests**

Replace `tests/test_ast.cpp`:

```cpp
#include <refmacro/ast.hpp>
#include <gtest/gtest.h>

using namespace refmacro;

TEST(StringUtils, CopyAndCompare) {
    constexpr auto result = [] consteval {
        char buf[16]{};
        copy_str(buf, "hello");
        return str_eq(buf, "hello");
    }();
    static_assert(result);
}

TEST(StringUtils, Inequality) {
    static_assert(!str_eq("abc", "xyz"));
    static_assert(str_eq("abc", "abc"));
}

TEST(ASTNode, DefaultConstructed) {
    constexpr ASTNode n{};
    static_assert(n.payload == 0.0);
    static_assert(n.child_count == 0);
    static_assert(n.scope == 0);
}

TEST(ASTNode, LitNode) {
    constexpr auto n = [] consteval {
        ASTNode node{};
        copy_str(node.tag, "lit");
        node.payload = 42.0;
        return node;
    }();
    static_assert(str_eq(n.tag, "lit"));
    static_assert(n.payload == 42.0);
}

TEST(ASTNode, VarNode) {
    constexpr auto n = [] consteval {
        ASTNode node{};
        copy_str(node.tag, "var");
        copy_str(node.name, "x");
        return node;
    }();
    static_assert(str_eq(n.tag, "var"));
    static_assert(str_eq(n.name, "x"));
}

// Verify ASTNode works as NTTP (critical for Backend dispatch)
template <ASTNode N>
consteval double nttp_payload() { return N.payload; }

TEST(ASTNode, WorksAsNTTP) {
    constexpr auto n = [] consteval {
        ASTNode node{};
        copy_str(node.tag, "lit");
        node.payload = 99.0;
        return node;
    }();
    static_assert(nttp_payload<n>() == 99.0);
}

TEST(AST, AddNode) {
    constexpr auto ast = [] consteval {
        AST<16> a{};
        ASTNode n{};
        copy_str(n.tag, "lit");
        n.payload = 7.0;
        a.add_node(n);
        return a;
    }();
    static_assert(ast.count == 1);
    static_assert(ast.root == 0);
    static_assert(ast.nodes[0].payload == 7.0);
}

TEST(AST, AddTaggedNode) {
    constexpr auto ast = [] consteval {
        AST<16> a{};

        ASTNode lit1{};
        copy_str(lit1.tag, "lit");
        lit1.payload = 1.0;
        int id1 = a.add_node(lit1);

        ASTNode lit2{};
        copy_str(lit2.tag, "lit");
        lit2.payload = 2.0;
        int id2 = a.add_node(lit2);

        a.add_tagged_node("add", {id1, id2});
        return a;
    }();
    static_assert(ast.count == 3);
    static_assert(str_eq(ast.nodes[2].tag, "add"));
    static_assert(ast.nodes[2].child_count == 2);
    static_assert(ast.nodes[2].children[0] == 0);
    static_assert(ast.nodes[2].children[1] == 1);
}

TEST(AST, Merge) {
    constexpr auto result = [] consteval {
        AST<16> a{};
        ASTNode n1{};
        copy_str(n1.tag, "lit");
        n1.payload = 1.0;
        a.add_node(n1);

        AST<16> b{};
        ASTNode n2{};
        copy_str(n2.tag, "lit");
        n2.payload = 2.0;
        b.add_node(n2);

        int offset = a.merge(b);
        return std::pair{a, offset};
    }();
    static_assert(result.first.count == 2);
    static_assert(result.second == 1);
    static_assert(result.first.nodes[1].payload == 2.0);
}
```

**Step 2: Run tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL — types not defined.

**Step 3: Implement ASTNode, AST, and utilities**

Replace `include/refmacro/ast.hpp`:

```cpp
#ifndef REFMACRO_AST_HPP
#define REFMACRO_AST_HPP

#include <cstddef>
#include <initializer_list>
#include <utility>

namespace refmacro {

// --- Consteval string utilities ---

consteval void copy_str(char* dst, const char* src, std::size_t max_len = 16) {
    for (std::size_t i = 0; i < max_len - 1 && src[i] != '\0'; ++i)
        dst[i] = src[i];
}

consteval bool str_eq(const char* a, const char* b) {
    for (std::size_t i = 0;; ++i) {
        if (a[i] != b[i]) return false;
        if (a[i] == '\0') return true;
    }
}

consteval std::size_t str_len(const char* s) {
    std::size_t len = 0;
    while (s[len] != '\0') ++len;
    return len;
}

// --- AST Node (structural type — works as NTTP) ---

struct ASTNode {
    char tag[16]{};
    double payload{};
    char name[16]{};
    int scope{0};
    int children[8]{-1, -1, -1, -1, -1, -1, -1, -1};
    int child_count{0};
};

// --- Flat AST Storage (structural type — works as NTTP) ---

template <std::size_t Cap = 64>
struct AST {
    ASTNode nodes[Cap]{};
    std::size_t count{0};
    int root{-1};

    consteval int add_node(ASTNode n) {
        int idx = static_cast<int>(count);
        nodes[count++] = n;
        root = idx;
        return idx;
    }

    consteval int add_tagged_node(const char* tag_name,
                                  std::initializer_list<int> children) {
        ASTNode n{};
        copy_str(n.tag, tag_name);
        int i = 0;
        for (int c : children) {
            n.children[i++] = c;
        }
        n.child_count = static_cast<int>(children.size());
        return add_node(n);
    }

    consteval int merge(const AST& other) {
        int offset = static_cast<int>(count);
        for (std::size_t i = 0; i < other.count; ++i) {
            ASTNode n = other.nodes[i];
            for (int c = 0; c < n.child_count; ++c) {
                if (n.children[c] >= 0)
                    n.children[c] += offset;
            }
            nodes[count++] = n;
        }
        return offset;
    }
};

} // namespace refmacro

#endif // REFMACRO_AST_HPP
```

**Step 4: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: All tests PASS, including the NTTP test.

**Step 5: Commit**

```bash
git add include/refmacro/ast.hpp tests/test_ast.cpp
git commit -m "feat: ASTNode (structural/NTTP), AST flat array, string utilities"
```

---

## Task 3: Expr — Generic Node Construction

**Files:**
- Create: `include/refmacro/expr.hpp`
- Create: `tests/test_expr.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing tests**

Create `tests/test_expr.cpp`:

```cpp
#include <refmacro/expr.hpp>
#include <gtest/gtest.h>

using namespace refmacro;

TEST(Expr, Lit) {
    constexpr auto e = Expr<>::lit(42.0);
    static_assert(e.ast.count == 1);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "lit"));
    static_assert(e.ast.nodes[e.id].payload == 42.0);
}

TEST(Expr, Var) {
    constexpr auto e = Expr<>::var("x");
    static_assert(e.ast.count == 1);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "var"));
    static_assert(str_eq(e.ast.nodes[e.id].name, "x"));
}

// make_node: generic node construction (the core of defmacro)
TEST(Expr, MakeNodeUnary) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = make_node("neg", x);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
    static_assert(e.ast.nodes[e.id].child_count == 1);
}

TEST(Expr, MakeNodeBinary) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = make_node("add", x, y);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
    static_assert(e.ast.nodes[e.id].child_count == 2);
    // LHS is var("x"), RHS is var("y")
    static_assert(str_eq(e.ast.nodes[e.ast.nodes[e.id].children[0]].tag, "var"));
    static_assert(str_eq(e.ast.nodes[e.ast.nodes[e.id].children[1]].tag, "var"));
}

TEST(Expr, MakeNodeTernary) {
    constexpr auto c = Expr<>::var("c");
    constexpr auto t = Expr<>::var("t");
    constexpr auto f = Expr<>::var("f");
    constexpr auto e = make_node("if_", c, t, f);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "if_"));
    static_assert(e.ast.nodes[e.id].child_count == 3);
}

TEST(Expr, MakeNodeLeaf) {
    constexpr auto e = make_node<>("custom_leaf");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "custom_leaf"));
    static_assert(e.ast.nodes[e.id].child_count == 0);
}

TEST(Expr, NestedMakeNode) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto one = Expr<>::lit(1.0);
    constexpr auto inner = make_node("add", x, one);
    constexpr auto outer = make_node("neg", inner);
    static_assert(str_eq(outer.ast.nodes[outer.id].tag, "neg"));
    static_assert(outer.ast.nodes[outer.id].child_count == 1);
    // Child is the add node
    constexpr int child_id = outer.ast.nodes[outer.id].children[0];
    static_assert(str_eq(outer.ast.nodes[child_id].tag, "add"));
}
```

**Step 2: Add test target to `tests/CMakeLists.txt`**

Append:

```cmake
add_executable(test_expr test_expr.cpp)
target_link_libraries(test_expr PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_expr PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_expr PROPERTIES TIMEOUT 60)
```

**Step 3: Run tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL — `Expr`, `make_node` not defined.

**Step 4: Implement Expr and make_node**

Create `include/refmacro/expr.hpp`:

```cpp
#ifndef REFMACRO_EXPR_HPP
#define REFMACRO_EXPR_HPP

#include <refmacro/ast.hpp>

namespace refmacro {

template <std::size_t Cap = 64>
struct Expr {
    AST<Cap> ast{};
    int id{-1};

    static consteval Expr lit(double v) {
        Expr e;
        ASTNode n{};
        copy_str(n.tag, "lit");
        n.payload = v;
        e.id = e.ast.add_node(n);
        return e;
    }

    static consteval Expr var(const char* name) {
        Expr e;
        ASTNode n{};
        copy_str(n.tag, "var");
        copy_str(n.name, name);
        e.id = e.ast.add_node(n);
        return e;
    }
};

// --- Generic node construction ---

// Nullary (leaf)
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag) {
    Expr<Cap> result;
    result.id = result.ast.add_tagged_node(tag, {});
    return result;
}

// Unary
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag, Expr<Cap> c0) {
    Expr<Cap> result;
    result.ast = c0.ast;
    result.id = result.ast.add_tagged_node(tag, {c0.id});
    return result;
}

// Binary
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag, Expr<Cap> c0, Expr<Cap> c1) {
    Expr<Cap> result;
    result.ast = c0.ast;
    int off1 = result.ast.merge(c1.ast);
    result.id = result.ast.add_tagged_node(tag, {c0.id, c1.id + off1});
    return result;
}

// Ternary
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag,
                               Expr<Cap> c0, Expr<Cap> c1, Expr<Cap> c2) {
    Expr<Cap> result;
    result.ast = c0.ast;
    int off1 = result.ast.merge(c1.ast);
    int off2 = result.ast.merge(c2.ast);
    result.id = result.ast.add_tagged_node(tag,
        {c0.id, c1.id + off1, c2.id + off2});
    return result;
}

// Quaternary
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag,
                               Expr<Cap> c0, Expr<Cap> c1,
                               Expr<Cap> c2, Expr<Cap> c3) {
    Expr<Cap> result;
    result.ast = c0.ast;
    int off1 = result.ast.merge(c1.ast);
    int off2 = result.ast.merge(c2.ast);
    int off3 = result.ast.merge(c3.ast);
    result.id = result.ast.add_tagged_node(tag,
        {c0.id, c1.id + off1, c2.id + off2, c3.id + off3});
    return result;
}

// Pipe operator for transform composition
template <std::size_t Cap, typename F>
consteval auto operator|(Expr<Cap> e, F transform_fn) {
    return transform_fn(e);
}

} // namespace refmacro

#endif // REFMACRO_EXPR_HPP
```

**Step 5: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: All tests PASS.

**Step 6: Commit**

```bash
git add include/refmacro/expr.hpp tests/test_expr.cpp tests/CMakeLists.txt
git commit -m "feat: Expr with lit, var, and generic make_node for arbitrary tags"
```

---

## Task 4: defmacro + compile()

This is the core task. `defmacro` defines AST node types with deferred lowering. `compile()` walks the tree and applies lowerings.

**Files:**
- Create: `include/refmacro/macro.hpp`
- Create: `include/refmacro/compile.hpp`
- Create: `tests/test_compile.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing tests**

Create `tests/test_compile.cpp`:

```cpp
#include <refmacro/macro.hpp>
#include <refmacro/compile.hpp>
#include <refmacro/expr.hpp>
#include <gtest/gtest.h>

using namespace refmacro;

// --- Define macros for testing ---

// Unary: negation
constexpr auto Neg = defmacro("neg", [](auto x) {
    return [=](auto... args) constexpr { return -x(args...); };
});

// Binary: addition
constexpr auto Add = defmacro("add", [](auto lhs, auto rhs) {
    return [=](auto... args) constexpr { return lhs(args...) + rhs(args...); };
});

// Binary: multiplication
constexpr auto Mul = defmacro("mul", [](auto lhs, auto rhs) {
    return [=](auto... args) constexpr { return lhs(args...) * rhs(args...); };
});

// --- Test defmacro as AST builder ---

TEST(Defmacro, BuildsASTNode) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = Neg(x);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
    static_assert(e.ast.nodes[e.id].child_count == 1);
}

TEST(Defmacro, BuildsBinaryNode) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = Add(x, y);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
    static_assert(e.ast.nodes[e.id].child_count == 2);
}

TEST(Defmacro, NestedConstruction) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = Neg(Add(x, Expr<>::lit(1.0)));
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
}

// --- Test compile ---

TEST(Compile, LitBuiltIn) {
    constexpr auto e = Expr<>::lit(42.0);
    constexpr auto fn = compile<e>();
    static_assert(fn() == 42.0);
}

TEST(Compile, SingleVar) {
    constexpr auto e = Expr<>::var("x");
    constexpr auto fn = compile<e>();
    static_assert(fn(5.0) == 5.0);
}

TEST(Compile, TwoVars) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    // Build with macro: Add(x, y)
    constexpr auto e = Add(x, y);
    constexpr auto fn = compile<e, Add>();
    static_assert(fn(3.0, 4.0) == 7.0);
}

TEST(Compile, UnaryMacro) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = Neg(x);
    constexpr auto fn = compile<e, Neg>();
    static_assert(fn(5.0) == -5.0);
}

TEST(Compile, NestedMacros) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    // neg(add(x, y))
    constexpr auto e = Neg(Add(x, y));
    constexpr auto fn = compile<e, Neg, Add>();
    static_assert(fn(3.0, 4.0) == -7.0);
}

TEST(Compile, ComplexExpression) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    // x * x + 2 * y
    constexpr auto e = Add(Mul(x, x), Mul(Expr<>::lit(2.0), y));
    constexpr auto fn = compile<e, Add, Mul>();
    static_assert(fn(3.0, 4.0) == 17.0); // 9 + 8
}

TEST(Compile, RuntimeCall) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = Add(Mul(x, x), y);
    constexpr auto fn = compile<e, Add, Mul>();
    EXPECT_DOUBLE_EQ(fn(3.0, 1.0), 10.0);
    EXPECT_DOUBLE_EQ(fn(0.0, 5.0), 5.0);
}

// --- Test that macros are truly generic (custom DSL node) ---

constexpr auto If = defmacro("if_", [](auto cond, auto then_val, auto else_val) {
    return [=](auto... args) constexpr {
        return cond(args...) ? then_val(args...) : else_val(args...);
    };
});

constexpr auto Gt = defmacro("gt", [](auto lhs, auto rhs) {
    return [=](auto... args) constexpr { return lhs(args...) > rhs(args...); };
});

TEST(Compile, CustomDSLNode) {
    constexpr auto x = Expr<>::var("x");
    // if_(x > 0, x, neg(x))  — absolute value
    constexpr auto e = If(Gt(x, Expr<>::lit(0.0)), x, Neg(x));
    constexpr auto fn = compile<e, If, Gt, Neg>();
    static_assert(fn(5.0) == 5.0);
    static_assert(fn(-3.0) == 3.0);
}
```

**Step 2: Add test target to `tests/CMakeLists.txt`**

Append:

```cmake
add_executable(test_compile test_compile.cpp)
target_link_libraries(test_compile PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_compile PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_compile PROPERTIES TIMEOUT 60)
```

**Step 3: Run tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL — `defmacro`, `compile` not defined.

**Step 4: Implement defmacro**

Create `include/refmacro/macro.hpp`:

```cpp
#ifndef REFMACRO_MACRO_HPP
#define REFMACRO_MACRO_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>

namespace refmacro {

// Macro: an AST node type with deferred lowering.
//
// - Calling a Macro builds an AST node (no lowering happens).
// - The lowering function `fn` is applied later by compile().
// - `fn` receives compiled children (lambdas) and returns a lambda.
// - CompileFn must be a stateless lambda (structural → NTTP-compatible).
template <typename CompileFn>
struct Macro {
    char tag[16]{};
    CompileFn fn{};

    // Build AST nodes — lowering is deferred
    consteval Expr<> operator()() const {
        return make_node<>(tag);
    }
    consteval Expr<> operator()(Expr<> c0) const {
        return make_node(tag, c0);
    }
    consteval Expr<> operator()(Expr<> c0, Expr<> c1) const {
        return make_node(tag, c0, c1);
    }
    consteval Expr<> operator()(Expr<> c0, Expr<> c1, Expr<> c2) const {
        return make_node(tag, c0, c1, c2);
    }
    consteval Expr<> operator()(Expr<> c0, Expr<> c1,
                                Expr<> c2, Expr<> c3) const {
        return make_node(tag, c0, c1, c2, c3);
    }
};

template <typename CompileFn>
consteval auto defmacro(const char* name, CompileFn) {
    Macro<CompileFn> m{};
    copy_str(m.tag, name);
    return m;
}

} // namespace refmacro

#endif // REFMACRO_MACRO_HPP
```

**Step 5: Implement compile**

Create `include/refmacro/compile.hpp`:

```cpp
#ifndef REFMACRO_COMPILE_HPP
#define REFMACRO_COMPILE_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <tuple>
#include <utility>

namespace refmacro {

// --- VarMap: extract ordered unique variable names from AST ---

template <std::size_t MaxVars = 8>
struct VarMap {
    char names[MaxVars][16]{};
    std::size_t count{0};

    consteval bool contains(const char* name) const {
        for (std::size_t i = 0; i < count; ++i)
            if (str_eq(names[i], name)) return true;
        return false;
    }

    consteval void add(const char* name) {
        if (!contains(name)) {
            copy_str(names[count], name);
            ++count;
        }
    }

    consteval int index_of(const char* name) const {
        for (std::size_t i = 0; i < count; ++i)
            if (str_eq(names[i], name)) return static_cast<int>(i);
        return -1;
    }
};

template <std::size_t Cap>
consteval auto extract_var_map(const AST<Cap>& ast) {
    VarMap<> vm{};
    for (std::size_t i = 0; i < ast.count; ++i) {
        if (str_eq(ast.nodes[i].tag, "var"))
            vm.add(ast.nodes[i].name);
    }
    return vm;
}

// --- Macro dispatch: find macro by tag and apply ---

namespace detail {

// Apply the first macro whose tag matches. If none match, compile error.
template <auto Tag, auto First, auto... Rest>
consteval auto apply_macro(auto children_tuple) {
    if constexpr (str_eq(Tag, First.tag)) {
        return std::apply(First.fn, children_tuple);
    } else {
        static_assert(sizeof...(Rest) > 0,
            "no macro defined for AST tag");
        return apply_macro<Tag, Rest...>(children_tuple);
    }
}

// --- Recursive compile ---

template <auto ast, int id, auto var_map, auto... Macros>
consteval auto compile_node() {
    constexpr auto n = ast.nodes[id];

    // Built-in: variable → argument accessor
    if constexpr (str_eq(n.tag, "var")) {
        constexpr int idx = var_map.index_of(n.name);
        static_assert(idx >= 0, "unbound variable in AST");
        return [](auto... args) constexpr {
            return std::get<idx>(std::tuple{args...});
        };
    }
    // Built-in: literal → constant
    else if constexpr (str_eq(n.tag, "lit")) {
        return [](auto...) constexpr { return n.payload; };
    }
    // Everything else: dispatch to macros
    else {
        // Compile children bottom-up, then dispatch to matching macro
        auto children = [&]<int... Cs>(std::integer_sequence<int, Cs...>) {
            return std::tuple{
                compile_node<ast, n.children[Cs], var_map, Macros...>()...
            };
        }(std::make_integer_sequence<int, n.child_count>{});

        return apply_macro<n.tag, Macros...>(children);
    }
}

} // namespace detail

// --- Public API ---

template <auto e, auto... Macros>
consteval auto compile() {
    constexpr auto vm = extract_var_map(e.ast);
    return detail::compile_node<e.ast, e.id, vm, Macros...>();
}

} // namespace refmacro

#endif // REFMACRO_COMPILE_HPP
```

**Step 6: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: All tests PASS, including custom DSL node (if_/gt).

**Step 7: Commit**

```bash
git add include/refmacro/macro.hpp include/refmacro/compile.hpp \
        tests/test_compile.cpp tests/CMakeLists.txt
git commit -m "feat: defmacro with deferred lowering and generic compile()"
```

---

## Task 5: Math Macros + Operator Sugar

**Files:**
- Create: `include/refmacro/math.hpp`
- Create: `tests/test_math.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing tests**

Create `tests/test_math.cpp`:

```cpp
#include <refmacro/math.hpp>
#include <gtest/gtest.h>

using namespace refmacro;

// --- Test operator sugar ---

TEST(MathOps, Add) {
    constexpr auto e = Expr<>::var("x") + Expr<>::lit(1.0);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
}

TEST(MathOps, Mul) {
    constexpr auto e = Expr<>::var("x") * Expr<>::var("y");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "mul"));
}

TEST(MathOps, Sub) {
    constexpr auto e = Expr<>::var("x") - Expr<>::lit(1.0);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "sub"));
}

TEST(MathOps, Div) {
    constexpr auto e = Expr<>::var("x") / Expr<>::lit(2.0);
    static_assert(str_eq(e.ast.nodes[e.id].tag, "div"));
}

TEST(MathOps, UnaryNeg) {
    constexpr auto e = -Expr<>::var("x");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "neg"));
}

TEST(MathOps, DoubleLhsAdd) {
    constexpr auto e = 2.0 + Expr<>::var("x");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
    static_assert(e.ast.nodes[e.ast.nodes[e.id].children[0]].payload == 2.0);
}

TEST(MathOps, DoubleLhsMul) {
    constexpr auto e = 3.0 * Expr<>::var("x");
    static_assert(str_eq(e.ast.nodes[e.id].tag, "mul"));
}

TEST(MathOps, DoubleRhsAdd) {
    constexpr auto e = Expr<>::var("x") + 5.0;
    static_assert(str_eq(e.ast.nodes[e.id].tag, "add"));
}

// --- Test math_compile (convenience wrapper) ---

TEST(MathCompile, Linear) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = 3.0 * x + 5.0;
    constexpr auto fn = math_compile<e>();
    static_assert(fn(2.0) == 11.0);
}

TEST(MathCompile, Polynomial) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = x * x + 2.0 * y;
    constexpr auto fn = math_compile<e>();
    static_assert(fn(3.0, 4.0) == 17.0);
}

TEST(MathCompile, AllOps) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = -(x * x - x / 2.0);
    constexpr auto fn = math_compile<e>();
    // x=4: -(16 - 2) = -14
    static_assert(fn(4.0) == -14.0);
}

TEST(MathCompile, Runtime) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = x * x + 1.0;
    constexpr auto fn = math_compile<e>();
    EXPECT_DOUBLE_EQ(fn(3.0), 10.0);
    EXPECT_DOUBLE_EQ(fn(0.0), 1.0);
}
```

**Step 2: Add test target to `tests/CMakeLists.txt`**

Append:

```cmake
add_executable(test_math test_math.cpp)
target_link_libraries(test_math PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_math PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_math PROPERTIES TIMEOUT 60)
```

**Step 3: Run tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: FAIL — operators and math macros not defined.

**Step 4: Implement math macros and operator sugar**

Create `include/refmacro/math.hpp`:

```cpp
#ifndef REFMACRO_MATH_HPP
#define REFMACRO_MATH_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/macro.hpp>
#include <refmacro/compile.hpp>

namespace refmacro {

// --- Math macros (lowering to lambdas) ---

inline constexpr auto MAdd = defmacro("add", [](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) + rhs(a...); };
});

inline constexpr auto MSub = defmacro("sub", [](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) - rhs(a...); };
});

inline constexpr auto MMul = defmacro("mul", [](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) * rhs(a...); };
});

inline constexpr auto MDiv = defmacro("div", [](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) / rhs(a...); };
});

inline constexpr auto MNeg = defmacro("neg", [](auto x) {
    return [=](auto... a) constexpr { return -x(a...); };
});

// --- Operator sugar (creates AST nodes, no lowering) ---

template <std::size_t Cap>
consteval Expr<Cap> operator+(Expr<Cap> lhs, Expr<Cap> rhs) {
    return make_node("add", lhs, rhs);
}
template <std::size_t Cap>
consteval Expr<Cap> operator-(Expr<Cap> lhs, Expr<Cap> rhs) {
    return make_node("sub", lhs, rhs);
}
template <std::size_t Cap>
consteval Expr<Cap> operator*(Expr<Cap> lhs, Expr<Cap> rhs) {
    return make_node("mul", lhs, rhs);
}
template <std::size_t Cap>
consteval Expr<Cap> operator/(Expr<Cap> lhs, Expr<Cap> rhs) {
    return make_node("div", lhs, rhs);
}
template <std::size_t Cap>
consteval Expr<Cap> operator-(Expr<Cap> x) {
    return make_node("neg", x);
}

// double on LHS
consteval Expr<> operator+(double lhs, Expr<> rhs) { return Expr<>::lit(lhs) + rhs; }
consteval Expr<> operator-(double lhs, Expr<> rhs) { return Expr<>::lit(lhs) - rhs; }
consteval Expr<> operator*(double lhs, Expr<> rhs) { return Expr<>::lit(lhs) * rhs; }
consteval Expr<> operator/(double lhs, Expr<> rhs) { return Expr<>::lit(lhs) / rhs; }

// double on RHS
consteval Expr<> operator+(Expr<> lhs, double rhs) { return lhs + Expr<>::lit(rhs); }
consteval Expr<> operator-(Expr<> lhs, double rhs) { return lhs - Expr<>::lit(rhs); }
consteval Expr<> operator*(Expr<> lhs, double rhs) { return lhs * Expr<>::lit(rhs); }
consteval Expr<> operator/(Expr<> lhs, double rhs) { return lhs / Expr<>::lit(rhs); }

// --- Convenience: compile with all math macros ---

template <auto e>
consteval auto math_compile() {
    return compile<e, MAdd, MSub, MMul, MDiv, MNeg>();
}

} // namespace refmacro

#endif // REFMACRO_MATH_HPP
```

**Step 5: Run tests to verify they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: All tests PASS.

**Step 6: Commit**

```bash
git add include/refmacro/math.hpp tests/test_math.cpp tests/CMakeLists.txt
git commit -m "feat: math macros (add/sub/mul/div/neg) with operator sugar and math_compile"
```

---

## Task 6: NodeView Cursor

**Files:**
- Create: `include/refmacro/node_view.hpp`
- Modify: `tests/test_ast.cpp`

**Step 1: Write failing tests**

Append to `tests/test_ast.cpp`:

```cpp
#include <refmacro/node_view.hpp>

TEST(NodeView, TagAndPayload) {
    constexpr auto e = [] consteval {
        AST<16> a{};
        ASTNode n{};
        copy_str(n.tag, "lit");
        n.payload = 3.14;
        a.add_node(n);
        return a;
    }();
    constexpr NodeView v{e, 0};
    static_assert(v.tag() == "lit");
    static_assert(v.payload() == 3.14);
}

TEST(NodeView, ChildAccess) {
    constexpr auto e = [] consteval {
        AST<16> a{};
        ASTNode lit1{};
        copy_str(lit1.tag, "lit");
        lit1.payload = 1.0;
        int id1 = a.add_node(lit1);

        ASTNode lit2{};
        copy_str(lit2.tag, "lit");
        lit2.payload = 2.0;
        int id2 = a.add_node(lit2);

        a.add_tagged_node("add", {id1, id2});
        return a;
    }();
    constexpr NodeView root{e, 2};
    static_assert(root.tag() == "add");
    static_assert(root.child_count() == 2);
    static_assert(root.child(0).tag() == "lit");
    static_assert(root.child(0).payload() == 1.0);
    static_assert(root.child(1).payload() == 2.0);
}

TEST(NodeView, Name) {
    constexpr auto e = [] consteval {
        AST<16> a{};
        ASTNode n{};
        copy_str(n.tag, "var");
        copy_str(n.name, "x");
        a.add_node(n);
        return a;
    }();
    constexpr NodeView v{e, 0};
    static_assert(v.name() == "x");
}
```

**Step 2: Run tests to verify they fail**

Expected: FAIL — `NodeView` not defined.

**Step 3: Implement NodeView**

Create `include/refmacro/node_view.hpp`:

```cpp
#ifndef REFMACRO_NODE_VIEW_HPP
#define REFMACRO_NODE_VIEW_HPP

#include <refmacro/ast.hpp>
#include <string_view>

namespace refmacro {

template <std::size_t Cap>
struct NodeView {
    const AST<Cap>& ast;
    int id;

    consteval auto tag() const -> std::string_view { return ast.nodes[id].tag; }
    consteval auto name() const -> std::string_view { return ast.nodes[id].name; }
    consteval double payload() const { return ast.nodes[id].payload; }
    consteval int scope() const { return ast.nodes[id].scope; }
    consteval int child_count() const { return ast.nodes[id].child_count; }
    consteval NodeView child(int i) const {
        return NodeView{ast, ast.nodes[id].children[i]};
    }
};

template <std::size_t Cap>
NodeView(const AST<Cap>&, int) -> NodeView<Cap>;

} // namespace refmacro

#endif // REFMACRO_NODE_VIEW_HPP
```

**Step 4: Run tests, verify pass**

**Step 5: Commit**

```bash
git add include/refmacro/node_view.hpp tests/test_ast.cpp
git commit -m "feat: NodeView cursor for ergonomic AST traversal"
```

---

## Task 7: rewrite() and transform() Primitives

**Files:**
- Create: `include/refmacro/transforms.hpp`
- Create: `tests/test_transforms.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing tests**

Create `tests/test_transforms.cpp`:

```cpp
#include <refmacro/transforms.hpp>
#include <refmacro/math.hpp>
#include <gtest/gtest.h>

using namespace refmacro;

consteval auto root_tag(const Expr<>& e) -> std::string_view {
    return e.ast.nodes[e.id].tag;
}
consteval double root_payload(const Expr<>& e) {
    return e.ast.nodes[e.id].payload;
}

// --- rewrite tests ---

TEST(Rewrite, Identity) {
    constexpr auto e = Expr<>::var("x") + Expr<>::lit(1.0);
    constexpr auto result = rewrite(e, [](NodeView<64>) consteval
                                       -> std::optional<Expr<>> {
        return std::nullopt;
    });
    static_assert(root_tag(result) == "add");
}

TEST(Rewrite, AddZero) {
    constexpr auto e = Expr<>::var("x") + Expr<>::lit(0.0);
    constexpr auto result = rewrite(e, [](NodeView<64> n) consteval
                                       -> std::optional<Expr<>> {
        if (n.tag() == "add" && n.child(1).tag() == "lit"
            && n.child(1).payload() == 0.0)
            return to_expr(n, n.child(0));
        return std::nullopt;
    });
    static_assert(root_tag(result) == "var");
}

TEST(Rewrite, ConstantFold) {
    constexpr auto e = Expr<>::lit(3.0) + Expr<>::lit(4.0);
    constexpr auto result = rewrite(e, [](NodeView<64> n) consteval
                                       -> std::optional<Expr<>> {
        if (n.tag() == "add" && n.child(0).tag() == "lit"
            && n.child(1).tag() == "lit")
            return Expr<>::lit(n.child(0).payload() + n.child(1).payload());
        return std::nullopt;
    });
    static_assert(root_payload(result) == 7.0);
}

// --- transform tests ---

TEST(Transform, DoubleLiterals) {
    constexpr auto e = Expr<>::lit(3.0) + Expr<>::lit(4.0);
    constexpr auto result = transform(e,
        [](NodeView<64> n, auto recurse) consteval -> Expr<> {
            if (n.tag() == "lit")
                return Expr<>::lit(n.payload() * 2.0);
            if (n.tag() == "add")
                return recurse(n.child(0)) + recurse(n.child(1));
            return Expr<>::lit(0.0);
        });
    static_assert(root_tag(result) == "add");
    constexpr auto lhs = result.ast.nodes[result.ast.nodes[result.id].children[0]];
    constexpr auto rhs = result.ast.nodes[result.ast.nodes[result.id].children[1]];
    static_assert(lhs.payload == 6.0);
    static_assert(rhs.payload == 8.0);
}

TEST(Transform, IdentityPreservesStructure) {
    constexpr auto e = Expr<>::var("x") * Expr<>::var("y");
    constexpr auto result = transform(e,
        [](NodeView<64> n, auto recurse) consteval -> Expr<> {
            if (n.tag() == "var")
                return Expr<>::var(n.name().data());
            if (n.tag() == "mul")
                return recurse(n.child(0)) * recurse(n.child(1));
            return Expr<>::lit(0.0);
        });
    static_assert(root_tag(result) == "mul");
}
```

**Step 2: Add test target to `tests/CMakeLists.txt`**

Append:

```cmake
add_executable(test_transforms test_transforms.cpp)
target_link_libraries(test_transforms PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_transforms PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_transforms PROPERTIES TIMEOUT 60)
```

**Step 3: Run tests to verify they fail**

Expected: FAIL — `rewrite`, `transform`, `to_expr` not defined.

**Step 4: Implement transforms**

Create `include/refmacro/transforms.hpp`:

```cpp
#ifndef REFMACRO_TRANSFORMS_HPP
#define REFMACRO_TRANSFORMS_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/node_view.hpp>
#include <optional>

namespace refmacro {

// --- to_expr: extract a subtree into a standalone Expr ---

namespace detail {

template <std::size_t Cap>
consteval int copy_subtree(AST<Cap>& dst, const AST<Cap>& src, int src_id) {
    ASTNode n = src.nodes[src_id];
    int new_children[8]{-1, -1, -1, -1, -1, -1, -1, -1};
    for (int i = 0; i < n.child_count; ++i) {
        new_children[i] = copy_subtree(dst, src, n.children[i]);
    }
    for (int i = 0; i < n.child_count; ++i) {
        n.children[i] = new_children[i];
    }
    return dst.add_node(n);
}

template <std::size_t Cap, typename Rule>
consteval Expr<Cap> rebuild_bottom_up(const AST<Cap>& ast, int id, Rule rule) {
    ASTNode n = ast.nodes[id];

    if (n.child_count == 0) {
        NodeView<Cap> view{ast, id};
        auto replacement = rule(view);
        if (replacement) return *replacement;
        Expr<Cap> leaf;
        leaf.id = leaf.ast.add_node(n);
        return leaf;
    }

    // Rebuild children first (bottom-up)
    Expr<Cap> child_exprs[8];
    for (int i = 0; i < n.child_count; ++i) {
        child_exprs[i] = rebuild_bottom_up<Cap>(ast, n.children[i], rule);
    }

    // Merge rebuilt children into new Expr
    Expr<Cap> rebuilt;
    int new_child_ids[8]{-1, -1, -1, -1, -1, -1, -1, -1};
    for (int i = 0; i < n.child_count; ++i) {
        int offset = (i == 0 && rebuilt.ast.count == 0)
            ? 0 : rebuilt.ast.merge(child_exprs[i].ast);
        if (i == 0 && rebuilt.ast.count == 0) {
            rebuilt.ast = child_exprs[i].ast;
            offset = 0;
        }
        new_child_ids[i] = child_exprs[i].id + offset;
    }

    ASTNode rebuilt_node = n;
    for (int i = 0; i < n.child_count; ++i) {
        rebuilt_node.children[i] = new_child_ids[i];
    }
    rebuilt.id = rebuilt.ast.add_node(rebuilt_node);

    // Try rule on rebuilt node
    NodeView<Cap> view{rebuilt.ast, rebuilt.id};
    auto replacement = rule(view);
    if (replacement) return *replacement;

    return rebuilt;
}

} // namespace detail

template <std::size_t Cap>
consteval Expr<Cap> to_expr(NodeView<Cap> root, NodeView<Cap> subtree) {
    Expr<Cap> result;
    result.id = detail::copy_subtree(result.ast, root.ast, subtree.id);
    return result;
}

// --- rewrite: bottom-up rule application until fixed point ---

template <std::size_t Cap = 64, typename Rule>
consteval Expr<Cap> rewrite(Expr<Cap> e, Rule rule, int max_iters = 100) {
    for (int iter = 0; iter < max_iters; ++iter) {
        auto result = detail::rebuild_bottom_up<Cap>(e.ast, e.id, rule);
        auto check = detail::rebuild_bottom_up<Cap>(result.ast, result.id, rule);
        if (check.ast.count == result.ast.count) {
            return result;
        }
        e = result;
    }
    return e;
}

// --- transform: structural recursion with user visitor ---

template <std::size_t Cap = 64, typename Visitor>
consteval Expr<Cap> transform(Expr<Cap> e, Visitor visitor) {
    auto recurse = [&](auto self, int id) consteval -> Expr<Cap> {
        NodeView<Cap> view{e.ast, id};
        auto rec = [&](NodeView<Cap> child) consteval -> Expr<Cap> {
            return self(self, child.id);
        };
        return visitor(view, rec);
    };
    return recurse(recurse, e.id);
}

} // namespace refmacro

#endif // REFMACRO_TRANSFORMS_HPP
```

**Step 5: Run tests, verify pass**

**Step 6: Commit**

```bash
git add include/refmacro/transforms.hpp tests/test_transforms.cpp tests/CMakeLists.txt
git commit -m "feat: rewrite() and transform() AST rewriting primitives"
```

---

## Task 8: Pretty Printer

**Files:**
- Create: `include/refmacro/pretty_print.hpp`
- Modify: `tests/test_math.cpp`

**Step 1: Write failing tests**

Append to `tests/test_math.cpp`:

```cpp
#include <refmacro/pretty_print.hpp>

TEST(PrettyPrint, Lit) {
    constexpr auto e = Expr<>::lit(42.0);
    constexpr auto s = pretty_print(e);
    static_assert(s == "42");
}

TEST(PrettyPrint, Var) {
    constexpr auto e = Expr<>::var("x");
    constexpr auto s = pretty_print(e);
    static_assert(s == "x");
}

TEST(PrettyPrint, BinaryInfix) {
    constexpr auto e = Expr<>::var("x") + Expr<>::lit(1.0);
    constexpr auto s = pretty_print(e);
    static_assert(s == "(x + 1)");
}

TEST(PrettyPrint, Nested) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto e = x * x + 2.0 * y;
    constexpr auto s = pretty_print(e);
    static_assert(s == "((x * x) + (2 * y))");
}

TEST(PrettyPrint, Neg) {
    constexpr auto e = -Expr<>::var("x");
    constexpr auto s = pretty_print(e);
    static_assert(s == "(-x)");
}

TEST(PrettyPrint, GenericTag) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = make_node("custom", x);
    constexpr auto s = pretty_print(e);
    static_assert(s == "custom(x)");
}
```

**Step 2: Implement pretty_print**

Create `include/refmacro/pretty_print.hpp`:

```cpp
#ifndef REFMACRO_PRETTY_PRINT_HPP
#define REFMACRO_PRETTY_PRINT_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>

namespace refmacro {

template <std::size_t N = 256>
struct FixedString {
    char data[N]{};
    std::size_t len{0};

    consteval FixedString() = default;
    consteval FixedString(const char* s) {
        while (s[len] != '\0' && len < N - 1) { data[len] = s[len]; ++len; }
    }

    consteval void append(const char* s) {
        for (std::size_t i = 0; s[i] != '\0' && len < N - 1; ++i)
            data[len++] = s[i];
    }
    consteval void append(const FixedString& o) {
        for (std::size_t i = 0; i < o.len && len < N - 1; ++i)
            data[len++] = o.data[i];
    }
    consteval void append_char(char c) { if (len < N - 1) data[len++] = c; }

    consteval void append_int(int v) {
        if (v < 0) { append_char('-'); v = -v; }
        if (v == 0) { append_char('0'); return; }
        char buf[20]{};
        int pos = 0;
        while (v > 0) { buf[pos++] = '0' + (v % 10); v /= 10; }
        for (int i = pos - 1; i >= 0; --i) append_char(buf[i]);
    }

    consteval void append_double(double v) {
        if (v < 0) { append_char('-'); v = -v; }
        int integer_part = static_cast<int>(v);
        double frac = v - integer_part;
        append_int(integer_part);
        if (frac > 0.0001) {
            append_char('.');
            for (int i = 0; i < 6 && frac > 0.0001; ++i) {
                frac *= 10.0;
                int digit = static_cast<int>(frac);
                append_char('0' + digit);
                frac -= digit;
            }
        }
    }

    consteval bool operator==(const char* s) const {
        for (std::size_t i = 0; i < len; ++i)
            if (data[i] != s[i]) return false;
        return s[len] == '\0';
    }
};

namespace detail {

consteval bool is_infix(const char* tag) {
    return str_eq(tag, "add") || str_eq(tag, "sub")
        || str_eq(tag, "mul") || str_eq(tag, "div");
}

consteval const char* infix_sym(const char* tag) {
    if (str_eq(tag, "add")) return " + ";
    if (str_eq(tag, "sub")) return " - ";
    if (str_eq(tag, "mul")) return " * ";
    if (str_eq(tag, "div")) return " / ";
    return " ? ";
}

template <std::size_t Cap>
consteval FixedString<256> pp_node(const AST<Cap>& ast, int id) {
    auto n = ast.nodes[id];
    FixedString<256> s;

    if (str_eq(n.tag, "lit")) { s.append_double(n.payload); return s; }
    if (str_eq(n.tag, "var")) { s.append(n.name); return s; }

    if (str_eq(n.tag, "neg") && n.child_count == 1) {
        s.append("(-");
        s.append(pp_node(ast, n.children[0]));
        s.append_char(')');
        return s;
    }

    if (is_infix(n.tag) && n.child_count == 2) {
        s.append_char('(');
        s.append(pp_node(ast, n.children[0]));
        s.append(infix_sym(n.tag));
        s.append(pp_node(ast, n.children[1]));
        s.append_char(')');
        return s;
    }

    // Generic: tag(child, child, ...)
    s.append(n.tag);
    s.append_char('(');
    for (int i = 0; i < n.child_count; ++i) {
        if (i > 0) s.append(", ");
        s.append(pp_node(ast, n.children[i]));
    }
    s.append_char(')');
    return s;
}

} // namespace detail

template <std::size_t Cap = 64>
consteval FixedString<256> pretty_print(const Expr<Cap>& e) {
    return detail::pp_node(e.ast, e.id);
}

} // namespace refmacro

#endif // REFMACRO_PRETTY_PRINT_HPP
```

**Step 3: Run tests, verify pass**

**Step 4: Commit**

```bash
git add include/refmacro/pretty_print.hpp tests/test_math.cpp
git commit -m "feat: pretty_print with infix math and generic tag(args) fallback"
```

---

## Task 9: simplify() and differentiate()

**Files:**
- Modify: `include/refmacro/math.hpp`
- Modify: `tests/test_math.cpp`

**Step 1: Write failing tests**

Append to `tests/test_math.cpp`:

```cpp
#include <refmacro/transforms.hpp>

consteval auto root_tag(const Expr<>& e) -> std::string_view {
    return e.ast.nodes[e.id].tag;
}
consteval double root_payload(const Expr<>& e) {
    return e.ast.nodes[e.id].payload;
}
consteval auto root_name(const Expr<>& e) -> std::string_view {
    return e.ast.nodes[e.id].name;
}

// --- simplify ---
TEST(Simplify, AddZero)     { static_assert(root_tag(simplify(Expr<>::var("x") + 0.0)) == "var"); }
TEST(Simplify, ZeroAdd)     { static_assert(root_tag(simplify(0.0 + Expr<>::var("x"))) == "var"); }
TEST(Simplify, MulOne)      { static_assert(root_tag(simplify(Expr<>::var("x") * 1.0)) == "var"); }
TEST(Simplify, OneMul)      { static_assert(root_tag(simplify(1.0 * Expr<>::var("x"))) == "var"); }
TEST(Simplify, MulZero)     { static_assert(root_payload(simplify(0.0 * Expr<>::var("x"))) == 0.0); }
TEST(Simplify, SubZero)     { static_assert(root_tag(simplify(Expr<>::var("x") - 0.0)) == "var"); }
TEST(Simplify, DivOne)      { static_assert(root_tag(simplify(Expr<>::var("x") / 1.0)) == "var"); }
TEST(Simplify, DoubleNeg)   { static_assert(root_tag(simplify(-(-Expr<>::var("x")))) == "var"); }
TEST(Simplify, ConstFold)   { static_assert(root_payload(simplify(Expr<>::lit(3.0) + 4.0)) == 7.0); }
TEST(Simplify, Nested)      { static_assert(root_tag(simplify((Expr<>::var("x") + 0.0) * 1.0)) == "var"); }

// --- differentiate ---
TEST(Diff, Lit)           { static_assert(root_payload(differentiate(Expr<>::lit(5.0), "x")) == 0.0); }
TEST(Diff, VarSelf)       { static_assert(root_payload(differentiate(Expr<>::var("x"), "x")) == 1.0); }
TEST(Diff, VarOther)      { static_assert(root_payload(differentiate(Expr<>::var("y"), "x")) == 0.0); }
TEST(Diff, SumSimplified)  {
    static_assert(root_payload(simplify(differentiate(Expr<>::var("x") + Expr<>::var("y"), "x"))) == 1.0);
}
TEST(Diff, ConstTimesVar) {
    static_assert(root_payload(simplify(differentiate(2.0 * Expr<>::var("x"), "x"))) == 2.0);
}
TEST(Diff, NegVar) {
    static_assert(root_payload(simplify(differentiate(-Expr<>::var("x"), "x"))) == -1.0);
}

// --- Full pipeline: build -> differentiate -> simplify -> compile ---
TEST(Pipeline, Quadratic) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = x * x + 2.0 * x + 1.0;
    constexpr auto diff_x = [](Expr<> e) consteval { return differentiate(e, "x"); };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto fn = math_compile<df>();
    static_assert(fn(3.0) == 8.0);  // 2*3 + 2
    static_assert(fn(0.0) == 2.0);
}

TEST(Pipeline, SecondDerivative) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = x * x * x;
    constexpr auto diff_x = [](Expr<> e) consteval { return differentiate(e, "x"); };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto d2f = f | diff_x | simp | diff_x | simp;
    constexpr auto fn = math_compile<d2f>();
    static_assert(fn(2.0) == 12.0);  // 6*2
}
```

**Step 2: Run tests to verify they fail**

Expected: FAIL — `simplify`, `differentiate` not defined.

**Step 3: Add simplify and differentiate to `include/refmacro/math.hpp`**

Append before the closing `#endif`:

```cpp
// --- simplify: algebraic identities + constant folding ---

namespace detail {
consteval double eval_op(std::string_view tag, double lhs, double rhs) {
    if (tag == "add") return lhs + rhs;
    if (tag == "sub") return lhs - rhs;
    if (tag == "mul") return lhs * rhs;
    if (tag == "div") return lhs / rhs;
    return 0.0;
}
} // namespace detail

template <std::size_t Cap = 64>
consteval Expr<Cap> simplify(Expr<Cap> e) {
    return rewrite(e, [](NodeView<Cap> n) consteval -> std::optional<Expr<Cap>> {
        if (n.tag() == "add" && n.child_count() == 2 && n.child(1).tag() == "lit" && n.child(1).payload() == 0.0) return to_expr(n, n.child(0));
        if (n.tag() == "add" && n.child_count() == 2 && n.child(0).tag() == "lit" && n.child(0).payload() == 0.0) return to_expr(n, n.child(1));
        if (n.tag() == "mul" && n.child_count() == 2 && n.child(1).tag() == "lit" && n.child(1).payload() == 1.0) return to_expr(n, n.child(0));
        if (n.tag() == "mul" && n.child_count() == 2 && n.child(0).tag() == "lit" && n.child(0).payload() == 1.0) return to_expr(n, n.child(1));
        if (n.tag() == "mul" && n.child_count() == 2 && n.child(0).tag() == "lit" && n.child(0).payload() == 0.0) return Expr<Cap>::lit(0.0);
        if (n.tag() == "mul" && n.child_count() == 2 && n.child(1).tag() == "lit" && n.child(1).payload() == 0.0) return Expr<Cap>::lit(0.0);
        if (n.tag() == "sub" && n.child_count() == 2 && n.child(1).tag() == "lit" && n.child(1).payload() == 0.0) return to_expr(n, n.child(0));
        if (n.tag() == "div" && n.child_count() == 2 && n.child(1).tag() == "lit" && n.child(1).payload() == 1.0) return to_expr(n, n.child(0));
        if (n.tag() == "neg" && n.child_count() == 1 && n.child(0).tag() == "neg") return to_expr(n, n.child(0).child(0));
        if (n.tag() == "neg" && n.child_count() == 1 && n.child(0).tag() == "lit") return Expr<Cap>::lit(-n.child(0).payload());
        if (n.child_count() == 2 && n.child(0).tag() == "lit" && n.child(1).tag() == "lit") {
            auto tag = n.tag();
            if (tag == "add" || tag == "sub" || tag == "mul" || tag == "div")
                return Expr<Cap>::lit(detail::eval_op(tag, n.child(0).payload(), n.child(1).payload()));
        }
        return std::nullopt;
    });
}

// --- differentiate: symbolic differentiation via structural recursion ---

template <std::size_t Cap = 64>
consteval Expr<Cap> differentiate(Expr<Cap> e, const char* var) {
    return transform(e,
        [var](NodeView<Cap> n, auto recurse) consteval -> Expr<Cap> {
            if (n.tag() == "lit") return Expr<Cap>::lit(0.0);
            if (n.tag() == "var") return Expr<Cap>::lit(n.name() == var ? 1.0 : 0.0);
            if (n.tag() == "neg" && n.child_count() == 1) return -recurse(n.child(0));
            if (n.tag() == "add" && n.child_count() == 2) return recurse(n.child(0)) + recurse(n.child(1));
            if (n.tag() == "sub" && n.child_count() == 2) return recurse(n.child(0)) - recurse(n.child(1));
            if (n.tag() == "mul" && n.child_count() == 2) {
                auto f = to_expr(n, n.child(0));
                auto g = to_expr(n, n.child(1));
                return f * recurse(n.child(1)) + recurse(n.child(0)) * g;
            }
            if (n.tag() == "div" && n.child_count() == 2) {
                auto f = to_expr(n, n.child(0));
                auto g = to_expr(n, n.child(1));
                return (recurse(n.child(0)) * g - f * recurse(n.child(1))) / (g * g);
            }
            return Expr<Cap>::lit(0.0);
        });
}
```

> **Note:** `simplify` and `differentiate` include `#include <refmacro/transforms.hpp>` at the top of `math.hpp`.

**Step 4: Run tests, verify pass**

**Step 5: Commit**

```bash
git add include/refmacro/math.hpp tests/test_math.cpp
git commit -m "feat: simplify() and differentiate() using rewrite/transform primitives"
```

---

## Task 10: expr() Reflection Binding

**Files:**
- Modify: `include/refmacro/expr.hpp`
- Modify: `tests/test_math.cpp`

> **Note:** Requires `parameters_of()` and `identifier_of()` from P2996. Includes manual fallback.

**Step 1: Write failing tests**

Append to `tests/test_math.cpp`:

```cpp
TEST(ExprBinding, SingleVar) {
    constexpr auto e = expr([](auto x) { return x * x; });
    constexpr auto fn = math_compile<e>();
    static_assert(fn(3.0) == 9.0);
}

TEST(ExprBinding, TwoVars) {
    constexpr auto e = expr([](auto x, auto y) { return x * x + 2.0 * y; });
    constexpr auto fn = math_compile<e>();
    static_assert(fn(3.0, 4.0) == 17.0);
}

TEST(ExprBinding, Pipeline) {
    constexpr auto f = expr([](auto x) { return x * x + 2.0 * x + 1.0; });
    constexpr auto diff_x = [](Expr<> e) consteval { return differentiate(e, "x"); };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto fn = math_compile<df>();
    static_assert(fn(3.0) == 8.0);
}
```

**Step 2: Implement expr()**

Add to `include/refmacro/expr.hpp`:

```cpp
#include <meta>

// Reflection-based expr(): extracts parameter names from lambda
template <typename F>
consteval auto expr(F f) {
    constexpr auto params = parameters_of(^^decltype(&F::operator()));
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return f(Expr<>::var(identifier_of(params[Is]).data())...);
    }(std::make_index_sequence<params.size()>{});
}
```

> **Fallback if reflection unavailable:** Provide `expr_manual(f, "x")`, `expr_manual(f, "x", "y")` overloads that pass explicit variable names.

**Step 3: Run tests, verify pass**

**Step 4: Commit**

```bash
git add include/refmacro/expr.hpp tests/test_math.cpp
git commit -m "feat: expr() with reflection-based lambda parameter name extraction"
```

---

## Task 11: Umbrella Header + Integration Tests

**Files:**
- Create: `include/refmacro/refmacro.hpp`
- Create: `tests/test_integration.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Create umbrella header**

```cpp
#ifndef REFMACRO_REFMACRO_HPP
#define REFMACRO_REFMACRO_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/macro.hpp>
#include <refmacro/compile.hpp>
#include <refmacro/node_view.hpp>
#include <refmacro/transforms.hpp>
#include <refmacro/pretty_print.hpp>
#include <refmacro/math.hpp>

#endif // REFMACRO_REFMACRO_HPP
```

**Step 2: Write integration tests**

Create `tests/test_integration.cpp`:

```cpp
#include <refmacro/refmacro.hpp>
#include <gtest/gtest.h>

using namespace refmacro;

// --- End-to-end with defmacro ---

TEST(Integration, CustomAbsViaMacro) {
    constexpr auto Abs = defmacro("abs", [](auto x) {
        return [=](auto... a) constexpr {
            auto v = x(a...);
            return v < 0 ? -v : v;
        };
    });

    constexpr auto x = Expr<>::var("x");
    constexpr auto e = Abs(x);
    constexpr auto fn = compile<e, Abs>();
    static_assert(fn(5.0) == 5.0);
    static_assert(fn(-3.0) == 3.0);
}

// --- End-to-end math pipeline ---

TEST(Integration, LinearFunction) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = 3.0 * x + 5.0;
    constexpr auto fn = math_compile<f>();
    static_assert(fn(2.0) == 11.0);
    static_assert(fn(0.0) == 5.0);

    constexpr auto diff_x = [](Expr<> e) consteval { return differentiate(e, "x"); };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto dfn = math_compile<df>();
    static_assert(dfn(100.0) == 3.0);
}

TEST(Integration, QuadraticRoots) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = x * x - 4.0 * x + 4.0;
    constexpr auto fn = math_compile<f>();
    static_assert(fn(2.0) == 0.0);

    constexpr auto diff_x = [](Expr<> e) consteval { return differentiate(e, "x"); };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto df = f | diff_x | simp;
    constexpr auto dfn = math_compile<df>();
    static_assert(dfn(2.0) == 0.0); // minimum
}

TEST(Integration, MultivarGradient) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto f = x * y + x + y;
    constexpr auto fn = math_compile<f>();
    static_assert(fn(2.0, 3.0) == 11.0);

    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto dfx = simplify(differentiate(f, "x"));
    constexpr auto dfx_fn = math_compile<dfx>();
    static_assert(dfx_fn(2.0, 3.0) == 4.0); // y + 1

    constexpr auto dfy = simplify(differentiate(f, "y"));
    constexpr auto dfy_fn = math_compile<dfy>();
    static_assert(dfy_fn(2.0, 3.0) == 3.0); // x + 1
}

TEST(Integration, SecondDerivative) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto f = x * x * x;
    constexpr auto diff_x = [](Expr<> e) consteval { return differentiate(e, "x"); };
    constexpr auto simp = [](Expr<> e) consteval { return simplify(e); };
    constexpr auto d2f = f | diff_x | simp | diff_x | simp;
    constexpr auto fn = math_compile<d2f>();
    static_assert(fn(2.0) == 12.0); // 6*2
}

// --- Mixing custom macros with math ---

TEST(Integration, CustomMacroWithMath) {
    constexpr auto Clamp = defmacro("clamp", [](auto val, auto lo, auto hi) {
        return [=](auto... a) constexpr {
            auto v = val(a...);
            auto l = lo(a...);
            auto h = hi(a...);
            return v < l ? l : (v > h ? h : v);
        };
    });

    constexpr auto x = Expr<>::var("x");
    // clamp(x * x - 10, 0, 100)
    constexpr auto e = Clamp(x * x - 10.0, Expr<>::lit(0.0), Expr<>::lit(100.0));
    constexpr auto fn = compile<e, MAdd, MSub, MMul, MDiv, MNeg, Clamp>();
    static_assert(fn(1.0) == 0.0);   // 1-10 = -9, clamped to 0
    static_assert(fn(4.0) == 6.0);   // 16-10 = 6
    static_assert(fn(20.0) == 100.0); // 400-10 = 390, clamped to 100
}

TEST(Integration, PrettyPrint) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto e = x * x + 1.0;
    constexpr auto s = pretty_print(e);
    static_assert(s == "((x * x) + 1)");
}

TEST(Integration, RuntimeCalls) {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");
    constexpr auto f = x * x + y * y;
    constexpr auto fn = math_compile<f>();
    EXPECT_DOUBLE_EQ(fn(3.0, 4.0), 25.0);
    EXPECT_DOUBLE_EQ(fn(0.0, 0.0), 0.0);
}
```

**Step 3: Add test target, run, commit**

```cmake
add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration PRIVATE refmacro::refmacro GTest::gtest_main)
target_compile_options(test_integration PRIVATE -Wall -Wextra -Werror)
gtest_discover_tests(test_integration PROPERTIES TIMEOUT 120)
```

```bash
git add include/refmacro/refmacro.hpp tests/test_integration.cpp tests/CMakeLists.txt
git commit -m "feat: umbrella header and comprehensive integration tests"
```

---

## Summary

| Task | What | Key Files | Generic? |
|------|------|-----------|----------|
| 1 | Project scaffold | `CMakeLists.txt`, toolchain | — |
| 2 | ASTNode + AST (structural/NTTP) | `ast.hpp` | Yes |
| 3 | Expr: lit, var, make_node | `expr.hpp` | Yes |
| 4 | **defmacro + compile()** | `macro.hpp`, `compile.hpp` | **Yes — core** |
| 5 | Math macros + operators | `math.hpp` | Domain |
| 6 | NodeView | `node_view.hpp` | Yes |
| 7 | rewrite() + transform() | `transforms.hpp` | Yes |
| 8 | Pretty printer | `pretty_print.hpp` | Yes |
| 9 | simplify + differentiate | `math.hpp` | Domain |
| 10 | expr() reflection | `expr.hpp` | Yes |
| 11 | Umbrella + integration | `refmacro.hpp`, tests | — |

**The generic core (Tasks 2-4, 6-8, 10) knows nothing about math.** Math is plugged in via `defmacro` (Task 5) and domain transforms (Task 9). A user can define any DSL by providing their own macros:

```cpp
// Define your DSL
constexpr auto If = defmacro("if_", [](auto c, auto t, auto f) {
    return [=](auto... a) { return c(a...) ? t(a...) : f(a...); };
});
constexpr auto Seq = defmacro("seq", [](auto a, auto b) {
    return [=](auto... args) { a(args...); return b(args...); };
});

// Build, compile
constexpr auto prog = If(Gt(x, y), x, y);
constexpr auto fn = compile<prog, If, Gt>();
```

**End-to-end math usage after all tasks:**

```cpp
#include <refmacro/refmacro.hpp>
using namespace refmacro;

constexpr auto f = expr([](auto x, auto y) { return x * x + 2.0 * y; });
constexpr auto df = f
    | [](Expr<> e) consteval { return differentiate(e, "x"); }
    | [](Expr<> e) consteval { return simplify(e); };
constexpr auto fn = math_compile<df>();
static_assert(fn(3.0, 4.0) == 6.0);
```

---

Plan complete. Two execution options:

**1. Subagent-Driven (this session)** — Fresh subagent per task, review between tasks

**2. Parallel Session (separate)** — New session with `executing-plans`

Which approach?
