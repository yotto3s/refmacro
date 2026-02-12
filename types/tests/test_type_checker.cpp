#include <gtest/gtest.h>

#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <reftype/check.hpp>
#include <reftype/pretty.hpp>
#include <reftype/subtype.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using reftype::ann;
using reftype::BaseKind;
using reftype::get_base_kind;
using reftype::is_subtype;
using reftype::join;
using reftype::pos_int;
using reftype::synth;
using reftype::tarr;
using reftype::TBool;
using reftype::TInt;
using reftype::TReal;
using reftype::tref;
using reftype::type_check;
using reftype::TypeEnv;
using reftype::TypeResult;
using reftype::types_equal;

using E = Expression<128>;

// --- Literal typing ---

TEST(TypeChecker, LitInteger) {
    constexpr auto r = type_check(E::lit(5));
    static_assert(r.valid);
    // Singleton type: {#v : Int | #v == 5}
    static_assert(reftype::is_refined(r.type));
    static_assert(get_base_kind(r.type) == BaseKind::Int);
}

TEST(TypeChecker, LitReal) {
    constexpr auto r = type_check(E::lit(3.14));
    static_assert(r.valid);
    static_assert(reftype::is_refined(r.type));
    static_assert(get_base_kind(r.type) == BaseKind::Real);
}

TEST(TypeChecker, LitZero) {
    constexpr auto r = type_check(E::lit(0));
    static_assert(r.valid);
    static_assert(get_base_kind(r.type) == BaseKind::Int);
}

// --- Variable lookup ---

TEST(TypeChecker, VarLookup) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", TInt);
    constexpr auto r = type_check(E::var("x"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeChecker, VarWithRefinedType) {
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", nat);
    constexpr auto r = type_check(E::var("x"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, nat));
}

// --- Annotation ---

TEST(TypeChecker, AnnLiteralValidRefinement) {
    // ann(lit(5), {#v : Int | #v > 0}) — 5 > 0, valid
    constexpr auto r = type_check(ann(E::lit(5), pos_int()));
    static_assert(r.valid);
    static_assert(types_equal(r.type, pos_int()));
}

TEST(TypeChecker, AnnLiteralInvalidRefinement) {
    // ann(lit(0), {#v : Int | #v > 0}) — 0 > 0 fails
    constexpr auto r = type_check(ann(E::lit(0), pos_int()));
    static_assert(!r.valid);
}

TEST(TypeChecker, AnnLiteralNegativeInvalidRefinement) {
    // ann(lit(-3), {#v : Int | #v > 0}) — -3 > 0 fails
    constexpr auto r = type_check(ann(E::lit(-3), pos_int()));
    static_assert(!r.valid);
}

TEST(TypeChecker, AnnBaseTypeValid) {
    // ann(lit(5), TInt) — {#v:Int|#v==5} <: Int, valid
    constexpr auto r = type_check(ann(E::lit(5), TInt));
    static_assert(r.valid);
}

TEST(TypeChecker, AnnBaseTypeMismatch) {
    // ann(lit(5), TBool) — {#v:Int|#v==5} </: Bool, invalid
    constexpr auto r = type_check(ann(E::lit(5), TBool));
    static_assert(!r.valid);
}

// --- Arithmetic ---

TEST(TypeChecker, AddIntInt) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TInt)
        .bind("y", TInt);
    constexpr auto r = type_check(E::var("x") + E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeChecker, AddRealReal) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TReal)
        .bind("y", TReal);
    constexpr auto r = type_check(E::var("x") + E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TReal));
}

TEST(TypeChecker, SubIntInt) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TInt)
        .bind("y", TInt);
    constexpr auto r = type_check(E::var("x") - E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeChecker, MulIntInt) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TInt)
        .bind("y", TInt);
    constexpr auto r = type_check(E::var("x") * E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeChecker, ArithWithRefinedOperands) {
    // Refined operands: base type extracted for result
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", nat)
        .bind("y", nat);
    constexpr auto r = type_check(E::var("x") + E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeChecker, ArithLiterals) {
    // add(lit(1), lit(2)) — both have Int singleton types
    constexpr auto r = type_check(E::lit(1) + E::lit(2));
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

// --- Negation ---

TEST(TypeChecker, NegInt) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", TInt);
    constexpr auto neg = refmacro::make_node<128>("neg", E::var("x"));
    constexpr auto r = type_check(neg, env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

// --- Comparisons ---

TEST(TypeChecker, GtIntInt) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TInt)
        .bind("y", TInt);
    constexpr auto r = type_check(E::var("x") > E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool));
}

TEST(TypeChecker, EqIntInt) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TInt)
        .bind("y", TInt);
    constexpr auto r = type_check(E::var("x") == E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool));
}

