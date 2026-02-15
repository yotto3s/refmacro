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

template <std::size_t Cap = 128, auto... Ms>
consteval Expression<Cap> strip_types(Expression<Cap, Ms...> e) {
    Expression<Cap> plain = e; // strip macros
    return refmacro::transform(
        plain, [](NodeView<Cap> n, auto rec) consteval -> Expression<Cap> {
            // ann(expr, type) => strip annotation, recurse on expr only
            if (n.tag() == "ann")
                return rec(n.child(0));

            // Bare type nodes outside annotations => error
            if (n.tag() == "tint" || n.tag() == "tbool" || n.tag() == "treal" ||
                n.tag() == "tref" || n.tag() == "tarr")
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

            auto first = rec(n.child(0));
            Expression<Cap> result;
            result.ast = first.ast;
            int ids[8]{first.id};
            for (int i = 1; i < n.child_count(); ++i) {
                auto child = rec(n.child(i));
                ids[i] = child.id + result.ast.merge(child.ast);
            }
            result.id = result.ast.add_tagged_node(tag, ids, n.child_count());
            return result;
        });
}

// --- Helper: compile stripped AST with macros extracted from original expr ---
//
// strip_types returns Expression<Cap> (no macros), losing any macros embedded
// in the original expression's type. This helper recovers them by extracting
// Embedded... from the original Expression<Cap, Embedded...> and forwarding
// them as extra macros to compile, alongside any explicitly-passed Macros.

namespace detail {

template <auto stripped, auto... Extra, std::size_t Cap, auto... Embedded>
consteval auto compile_with_macros_from(Expression<Cap, Embedded...>) {
    return refmacro::compile<stripped, Embedded..., Extra...>();
}

} // namespace detail

// --- typed_compile: type check + strip + compile in one step ---
//
// Usage:
//   constexpr auto f = typed_compile<expr, MAdd, MSub, ...>();
//
// Auto-tracked macros embedded in expr's type are automatically forwarded
// to compile, so typed_compile<expr>() works without explicit Macros when
// the expression was built with auto-tracking operators.
//
// Fails at compile time if type checking fails: either via static_assert
// (when type_check returns invalid) or via consteval throw (e.g. unbound
// variable, unsupported tag).

template <auto expr, auto... Macros>
    requires(sizeof...(Macros) == 0 || (... &&
                                        requires {
                                            Macros.tag;
                                            Macros.fn;
                                        }))
consteval auto typed_compile() {
    constexpr auto result = type_check(expr);
    static_assert(result.valid, "typed_compile: type check failed");
    constexpr auto stripped = strip_types(expr);
    return detail::compile_with_macros_from<stripped, Macros...>(expr);
}

template <auto expr, auto env, auto... Macros>
    requires requires {
        env.count;
        env.lookup("");
    }
consteval auto typed_compile() {
    constexpr auto result = type_check(expr, env);
    static_assert(result.valid, "typed_compile: type check failed");
    constexpr auto stripped = strip_types(expr);
    return detail::compile_with_macros_from<stripped, Macros...>(expr);
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
    requires requires {
        env.count;
        env.lookup("");
    }
consteval auto typed_full_compile() {
    using namespace refmacro;
    return typed_compile<expr, env, MAdd, MSub, MMul, MDiv, MNeg, MCond, MLand,
                         MLor, MLnot, MEq, MLt, MGt, MLe, MGe, MProgn>();
}

} // namespace reftype

#endif // REFTYPE_STRIP_HPP
