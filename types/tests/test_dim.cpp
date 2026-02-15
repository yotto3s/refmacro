#include <gtest/gtest.h>

#include <refmacro/math.hpp>
#include <reftype/dim.hpp>

using refmacro::Expression;
using reftype::ann;
using reftype::is_subtype;
using reftype::join;
using reftype::TypeEnv;
using reftype::types_equal;

using E = Expression<128>;
using reftype::dim::tdim;

// ============================================================
// tdim construction and accessors
// ============================================================

TEST(DimType, Construction) {
    static constexpr auto meter = tdim(1, 0, 0);
    static_assert(reftype::dim::is_dim(meter));
    static_assert(reftype::dim::dim_exp(meter, 0) == 1);
    static_assert(reftype::dim::dim_exp(meter, 1) == 0);
    static_assert(reftype::dim::dim_exp(meter, 2) == 0);
}

TEST(DimType, NegativeExponents) {
    static constexpr auto velocity = tdim(1, -1, 0);
    static_assert(reftype::dim::dim_exp(velocity, 0) == 1);
    static_assert(reftype::dim::dim_exp(velocity, 1) == -1);
    static_assert(reftype::dim::dim_exp(velocity, 2) == 0);
}

TEST(DimType, Dimensionless) {
    static constexpr auto scalar = tdim(0, 0, 0);
    static_assert(reftype::dim::is_dim(scalar));
    static_assert(reftype::dim::dim_exp(scalar, 0) == 0);
    static_assert(reftype::dim::dim_exp(scalar, 1) == 0);
    static_assert(reftype::dim::dim_exp(scalar, 2) == 0);
}

// ============================================================
// Subtype (from Task 1 -- kept for regression)
// ============================================================

TEST(DimSubtype, SameDimIsSubtype) {
    static constexpr auto meter = tdim(1, 0, 0);
    static constexpr auto meter2 = tdim(1, 0, 0);
    static_assert(is_subtype(meter, meter2));
}

TEST(DimSubtype, DifferentDimNotSubtype) {
    static constexpr auto meter = tdim(1, 0, 0);
    static constexpr auto second = tdim(0, 1, 0);
    static_assert(!is_subtype(meter, second));
}

TEST(DimSubtype, DimNotSubtypeOfBase) {
    static constexpr auto meter = tdim(1, 0, 0);
    static_assert(!is_subtype(meter, reftype::TInt));
    static_assert(!is_subtype(meter, reftype::TReal));
}

TEST(DimJoin, SameDimJoinsToSelf) {
    static constexpr auto meter = tdim(1, 0, 0);
    static constexpr auto result = join(meter, meter);
    static_assert(types_equal(result, meter));
}

// ============================================================
// dim_type_check: arithmetic propagation
// ============================================================

TEST(DimTypeCheck, AddMatchingDims) {
    static constexpr auto env =
        TypeEnv<128>{}.bind("x", tdim(1, 0, 0)).bind("y", tdim(1, 0, 0));
    static constexpr auto expr = ann(E::var("x") + E::var("y"), tdim(1, 0, 0));
    static constexpr auto result = reftype::dim::dim_type_check(expr, env);
    static_assert(result.valid);
}

TEST(DimTypeCheck, MulAddExponents) {
    static constexpr auto env =
        TypeEnv<128>{}.bind("v", tdim(1, -1, 0)).bind("t", tdim(0, 1, 0));
    static constexpr auto expr = ann(E::var("v") * E::var("t"), tdim(1, 0, 0));
    static constexpr auto result = reftype::dim::dim_type_check(expr, env);
    static_assert(result.valid);
}

TEST(DimTypeCheck, DivSubtractExponents) {
    static constexpr auto env =
        TypeEnv<128>{}.bind("x", tdim(1, 0, 0)).bind("t", tdim(0, 1, 0));
    static constexpr auto expr = ann(E::var("x") / E::var("t"), tdim(1, -1, 0));
    static constexpr auto result = reftype::dim::dim_type_check(expr, env);
    static_assert(result.valid);
}

TEST(DimTypeCheck, ScalarMulPreservesDim) {
    static constexpr auto env = TypeEnv<128>{}.bind("x", tdim(1, 0, 0));
    static constexpr auto expr = ann(E::lit(2) * E::var("x"), tdim(1, 0, 0));
    static constexpr auto result = reftype::dim::dim_type_check(expr, env);
    static_assert(result.valid);
}

TEST(DimTypeCheck, NegPreservesDim) {
    static constexpr auto env = TypeEnv<128>{}.bind("x", tdim(1, 0, 0));
    static constexpr auto expr = ann(-E::var("x"), tdim(1, 0, 0));
    static constexpr auto result = reftype::dim::dim_type_check(expr, env);
    static_assert(result.valid);
}

TEST(DimTypeCheck, NonDimFallback) {
    static constexpr auto env =
        TypeEnv<128>{}.bind("x", reftype::TInt).bind("y", reftype::TInt);
    static constexpr auto expr = ann(E::var("x") + E::var("y"), reftype::TInt);
    static constexpr auto result = reftype::dim::dim_type_check(expr, env);
    static_assert(result.valid);
}

// ============================================================
// dim_typed_compile: end-to-end
// ============================================================

TEST(DimTypedCompile, SimpleAdd) {
    static constexpr auto env =
        TypeEnv<128>{}.bind("x", tdim(1, 0, 0)).bind("y", tdim(1, 0, 0));
    static constexpr auto expr = ann(E::var("x") + E::var("y"), tdim(1, 0, 0));
    constexpr auto fn = reftype::dim::dim_typed_compile<expr, env>();
    static_assert(fn(3.0, 4.0) == 7.0);
}

TEST(DimTypedCompile, Kinematics) {
    static constexpr auto env = TypeEnv<128>{}
                                    .bind("x0", tdim(1, 0, 0))
                                    .bind("v0", tdim(1, -1, 0))
                                    .bind("a", tdim(1, -2, 0))
                                    .bind("t", tdim(0, 1, 0));
    static constexpr auto expr =
        ann(E::var("x0") + E::var("v0") * E::var("t") +
                E::lit(0.5) * E::var("a") * E::var("t") * E::var("t"),
            tdim(1, 0, 0));
    // Variable order is DFS: x0, v0, t, a
    constexpr auto fn = reftype::dim::dim_typed_compile<expr, env>();
    static_assert(fn(10.0, 5.0, 2.0, 9.8) == 10.0 + 10.0 + 19.6);
}
