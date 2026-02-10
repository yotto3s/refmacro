#include <gtest/gtest.h>
#include <reftype/fm/types.hpp>

using namespace reftype::fm;

// --- LinearTerm ---

TEST(LinearTerm, DefaultConstruction) {
    constexpr LinearTerm t{};
    static_assert(t.var_id == -1);
    static_assert(t.coeff == 0.0);
}

TEST(LinearTerm, ValueConstruction) {
    constexpr LinearTerm t{.var_id = 2, .coeff = 3.5};
    static_assert(t.var_id == 2);
    static_assert(t.coeff == 3.5);
}

// --- LinearInequality ---

TEST(LinearInequality, DefaultConstruction) {
    constexpr LinearInequality ineq{};
    static_assert(ineq.term_count == 0);
    static_assert(ineq.constant == 0.0);
    static_assert(ineq.strict == false);
}

TEST(LinearInequality, WithTerms) {
    // Represents: 2x + 3y - 5 >= 0
    constexpr LinearInequality ineq{
        .terms = {LinearTerm{0, 2.0}, LinearTerm{1, 3.0}},
        .term_count = 2,
        .constant = -5.0,
        .strict = false,
    };
    static_assert(ineq.terms[0].var_id == 0);
    static_assert(ineq.terms[0].coeff == 2.0);
    static_assert(ineq.terms[1].var_id == 1);
    static_assert(ineq.terms[1].coeff == 3.0);
    static_assert(ineq.term_count == 2);
    static_assert(ineq.constant == -5.0);
}

TEST(LinearInequality, StrictInequality) {
    // Represents: x > 0
    constexpr LinearInequality ineq{
        .terms = {LinearTerm{0, 1.0}},
        .term_count = 1,
        .constant = 0.0,
        .strict = true,
    };
    static_assert(ineq.strict == true);
}

// --- VarInfo ---

TEST(VarInfo, FindOrAdd) {
    constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("x");
        v.find_or_add("y");
        return v;
    }();
    static_assert(vars.count == 2);
    static_assert(vars.find("x") == 0);
    static_assert(vars.find("y") == 1);
}

TEST(VarInfo, FindOrAddDuplicate) {
    constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("x");
        v.find_or_add("x"); // duplicate — should return existing
        return v;
    }();
    static_assert(vars.count == 1);
    static_assert(vars.find("x") == 0);
}

TEST(VarInfo, FindMissing) {
    constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("x");
        return v;
    }();
    static_assert(vars.find("z") == -1);
}

TEST(VarInfo, IntegerFlag) {
    constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("n", true);  // integer
        v.find_or_add("r", false); // real
        return v;
    }();
    static_assert(vars.is_integer[0] == true);
    static_assert(vars.is_integer[1] == false);
}

// --- InequalitySystem ---

TEST(InequalitySystem, Empty) {
    constexpr InequalitySystem<> sys{};
    static_assert(sys.count == 0);
    static_assert(sys.vars.count == 0);
}

TEST(InequalitySystem, Add) {
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        LinearInequality ineq{
            .terms = {LinearTerm{0, 1.0}},
            .term_count = 1,
            .constant = 0.0,
        };
        return s.add(ineq);
    }();
    static_assert(sys.count == 1);
    static_assert(sys.ineqs[0].terms[0].coeff == 1.0);
}

TEST(InequalitySystem, AddMultiple) {
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        LinearInequality a{
            .terms = {LinearTerm{0, 1.0}},
            .term_count = 1,
            .constant = -10.0,
        };
        LinearInequality b{
            .terms = {LinearTerm{0, -1.0}},
            .term_count = 1,
            .constant = 20.0,
        };
        return s.add(a).add(b);
    }();
    static_assert(sys.count == 2);
    static_assert(sys.ineqs[0].constant == -10.0);
    static_assert(sys.ineqs[1].constant == 20.0);
}

// --- NTTP compatibility ---

template <LinearTerm T>
consteval double get_coeff() {
    return T.coeff;
}

template <LinearInequality I>
consteval int get_term_count() {
    return I.term_count;
}

TEST(NTTPCompatibility, LinearTermAsNTTP) {
    constexpr auto c = get_coeff<LinearTerm{0, 4.2}>();
    static_assert(c == 4.2);
}

TEST(NTTPCompatibility, LinearInequalityAsNTTP) {
    constexpr LinearInequality ineq{
        .terms = {LinearTerm{0, 1.0}},
        .term_count = 1,
    };
    constexpr auto n = get_term_count<ineq>();
    static_assert(n == 1);
}
