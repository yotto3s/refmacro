#ifndef REFTYPE_TYPES_HPP
#define REFTYPE_TYPES_HPP

#include <refmacro/ast.hpp>
#include <refmacro/control.hpp>
#include <refmacro/expr.hpp>

namespace reftype {

using refmacro::ASTNode;
using refmacro::copy_str;
using refmacro::Expression;
using refmacro::make_node;

// Convenience alias for type-annotated expressions
using TypedExpr = Expression<128>;

// --- Base type constructors (leaf nodes, any Cap) ---

template <std::size_t Cap = 128> consteval Expression<Cap> tint() {
    return make_node<Cap>("tint");
}
template <std::size_t Cap = 128> consteval Expression<Cap> tbool() {
    return make_node<Cap>("tbool");
}
template <std::size_t Cap = 128> consteval Expression<Cap> treal() {
    return make_node<Cap>("treal");
}

// Convenience constants matching TypedExpr Cap
inline constexpr auto TInt = tint();
inline constexpr auto TBool = tbool();
inline constexpr auto TReal = treal();

// --- Composite type constructors ---

// Refinement type: {#v : base | pred(#v)}
template <std::size_t Cap = 128, auto... Ms1, auto... Ms2>
consteval Expression<Cap> tref(Expression<Cap, Ms1...> base,
                               Expression<Cap, Ms2...> pred) {
    Expression<Cap> result;
    result.ast = base.ast;
    int off = result.ast.merge(pred.ast);
    result.id = result.ast.add_tagged_node("tref", {base.id, pred.id + off});
    return result;
}

// Arrow type: (param : In) -> Out
template <std::size_t Cap = 128, auto... Ms1, auto... Ms2>
consteval Expression<Cap> tarr(const char* param, Expression<Cap, Ms1...> in,
                               Expression<Cap, Ms2...> out) {
    Expression<Cap> result;
    result.ast = in.ast;
    ASTNode param_node{};
    copy_str(param_node.tag, "var");
    copy_str(param_node.name, param);
    int param_id = result.ast.add_node(param_node);
    int off1 = 0; // in already in result.ast
    int off2 = result.ast.merge(out.ast);
    result.id = result.ast.add_tagged_node(
        "tarr", {param_id, in.id + off1, out.id + off2});
    return result;
}

// Type annotation: expr : type
template <std::size_t Cap = 128, auto... Ms1, auto... Ms2>
consteval Expression<Cap> ann(Expression<Cap, Ms1...> e,
                              Expression<Cap, Ms2...> type) {
    Expression<Cap> result;
    result.ast = e.ast;
    int off = result.ast.merge(type.ast);
    result.id = result.ast.add_tagged_node("ann", {e.id, type.id + off});
    return result;
}

// --- Common refinement helpers ---

// Positive integer: {#v : Int | #v > 0}
template <std::size_t Cap = 128> consteval Expression<Cap> pos_int() {
    return tref<Cap>(tint<Cap>(),
                     Expression<Cap>::var("#v") > Expression<Cap>::lit(0));
}

} // namespace reftype

#endif // REFTYPE_TYPES_HPP
