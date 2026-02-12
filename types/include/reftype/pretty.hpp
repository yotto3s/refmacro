#ifndef REFTYPE_PRETTY_HPP
#define REFTYPE_PRETTY_HPP

#include <refmacro/pretty_print.hpp>
#include <reftype/types.hpp>

namespace reftype {

namespace detail {

using refmacro::FixedString;
using refmacro::str_eq;

template <std::size_t Cap>
consteval FixedString<256> pp_node(const refmacro::AST<Cap>& ast, int id) {
    auto n = ast.nodes[id];
    FixedString<256> s;

    // --- Type-specific tags ---

    if (str_eq(n.tag, "tint")) {
        s.append("Int");
        return s;
    }
    if (str_eq(n.tag, "tbool")) {
        s.append("Bool");
        return s;
    }
    if (str_eq(n.tag, "treal")) {
        s.append("Real");
        return s;
    }

    // {#v : Base | pred}
    if (str_eq(n.tag, "tref") && n.child_count == 2) {
        s.append("{#v : ");
        s.append(pp_node(ast, n.children[0]));
        s.append(" | ");
        s.append(pp_node(ast, n.children[1]));
        s.append_char('}');
        return s;
    }

    // (param : In) -> Out
    if (str_eq(n.tag, "tarr") && n.child_count == 3) {
        auto param_node = ast.nodes[n.children[0]];
        s.append_char('(');
        s.append(param_node.name);
        s.append(" : ");
        s.append(pp_node(ast, n.children[1]));
        s.append(") -> ");
        s.append(pp_node(ast, n.children[2]));
        return s;
    }

    // (expr : Type)
    if (str_eq(n.tag, "ann") && n.child_count == 2) {
        s.append_char('(');
        s.append(pp_node(ast, n.children[0]));
        s.append(" : ");
        s.append(pp_node(ast, n.children[1]));
        s.append_char(')');
        return s;
    }

    // Delegate non-type tags to core pretty-print (public API)
    return refmacro::pretty_print(refmacro::Expression<Cap>{ast, id});
}

} // namespace detail

template <std::size_t Cap = 64>
consteval refmacro::FixedString<256>
pretty_print(const refmacro::Expression<Cap>& e) {
    return detail::pp_node(e.ast, e.id);
}

} // namespace reftype

#endif // REFTYPE_PRETTY_HPP