TEST(TypeChecker, ComparisonLiterals) {
    constexpr auto r = type_check(E::lit(3) > E::lit(1));
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool));
}

// --- Logical ---

TEST(TypeChecker, LandBoolBool) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("p", TBool)
        .bind("q", TBool);
    constexpr auto r = type_check(E::var("p") && E::var("q"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool));
}

TEST(TypeChecker, LnotBool) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("p", TBool);
    constexpr auto r = type_check(!E::var("p"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool));
}

TEST(TypeChecker, ComparisonInLogical) {
    // (x > 0) && (x < 10) — comparisons produce Bool, logical accepts Bool
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", TInt);
    constexpr auto r = type_check(
        (E::var("x") > E::lit(0)) && (E::var("x") < E::lit(10)), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool));
}

// --- Conditional ---

TEST(TypeChecker, CondSameType) {
    // cond(true_expr, lit(1), lit(2)) — both Int
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("p", TBool);
    constexpr auto e = refmacro::make_node<128>(
        "cond", E::var("p"), E::lit(1), E::lit(2));
    constexpr auto r = type_check(e, env);
    static_assert(r.valid);
    // join of two Int singletons
    static_assert(get_base_kind(r.type) == BaseKind::Int);
}

TEST(TypeChecker, CondJoinTypes) {
    // cond(p, x:Nat, y:Int) where Nat={#v>=0}, result = join(Nat, Int) = Int
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("p", TBool)
        .bind("x", nat)
        .bind("y", TInt);
    constexpr auto e = refmacro::make_node<128>(
        "cond", E::var("p"), E::var("x"), E::var("y"));
    constexpr auto r = type_check(e, env);
    static_assert(r.valid);
    // join(Nat, Int) = Int (refined + unrefined = drop refinement)
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeChecker, CondOverArrowTypes) {
    // cond(p, f, g) where f:(x:Int)->Int and g:(y:Int)->Int
    // Should succeed: join of alpha-equivalent arrows
    constexpr auto arrow_f = tarr("x", TInt, TInt);
    constexpr auto arrow_g = tarr("y", TInt, TInt);
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("p", TBool)
        .bind("f", arrow_f)
        .bind("g", arrow_g);
    constexpr auto e = refmacro::make_node<128>(
        "cond", E::var("p"), E::var("f"), E::var("g"));
    constexpr auto r = type_check(e, env);
    static_assert(r.valid);
    static_assert(reftype::is_arrow(r.type));
    static_assert(types_equal(reftype::get_arrow_input(r.type), TInt));
    static_assert(types_equal(reftype::get_arrow_output(r.type), TInt));
}

// --- Let-binding (apply/lambda) ---

TEST(TypeChecker, LetBinding) {
    // let x = 5 in x → type = {#v:Int|#v==5} (singleton from literal)
    constexpr auto e = refmacro::let_<128>("x", E::lit(5), E::var("x"));
    constexpr auto r = type_check(e);
    static_assert(r.valid);
    static_assert(get_base_kind(r.type) == BaseKind::Int);
}

TEST(TypeChecker, LetBindingArithmetic) {
    // let x = 5 in x + 1 → type = Int
    constexpr auto e = refmacro::let_<128>(
        "x", E::lit(5), E::var("x") + E::lit(1));
    constexpr auto r = type_check(e);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeChecker, LetBindingShadowing) {
    // let x = 5 in let x = 3.14 in x → type = {#v:Real|#v==3.14}
    constexpr auto e = refmacro::let_<128>(
        "x", E::lit(5),
        refmacro::let_<128>("x", E::lit(3.14), E::var("x")));
    constexpr auto r = type_check(e);
    static_assert(r.valid);
    static_assert(get_base_kind(r.type) == BaseKind::Real);
}

// --- Annotated lambda ---

TEST(TypeChecker, AnnotatedLambda) {
    // ann(lambda("x", x + 1), (x:Int) -> Int)
    constexpr auto body = E::var("x") + E::lit(1);
    constexpr auto lam = refmacro::lambda<128>("x", body);
    constexpr auto arrow_type = tarr("x", TInt, TInt);
    constexpr auto e = ann(lam, arrow_type);
    constexpr auto r = type_check(e);
    static_assert(r.valid);
    static_assert(reftype::is_arrow(r.type));
}

TEST(TypeChecker, AnnotatedLambdaOutputMismatch) {
    // ann(lambda("x", x + 1), (x:Int) -> Bool) — Int </: Bool
    constexpr auto body = E::var("x") + E::lit(1);
    constexpr auto lam = refmacro::lambda<128>("x", body);
    constexpr auto arrow_type = tarr("x", TInt, TBool);
    constexpr auto e = ann(lam, arrow_type);
    constexpr auto r = type_check(e);
    static_assert(!r.valid);
}

