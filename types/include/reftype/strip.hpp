#ifndef REFTYPE_STRIP_HPP
#define REFTYPE_STRIP_HPP

#include <refmacro/compile.hpp>
#include <refmacro/control.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/math.hpp>
#include <refmacro/transforms.hpp>
#include <reftype/check.hpp>

namespace reftype {

using refmacro::Expression;
using refmacro::make_node;
using refmacro::NodeView;

// --- strip_types: remove type annotations from a typed expression ---
//
// Walks the AST and:
//   - ann(expr, type) => recursively strip expr, drop type
//   - bare type nodes (tint, tbool, treal, tref, tarr) => compile error
//   - everything else => rebuild with recursively stripped children

template <std::size_t Cap = 128>
consteval Expression<Cap> strip_types(Expression<Cap> e) {
    return refmacro::transform(
        e, [](NodeView<Cap> n, auto rec) consteval -> Expression<Cap> {
            // ann(expr, type) => strip annotation, recurse on expr only
            if (n.tag() == "ann")
                return rec(n.child(0));

            // Bare type nodes outside annotations => error
            if (n.tag() == "tint" || n.tag() == "tbool" ||
                n.tag() == "treal" || n.tag() == "tref" ||
                n.tag() == "tarr")
                throw "strip_types: bare type node outside annotation";

            // Leaf nodes: copy the full node (preserves tag, payload, name)
            if (n.child_count() == 0) {
                Expression<Cap> leaf;
                leaf.id = leaf.ast.add_node(n.ast.nodes[n.id]);
                return leaf;
            }

            // Parent nodes: rebuild with recursively stripped children.
            // Invariant: only leaf nodes carry name/payload; parent nodes
            // are fully defined by their tag and children, so make_node
            // reconstruction is lossless here.
            const auto* tag = n.ast.nodes[n.id].tag;

            if (n.child_count() == 1)
                return make_node<Cap>(tag, rec(n.child(0)));
            if (n.child_count() == 2)
                return make_node<Cap>(tag, rec(n.child(0)), rec(n.child(1)));
            if (n.child_count() == 3)
                return make_node<Cap>(tag, rec(n.child(0)), rec(n.child(1)),
                                     rec(n.child(2)));
            if (n.child_count() == 4)
                return make_node<Cap>(tag, rec(n.child(0)), rec(n.child(1)),
                                     rec(n.child(2)), rec(n.child(3)));

            throw "strip_types: unsupported arity (>4 children)";
        });
}

// --- typed_compile: type check + strip + compile in one step ---
//
// Usage:
//   constexpr auto f = typed_compile<expr, MAdd, MSub, ...>();
//
// static_assert fails at compile time if type checking fails.

template <auto expr, auto... Macros> consteval auto typed_compile() {
    constexpr auto result = type_check(expr);
    static_assert(result.valid, "typed_compile: type check failed");
    constexpr auto stripped = strip_types(expr);
    return refmacro::compile<stripped, Macros...>();
}

template <auto expr, auto env, auto... Macros>
    requires requires { env.count; env.lookup(""); }
consteval auto typed_compile() {
    constexpr auto result = type_check(expr, env);
    static_assert(result.valid, "typed_compile: type check failed");
    constexpr auto stripped = strip_types(expr);
    return refmacro::compile<stripped, Macros...>();
}

// --- typed_full_compile: type check + strip + compile with all macros ---
//
// Includes math macros (MAdd, MSub, MMul, MDiv, MNeg) and control-flow
// macros (MCond, MLand, MLor, MLnot, MEq, MLt, MGt, MLe, MGe, MProgn).

template <auto expr> consteval auto typed_full_compile() {
    using namespace refmacro;
    return typed_compile<expr, MAdd, MSub, MMul, MDiv, MNeg, MCond, MLand, MLor,
                         MLnot, MEq, MLt, MGt, MLe, MGe, MProgn>();
}

template <auto expr, auto env>
    requires requires { env.count; env.lookup(""); }
consteval auto typed_full_compile() {
    using namespace refmacro;
    return typed_compile<expr, env, MAdd, MSub, MMul, MDiv, MNeg, MCond, MLand,
                         MLor, MLnot, MEq, MLt, MGt, MLe, MGe, MProgn>();
}

} // namespace reftype

#endif // REFTYPE_STRIP_HPP
