#include <gtest/gtest.h>

#include <type_traits>

#include <refmacro/math.hpp>
#include <reftype/pretty.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using reftype::ann;
using reftype::pos_int;
using reftype::tarr;
using reftype::TBool;
using reftype::TInt;
using reftype::TReal;
using reftype::tref;

// All reftype constructors default to Cap=128, matching TypedExpr
using E = Expression<128>;

// --- TypedExpr alias ---

TEST(TypeAlias, TypedExprIsExpression128) {
    static_assert(
        std::is_same_v<reftype::TypedExpr, refmacro::Expression<128>>);
}

// --- Base type constants ---

TEST(TypeConstants, TIntTagAndChildren) {
    static constexpr auto t = TInt;
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "tint"));
    static_assert(t.ast.nodes[t.id].child_count == 0);
}

TEST(TypeConstants, TBoolTagAndChildren) {
    static constexpr auto t = TBool;
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "tbool"));
    static_assert(t.ast.nodes[t.id].child_count == 0);
}

TEST(TypeConstants, TRealTagAndChildren) {
    static constexpr auto t = TReal;
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "treal"));
    static_assert(t.ast.nodes[t.id].child_count == 0);
}

// --- Refinement type ---

TEST(TypeConstructors, TrefStructure) {
    static constexpr auto t = tref(TInt, E::var("#v") > E::lit(0));
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "tref"));
    static_assert(t.ast.nodes[t.id].child_count == 2);
}

TEST(TypeConstructors, PosIntHelper) {
    static constexpr auto t = pos_int();
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "tref"));
    static_assert(t.ast.nodes[t.id].child_count == 2);
    constexpr auto base_id = t.ast.nodes[t.id].children[0];
    static_assert(refmacro::str_eq(t.ast.nodes[base_id].tag, "tint"));
}

// --- Arrow type ---

TEST(TypeConstructors, TarrStructure) {
    static constexpr auto t = tarr("x", TInt, TBool);
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "tarr"));
    static_assert(t.ast.nodes[t.id].child_count == 3);
    constexpr auto param_id = t.ast.nodes[t.id].children[0];
    static_assert(refmacro::str_eq(t.ast.nodes[param_id].tag, "var"));
    static_assert(refmacro::str_eq(t.ast.nodes[param_id].name, "x"));
}

// --- Annotation ---

TEST(TypeConstructors, AnnStructure) {
    static constexpr auto t = ann(E::var("x"), TInt);
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "ann"));
    static_assert(t.ast.nodes[t.id].child_count == 2);
}

TEST(TypeConstructors, AnnArithmeticExpr) {
    static constexpr auto t = ann(E::var("x") + E::lit(1), TInt);
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "ann"));
    static_assert(t.ast.nodes[t.id].child_count == 2);
    constexpr auto expr_id = t.ast.nodes[t.id].children[0];
    static_assert(refmacro::str_eq(t.ast.nodes[expr_id].tag, "add"));
}

// --- Nested types ---

TEST(TypeConstructors, NestedAnnotation) {
    static constexpr auto t =
        ann(E::var("x"), tref(TInt, E::var("#v") > E::lit(0)));
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "ann"));
    constexpr auto type_id = t.ast.nodes[t.id].children[1];
    static_assert(refmacro::str_eq(t.ast.nodes[type_id].tag, "tref"));
}

TEST(TypeConstructors, DependentArrow) {
    // (x : {#v : Int | #v > 0}) -> {#v : Int | #v > x}
    static constexpr auto t = tarr("x", tref(TInt, E::var("#v") > E::lit(0)),
                                   tref(TInt, E::var("#v") > E::var("x")));
    static_assert(refmacro::str_eq(t.ast.nodes[t.id].tag, "tarr"));
    static_assert(t.ast.nodes[t.id].child_count == 3);
}

// --- Pretty-print ---

TEST(TypePrettyPrint, BaseTypes) {
    static constexpr auto pp_int = reftype::pretty_print(TInt);
    static_assert(pp_int == "Int");

    static constexpr auto pp_bool = reftype::pretty_print(TBool);
    static_assert(pp_bool == "Bool");

    static constexpr auto pp_real = reftype::pretty_print(TReal);
    static_assert(pp_real == "Real");
}

TEST(TypePrettyPrint, RefinementType) {
    static constexpr auto t = tref(TInt, E::var("#v") > E::lit(0));
    static constexpr auto pp = reftype::pretty_print(t);
    static_assert(pp == "{#v : Int | (#v > 0)}");
}

TEST(TypePrettyPrint, ArrowType) {
    static constexpr auto t = tarr("x", TInt, TBool);
    static constexpr auto pp = reftype::pretty_print(t);
    static_assert(pp == "(x : Int) -> Bool");
}

TEST(TypePrettyPrint, Annotation) {
    static constexpr auto t = ann(E::var("x"), TInt);
    static constexpr auto pp = reftype::pretty_print(t);
    static_assert(pp == "(x : Int)");
}

TEST(TypePrettyPrint, NestedRefinement) {
    static constexpr auto t =
        ann(E::var("x"), tref(TInt, E::var("#v") > E::lit(0)));
    static constexpr auto pp = reftype::pretty_print(t);
    static_assert(pp == "(x : {#v : Int | (#v > 0)})");
}

TEST(TypePrettyPrint, DependentArrow) {
    static constexpr auto t = tarr("x", tref(TInt, E::var("#v") > E::lit(0)),
                                   tref(TInt, E::var("#v") > E::var("x")));
    static constexpr auto pp = reftype::pretty_print(t);
    static_assert(pp == "(x : {#v : Int | (#v > 0)}) -> {#v : Int | (#v > x)}");
}

TEST(TypePrettyPrint, PosIntHelper) {
    static constexpr auto pp = reftype::pretty_print(pos_int());
    static_assert(pp == "{#v : Int | (#v > 0)}");
}

TEST(TypePrettyPrint, ExpressionFallback) {
    // Core expression tags should still render correctly through
    // reftype::pretty_print
    static constexpr auto e = E::var("x") > E::lit(1);
    static constexpr auto pp = reftype::pretty_print(e);
    static_assert(pp == "(x > 1)");
}