// --- General function application ---

TEST(TypeChecker, FunctionApplication) {
    // Given f : (x:Int) -> Int, apply(f, lit(5)) → Int
    constexpr auto arrow = tarr("x", TInt, TInt);
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("f", arrow);
    constexpr auto e = refmacro::apply<128>(E::var("f"), E::lit(5));
    constexpr auto r = type_check(e, env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeChecker, FunctionApplicationSubtype) {
    // f : (x:Int) -> Int, apply(f, lit(5)) — {#v==5} <: Int, valid
    constexpr auto arrow = tarr("x", TInt, TInt);
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("f", arrow);
    constexpr auto e = refmacro::apply<128>(E::var("f"), E::lit(5));
    constexpr auto r = type_check(e, env);
    static_assert(r.valid);
}

// --- BaseKind helper ---

TEST(TypeChecker, BaseKindClassification) {
    static_assert(get_base_kind(TInt) == BaseKind::Int);
    static_assert(get_base_kind(TBool) == BaseKind::Bool);
    static_assert(get_base_kind(TReal) == BaseKind::Real);
    static_assert(get_base_kind(pos_int()) == BaseKind::Int);
    static_assert(get_base_kind(tarr("x", TInt, TInt)) == BaseKind::None);
}

// --- Logical or ---

TEST(TypeChecker, LorBoolBool) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("p", TBool)
        .bind("q", TBool);
    constexpr auto r = type_check(E::var("p") || E::var("q"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool));
}

// --- Division ---

TEST(TypeChecker, DivIntInt) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TInt)
        .bind("y", TInt);
    constexpr auto r = type_check(E::var("x") / E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TInt));
}

TEST(TypeChecker, DivRealReal) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TReal)
        .bind("y", TReal);
    constexpr auto r = type_check(E::var("x") / E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TReal));
}

// --- le/ge comparisons ---

TEST(TypeChecker, LeIntInt) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TInt)
        .bind("y", TInt);
    constexpr auto r = type_check(E::var("x") <= E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool));
}

TEST(TypeChecker, GeIntInt) {
    constexpr TypeEnv<128> env = TypeEnv<128>{}
        .bind("x", TInt)
        .bind("y", TInt);
    constexpr auto r = type_check(E::var("x") >= E::var("y"), env);
    static_assert(r.valid);
    static_assert(types_equal(r.type, TBool));
}

// --- Real-valued arithmetic ---

TEST(TypeChecker, LitRealArithmetic) {
    // add(lit(3.14), lit(1.5)) — both Real singleton types
    constexpr auto r = type_check(E::lit(3.14) + E::lit(1.5));
    static_assert(r.valid);
    static_assert(types_equal(r.type, TReal));
}

// --- Sequence (progn) ---

TEST(TypeChecker, Progn) {
    // progn(lit(1), lit(2)) — result type is second expression's type
    constexpr auto e = refmacro::make_node<128>("progn", E::lit(1), E::lit(2));
    constexpr auto r = type_check(e);
    static_assert(r.valid);
    static_assert(get_base_kind(r.type) == BaseKind::Int);
}

TEST(TypeChecker, PrognPropagatesValidity) {
    // progn(ann(lit(0), pos_int()), lit(1)) — first invalid, result invalid
    constexpr auto first = ann(E::lit(0), pos_int());
    constexpr auto e = refmacro::make_node<128>("progn", first, E::lit(1));
    constexpr auto r = type_check(e);
    static_assert(!r.valid);
}

// --- Validity propagation ---

TEST(TypeChecker, ValidityPropagatesThrough) {
    // ann(lit(0), pos_int()) is invalid; using result in add still propagates
    // This test just verifies that invalid annotations produce valid=false
    constexpr auto r = type_check(ann(E::lit(0), pos_int()));
    static_assert(!r.valid);
}

// --- Annotation with refinement VC ---

TEST(TypeChecker, RefinementAnnotationVC) {
    // ann(x + 1, {#v:Int|#v>0}) where x:Nat
    // synth(x+1) = Int. is_subtype(Int, {#v:Int|#v>0}) = false
    // (we don't track that x>=0 implies x+1>0 — requires dependent typing)
    constexpr auto nat = tref(TInt, E::var("#v") >= E::lit(0));
    constexpr TypeEnv<128> env = TypeEnv<128>{}.bind("x", nat);
    constexpr auto e = ann(E::var("x") + E::lit(1), pos_int());
    constexpr auto r = type_check(e, env);
    // Int (from arithmetic) is not subtype of {#v>0}
    static_assert(!r.valid);
}
