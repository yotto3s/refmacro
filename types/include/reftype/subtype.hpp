#ifndef REFTYPE_SUBTYPE_HPP
#define REFTYPE_SUBTYPE_HPP

#include <refmacro/expr.hpp>
#include <refmacro/str_utils.hpp>
#include <reftype/fm/solver.hpp>
#include <reftype/types.hpp>

namespace reftype {

using refmacro::Expression;
using refmacro::str_eq;

// --- AST tag accessor ---

template <std::size_t Cap>
consteval const char* type_tag(const Expression<Cap>& e) {
    return e.ast.nodes[e.id].tag;
}

// --- AST node classification ---

template <std::size_t Cap>
consteval bool is_base(const Expression<Cap>& e) {
    const auto* tag = type_tag(e);
    return str_eq(tag, "tint") || str_eq(tag, "tbool") || str_eq(tag, "treal");
}

template <std::size_t Cap>
consteval bool is_refined(const Expression<Cap>& e) {
    return str_eq(type_tag(e), "tref");
}

template <std::size_t Cap>
consteval bool is_arrow(const Expression<Cap>& e) {
    return str_eq(type_tag(e), "tarr");
}

// --- AST accessors ---

// Refinement type: tref(base, pred) — children [0]=base, [1]=pred
template <std::size_t Cap>
consteval Expression<Cap> get_refined_base(const Expression<Cap>& e) {
    return {e.ast, e.ast.nodes[e.id].children[0]};
}

template <std::size_t Cap>
consteval Expression<Cap> get_refined_pred(const Expression<Cap>& e) {
    return {e.ast, e.ast.nodes[e.id].children[1]};
}

// Arrow type: tarr(param_var, in, out) — children [0]=var, [1]=in, [2]=out
template <std::size_t Cap>
consteval Expression<Cap> get_arrow_input(const Expression<Cap>& e) {
    return {e.ast, e.ast.nodes[e.id].children[1]};
}

template <std::size_t Cap>
consteval Expression<Cap> get_arrow_output(const Expression<Cap>& e) {
    return {e.ast, e.ast.nodes[e.id].children[2]};
}

// --- Structural equality ---

namespace detail {

template <std::size_t CapA, std::size_t CapB>
consteval bool nodes_equal(const refmacro::AST<CapA>& ast_a, int id_a,
                           const refmacro::AST<CapB>& ast_b, int id_b) {
    const auto& a = ast_a.nodes[id_a];
    const auto& b = ast_b.nodes[id_b];

    if (!str_eq(a.tag, b.tag)) return false;
    if (a.payload != b.payload) return false;
    if (!str_eq(a.name, b.name)) return false;
    if (a.child_count != b.child_count) return false;

    for (int i = 0; i < a.child_count; ++i)
        if (!nodes_equal(ast_a, a.children[i], ast_b, b.children[i]))
            return false;
    return true;
}

} // namespace detail

template <std::size_t Cap>
consteval bool types_equal(const Expression<Cap>& a, const Expression<Cap>& b) {
    return detail::nodes_equal(a.ast, a.id, b.ast, b.id);
}

// --- Base type widening ---

// Bool <: Int <: Real (transitive)
consteval bool base_widens(const char* sub, const char* super) {
    if (str_eq(sub, "tbool") && str_eq(super, "tint")) return true;
    if (str_eq(sub, "tint") && str_eq(super, "treal")) return true;
    if (str_eq(sub, "tbool") && str_eq(super, "treal")) return true;
    return false;
}

consteval bool base_compatible(const char* sub, const char* super) {
    return str_eq(sub, super) || base_widens(sub, super);
}

// Returns the wider of two compatible base types.
template <std::size_t Cap>
consteval Expression<Cap> wider_base(const Expression<Cap>& t1,
                                     const Expression<Cap>& t2) {
    auto tag1 = type_tag(t1);
    auto tag2 = type_tag(t2);

    if (str_eq(tag1, tag2)) return t1;
    if (str_eq(tag1, "treal") || str_eq(tag2, "treal")) return treal<Cap>();
    if (str_eq(tag1, "tint") || str_eq(tag2, "tint")) return tint<Cap>();
    throw "incompatible base types for widening";
}

// --- Subtype checking ---

template <std::size_t Cap>
consteval bool is_subtype(const Expression<Cap>& sub,
                          const Expression<Cap>& super) {
    // Reflexivity
    if (types_equal(sub, super)) return true;

    // Base <: base — widening
    if (is_base(sub) && is_base(super))
        return base_widens(type_tag(sub), type_tag(super));

    // Unrefined <: refined — Q must be valid (always true)
    if (is_base(sub) && is_refined(super)) {
        auto super_base = get_refined_base(super);
        if (!base_compatible(type_tag(sub), type_tag(super_base)))
            return false;
        // #v ranges over super's refinement domain
        fm::VarInfo<> vars{};
        vars.find_or_add("#v", !str_eq(type_tag(super_base), "treal"));
        return fm::is_valid(get_refined_pred(super), vars);
    }

    // Refined <: unrefined — true if base compatible
    if (is_refined(sub) && is_base(super))
        return base_compatible(type_tag(get_refined_base(sub)),
                               type_tag(super));

    // Refined <: refined — P => Q via FM solver
    if (is_refined(sub) && is_refined(super)) {
        auto sub_base = get_refined_base(sub);
        auto super_base = get_refined_base(super);
        if (!base_compatible(type_tag(sub_base), type_tag(super_base)))
            return false;
        // #v ranges over sub's domain (the narrower type)
        fm::VarInfo<> vars{};
        vars.find_or_add("#v", !str_eq(type_tag(sub_base), "treal"));
        return fm::is_valid_implication<Cap>(get_refined_pred(sub),
                                             get_refined_pred(super), vars);
    }

    // Arrow <: arrow — contra/co variance
    if (is_arrow(sub) && is_arrow(super))
        return is_subtype(get_arrow_input(super), get_arrow_input(sub))
            && is_subtype(get_arrow_output(sub), get_arrow_output(super));

    return false;
}

// --- Join (least upper bound) ---

template <std::size_t Cap>
consteval Expression<Cap> join(const Expression<Cap>& t1,
                               const Expression<Cap>& t2) {
    if (types_equal(t1, t2)) return t1;

    // Base + base — wider
    if (is_base(t1) && is_base(t2))
        return wider_base(t1, t2);

    // Refined + refined — disjunction of predicates
    if (is_refined(t1) && is_refined(t2)) {
        auto base = wider_base(get_refined_base(t1), get_refined_base(t2));
        auto pred = get_refined_pred(t1) || get_refined_pred(t2);
        return tref(base, pred);
    }

    // Refined + unrefined — drop refinement, widen base
    if (is_refined(t1) && is_base(t2))
        return wider_base(get_refined_base(t1), t2);
    if (is_base(t1) && is_refined(t2))
        return wider_base(t1, get_refined_base(t2));

    throw "type error: incompatible types for join";
}

} // namespace reftype

#endif // REFTYPE_SUBTYPE_HPP
