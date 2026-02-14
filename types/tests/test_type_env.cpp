#include <gtest/gtest.h>
#include <reftype/type_env.hpp>
#include <reftype/types.hpp>

using namespace reftype;
using refmacro::str_eq;

// --- Basic construction ---

TEST(TypeEnv, EmptyEnv) {
    constexpr TypeEnv env{};
    static_assert(env.count == 0);
    static_assert(!env.has("x"));
}

// --- Bind and lookup ---

TEST(TypeEnv, BindAndLookup) {
    constexpr auto env = TypeEnv<>{}.bind("x", TInt);
    static_assert(env.count == 1);
    static_assert(env.has("x"));
    static_assert(!env.has("y"));

    constexpr auto looked_up = env.lookup("x");
    static_assert(str_eq(looked_up.ast.nodes[looked_up.id].tag, "tint"));
}

TEST(TypeEnv, MultipleBindings) {
    constexpr auto env =
        TypeEnv<>{}.bind("x", TInt).bind("y", TBool).bind("z", TReal);
    static_assert(env.count == 3);
    static_assert(env.has("x"));
    static_assert(env.has("y"));
    static_assert(env.has("z"));
    static_assert(!env.has("w"));

    constexpr auto tx = env.lookup("x");
    static_assert(str_eq(tx.ast.nodes[tx.id].tag, "tint"));

    constexpr auto ty = env.lookup("y");
    static_assert(str_eq(ty.ast.nodes[ty.id].tag, "tbool"));

    constexpr auto tz = env.lookup("z");
    static_assert(str_eq(tz.ast.nodes[tz.id].tag, "treal"));
}

// --- Shadowing ---

TEST(TypeEnv, ShadowingReturnsLatest) {
    constexpr auto env = TypeEnv<>{}.bind("x", TInt).bind("x", TBool);
    static_assert(env.count == 2);
    static_assert(env.has("x"));

    // lookup returns the latest binding (TBool, not TInt)
    constexpr auto tx = env.lookup("x");
    static_assert(str_eq(tx.ast.nodes[tx.id].tag, "tbool"));
}

TEST(TypeEnv, ShadowingDoesNotAffectOthers) {
    constexpr auto env =
        TypeEnv<>{}.bind("x", TInt).bind("y", TReal).bind("x", TBool);
    static_assert(env.count == 3);

    constexpr auto tx = env.lookup("x");
    static_assert(str_eq(tx.ast.nodes[tx.id].tag, "tbool"));

    constexpr auto ty = env.lookup("y");
    static_assert(str_eq(ty.ast.nodes[ty.id].tag, "treal"));
}

// --- Immutability ---

TEST(TypeEnv, BindReturnsNewEnv) {
    constexpr TypeEnv<> env0{};
    constexpr auto env1 = env0.bind("x", TInt);
    constexpr auto env2 = env1.bind("y", TBool);

    // Original envs are unchanged
    static_assert(env0.count == 0);
    static_assert(env1.count == 1);
    static_assert(env2.count == 2);

    static_assert(!env0.has("x"));
    static_assert(env1.has("x"));
    static_assert(!env1.has("y"));
    static_assert(env2.has("x"));
    static_assert(env2.has("y"));
}

// --- Complex type expressions ---

TEST(TypeEnv, RefinementType) {
    constexpr auto posint =
        tref(tint(), Expression<128>::var("#v") > Expression<128>::lit(0));
    constexpr auto env = TypeEnv<>{}.bind("n", posint);
    static_assert(env.count == 1);

    constexpr auto tn = env.lookup("n");
    static_assert(str_eq(tn.ast.nodes[tn.id].tag, "tref"));
}

TEST(TypeEnv, ArrowType) {
    constexpr auto fn_type = tarr("x", TInt, TBool);
    constexpr auto env = TypeEnv<>{}.bind("f", fn_type);

    constexpr auto tf = env.lookup("f");
    static_assert(str_eq(tf.ast.nodes[tf.id].tag, "tarr"));
}

TEST(TypeEnv, NestedTypes) {
    // (n : {#v : Int | #v > 0}) -> Bool
    constexpr auto posint =
        tref(tint(), Expression<128>::var("#v") > Expression<128>::lit(0));
    constexpr auto fn_type = tarr("n", posint, TBool);
    constexpr auto env = TypeEnv<>{}.bind("is_positive", fn_type);

    constexpr auto t = env.lookup("is_positive");
    static_assert(str_eq(t.ast.nodes[t.id].tag, "tarr"));
}

// --- Unbound variable ---

TEST(TypeEnv, UnboundVariableNotFound) {
    // lookup() on unbound name throws in consteval — verified via has()
    // since consteval throw prevents the expression from being constant.
    constexpr TypeEnv<> env{};
    static_assert(!env.has("nonexistent"));

    constexpr auto env2 = TypeEnv<>{}.bind("x", TInt);
    static_assert(!env2.has("y"));
}

// --- Capacity overflow ---

TEST(TypeEnv, CapacityOverflowPrevented) {
    // Use small MaxBindings to verify overflow guard
    constexpr auto env = TypeEnv<128, 2>{}.bind("x", TInt).bind("y", TBool);
    static_assert(env.count == 2);
    // Third bind would throw "TypeEnv capacity exceeded" in consteval.
    // Verify we're at capacity:
    static_assert(env.has("x"));
    static_assert(env.has("y"));
}

// --- Scoping pattern (idiomatic usage) ---

TEST(TypeEnv, ScopingPattern) {
    // Outer scope: x : Int
    constexpr auto outer = TypeEnv<>{}.bind("x", TInt);

    // Inner scope: x : Bool (shadows), y : Real (new)
    constexpr auto inner = outer.bind("x", TBool).bind("y", TReal);

    // Outer is unaffected
    static_assert(outer.count == 1);
    constexpr auto ox = outer.lookup("x");
    static_assert(str_eq(ox.ast.nodes[ox.id].tag, "tint"));

    // Inner sees shadowed x and new y
    static_assert(inner.count == 3);
    constexpr auto ix = inner.lookup("x");
    static_assert(str_eq(ix.ast.nodes[ix.id].tag, "tbool"));
    constexpr auto iy = inner.lookup("y");
    static_assert(str_eq(iy.ast.nodes[iy.id].tag, "treal"));
}

// --- Structural type (NTTP compatible) ---

TEST(TypeEnv, StructuralType) {
    // TypeEnv should be usable as NTTP since all members are structural
    constexpr auto env = TypeEnv<>{}.bind("x", TInt);
    static_assert(env.count == 1);
    // If this compiles, TypeEnv is structural
    []<auto E>() { static_assert(E.count == 1); }.template operator()<env>();
}
