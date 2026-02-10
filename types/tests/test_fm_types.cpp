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

TEST(LinearInequality, MakeBuilder) {
    // make() enforces term_count invariant
    constexpr auto ineq =
        LinearInequality::make({LinearTerm{0, 2.0}, LinearTerm{1, -1.0}}, 3.0, true);
    static_assert(ineq.term_count == 2);
    static_assert(ineq.terms[0].coeff == 2.0);
    static_assert(ineq.terms[1].coeff == -1.0);
    static_assert(ineq.constant == 3.0);
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
    static_assert(vars.find("x") == std::optional<std::size_t>{0});
    static_assert(vars.find("y") == std::optional<std::size_t>{1});
}

TEST(VarInfo, FindOrAddDuplicate) {
    constexpr auto result = [] {
        VarInfo<> v{};
        int first = v.find_or_add("x");
        int second = v.find_or_add("x"); // duplicate — should return existing
        return std::pair{v, std::pair{first, second}};
    }();
    static_assert(result.first.count == 1);
    static_assert(result.first.find("x") == std::optional<std::size_t>{0});
    static_assert(result.second.first == 0);
    static_assert(result.second.second == 0);
}

TEST(VarInfo, FindMissing) {
    constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("x");
        return v;
    }();
    static_assert(vars.find("z") == std::nullopt);
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

TEST(InequalitySystem, WithPopulatedVars) {
    // Build a system with both variables and inequalities
    // Represents: x >= 0, y >= 0, x + y - 10 <= 0 (negated: -x - y + 10 >= 0)
    constexpr auto sys = [] {
        InequalitySystem<> s{};
        int x = s.vars.find_or_add("x");
        int y = s.vars.find_or_add("y");
        return s
            .add(LinearInequality::make({LinearTerm{x, 1.0}}, 0.0))            // x >= 0
            .add(LinearInequality::make({LinearTerm{y, 1.0}}, 0.0))            // y >= 0
            .add(LinearInequality::make({LinearTerm{x, -1.0}, LinearTerm{y, -1.0}}, 10.0)); // -x-y+10>=0
    }();
    static_assert(sys.count == 3);
    static_assert(sys.vars.count == 2);
    static_assert(sys.vars.find("x") == std::optional<std::size_t>{0});
    static_assert(sys.vars.find("y") == std::optional<std::size_t>{1});
    static_assert(sys.ineqs[2].terms[0].coeff == -1.0);
    static_assert(sys.ineqs[2].constant == 10.0);
}

// --- NTTP compatibility ---

template <LinearTerm T>
consteval double get_coeff() {
    return T.coeff;
}

template <LinearInequality I>
consteval int get_term_count() {
    return static_cast<int>(I.term_count);
}

template <VarInfo<> V>
consteval std::size_t get_var_count() {
    return V.count;
}

template <InequalitySystem<> S>
consteval std::size_t get_ineq_count() {
    return S.count;
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

TEST(NTTPCompatibility, VarInfoAsNTTP) {
    static constexpr auto vars = [] {
        VarInfo<> v{};
        v.find_or_add("x");
        v.find_or_add("y");
        return v;
    }();
    constexpr auto n = get_var_count<vars>();
    static_assert(n == 2);
}

TEST(NTTPCompatibility, InequalitySystemAsNTTP) {
    static constexpr auto sys = [] {
        InequalitySystem<> s{};
        return s.add(LinearInequality::make({LinearTerm{0, 1.0}}, 0.0));
    }();
    constexpr auto n = get_ineq_count<sys>();
    static_assert(n == 1);
}
