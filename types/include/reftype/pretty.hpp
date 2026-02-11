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

    // --- Core expression tags (duplicated from refmacro::detail::pp_node) ---

    if (str_eq(n.tag, "lit")) {
        s.append_double(n.payload);
        return s;
    }
    if (str_eq(n.tag, "var")) {
        s.append(n.name);
        return s;
    }

    if (str_eq(n.tag, "neg") && n.child_count == 1) {
        s.append("(-");
        s.append(pp_node(ast, n.children[0]));
        s.append_char(')');
        return s;
    }

    if (str_eq(n.tag, "lnot") && n.child_count == 1) {
        s.append("(!");
        s.append(pp_node(ast, n.children[0]));
        s.append_char(')');
        return s;
    }

    if (str_eq(n.tag, "cond") && n.child_count == 3) {
        s.append("(cond ");
        s.append(pp_node(ast, n.children[0]));
        s.append_char(' ');
        s.append(pp_node(ast, n.children[1]));
        s.append_char(' ');
        s.append(pp_node(ast, n.children[2]));
        s.append_char(')');
        return s;
    }

    if (str_eq(n.tag, "progn") && n.child_count == 2) {
        s.append("(progn ");
        s.append(pp_node(ast, n.children[0]));
        s.append_char(' ');
        s.append(pp_node(ast, n.children[1]));
        s.append_char(')');
        return s;
    }

    // let detection: apply(lambda(x, body), val) -> (let x val body)
    if (str_eq(n.tag, "apply") && n.child_count == 2) {
        auto fn_node = ast.nodes[n.children[0]];
        if (str_eq(fn_node.tag, "lambda") && fn_node.child_count == 2) {
            auto param_node = ast.nodes[fn_node.children[0]];
            s.append("(let ");
            s.append(param_node.name);
            s.append_char(' ');
            s.append(pp_node(ast, n.children[1]));
            s.append_char(' ');
            s.append(pp_node(ast, fn_node.children[1]));
            s.append_char(')');
            return s;
        }
        s.append("(apply ");
        s.append(pp_node(ast, n.children[0]));
        s.append_char(' ');
        s.append(pp_node(ast, n.children[1]));
        s.append_char(')');
        return s;
    }

    if (str_eq(n.tag, "lambda") && n.child_count == 2) {
        auto param_node = ast.nodes[n.children[0]];
        s.append("(lambda (");
        s.append(param_node.name);
        s.append(") ");
        s.append(pp_node(ast, n.children[1]));
        s.append_char(')');
        return s;
    }

    if (refmacro::detail::is_infix(n.tag) && n.child_count == 2) {
        s.append_char('(');
        s.append(pp_node(ast, n.children[0]));
        s.append(refmacro::detail::infix_sym(n.tag));
        s.append(pp_node(ast, n.children[1]));
        s.append_char(')');
        return s;
    }

    // Generic fallback: tag(child, child, ...)
    s.append(n.tag);
    s.append_char('(');
    for (int i = 0; i < n.child_count; ++i) {
        if (i > 0)
            s.append(", ");
        s.append(pp_node(ast, n.children[i]));
    }
    s.append_char(')');
    return s;
}

} // namespace detail

template <std::size_t Cap = 64>
consteval refmacro::FixedString<256>
pretty_print(const refmacro::Expression<Cap>& e) {
    return detail::pp_node(e.ast, e.id);
}

} // namespace reftype

#endif // REFTYPE_PRETTY_HPP
