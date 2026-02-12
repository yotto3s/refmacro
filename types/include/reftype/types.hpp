#ifndef REFTYPE_TYPES_HPP
#define REFTYPE_TYPES_HPP

#include <refmacro/expr.hpp>

namespace reftype {

using refmacro::Expression;
using refmacro::make_node;

// Convenience alias for type-annotated expressions
using TypedExpr = Expression<128>;

// --- Base type constructors (leaf nodes, any Cap) ---

template <std::size_t Cap = 128>
consteval Expression<Cap> tint() {
    return make_node<Cap>("tint");
}
template <std::size_t Cap = 128>
consteval Expression<Cap> tbool() {
    return make_node<Cap>("tbool");
}
template <std::size_t Cap = 128>
consteval Expression<Cap> treal() {
    return make_node<Cap>("treal");
}

// Convenience constants matching TypedExpr Cap
inline constexpr auto TInt = tint();
inline constexpr auto TBool = tbool();
inline constexpr auto TReal = treal();

// --- Composite type constructors ---

// Refinement type: {#v : base | pred(#v)}
template <std::size_t Cap = 128>
consteval Expression<Cap> tref(Expression<Cap> base, Expression<Cap> pred) {
    return make_node("tref", base, pred);
}

// Arrow type: (param : In) -> Out
template <std::size_t Cap = 128>
consteval Expression<Cap> tarr(const char* param, Expression<Cap> in,
                               Expression<Cap> out) {
    return make_node("tarr", Expression<Cap>::var(param), in, out);
}

// Type annotation: expr : type
template <std::size_t Cap = 128>
consteval Expression<Cap> ann(Expression<Cap> e, Expression<Cap> type) {
    return make_node("ann", e, type);
}

// --- Common refinement helpers ---

// Positive integer: {#v : Int | #v > 0}
template <std::size_t Cap = 128>
consteval Expression<Cap> pos_int() {
    return tref<Cap>(tint<Cap>(), Expression<Cap>::var("#v") > Expression<Cap>::lit(0));
}

} // namespace reftype

#endif // REFTYPE_TYPES_HPP
