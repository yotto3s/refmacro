// Compile-time dimensional analysis
//
// Physical unit types tracked at compile time via `tdim(L, T, M)` exponents.
// Addition requires matching dimensions; multiplication/division combine them.
// The FM solver verifies value refinements on dimensional types.
//
// Build: cmake --build build --target 05_dimensional_analysis
// Run:   ./build/types/examples/05_dimensional_analysis

#include <cstdio>
#include <refmacro/math.hpp>
#include <reftype/dim.hpp>

using refmacro::Expression;
using reftype::ann;
using reftype::TypeEnv;

using E = Expression<128>;
using namespace reftype::dim;

// ===================================================================
// Section 1: SI units and dimensional types
// ===================================================================
//
// tdim(L, T, M) creates a type with SI exponents:
//   L = length (meters), T = time (seconds), M = mass (kilograms)
//
// Common units:
//   t_meter  = dim(1,0,0)     t_mps    = dim(1,-1,0)
//   t_second = dim(0,1,0)     t_mps2   = dim(1,-2,0)
//   t_kg     = dim(0,0,1)     t_newton = dim(1,-2,1)
//   t_scalar = dim(0,0,0)     t_joule  = dim(2,-2,1)

// ===================================================================
// Section 2: Kinematics — position equation
// ===================================================================
//
// x(t) = x0 + v0*t + 0.5*a*t^2
//
// Type derivation:
//   v0*t       = dim(1,-1,0) * dim(0,1,0) = dim(1,0,0)  [meter]
//   a*t*t      = dim(1,-2,0) * dim(0,1,0) * dim(0,1,0) = dim(1,0,0) [meter]
//   0.5*a*t*t  = dim(0,0,0) * dim(1,0,0) = dim(1,0,0)  [meter]
//   x0 + (...) + (...) = all dim(1,0,0)                  [meter OK]

static constexpr auto kin_env = TypeEnv<128>{}
                                    .bind("x0", t_meter)
                                    .bind("v0", t_mps)
                                    .bind("a", t_mps2)
                                    .bind("t", t_second);

static constexpr auto position =
    ann(E::var("x0") + E::var("v0") * E::var("t") +
            E::lit(0.5) * E::var("a") * E::var("t") * E::var("t"),
        t_meter);

// DFS variable discovery order: x0, v0, t, a
constexpr auto pos_fn = dim_typed_compile<position, kin_env>();
static_assert(pos_fn(10.0, 5.0, 2.0, 9.8) == 10.0 + 10.0 + 19.6);

// ===================================================================
// Section 3: Kinetic energy — E = 0.5 * m * v^2
// ===================================================================
//
// Type derivation:
//   m*v      = dim(0,0,1) * dim(1,-1,0) = dim(1,-1,1)
//   m*v*v    = dim(1,-1,1) * dim(1,-1,0) = dim(2,-2,1)  [joule]
//   0.5*...  = dim(0,0,0) * dim(2,-2,1) = dim(2,-2,1)  [joule]

static constexpr auto ke_env = TypeEnv<128>{}.bind("m", t_kg).bind("v", t_mps);

static constexpr auto kinetic_energy =
    ann(E::lit(0.5) * E::var("m") * E::var("v") * E::var("v"), t_joule);

constexpr auto ke_fn = dim_typed_compile<kinetic_energy, ke_env>();
static_assert(ke_fn(2.0, 3.0) == 0.5 * 2.0 * 9.0); // 9 joules

// ===================================================================
// Section 4: Newton's second law — F = m * a
// ===================================================================
//
// dim(0,0,1) * dim(1,-2,0) = dim(1,-2,1)  [newton]

static constexpr auto force_env =
    TypeEnv<128>{}.bind("m", t_kg).bind("a", t_mps2);

static constexpr auto force = ann(E::var("m") * E::var("a"), t_newton);

constexpr auto force_fn = dim_typed_compile<force, force_env>();
static_assert(force_fn(10.0, 9.8) == 98.0); // 98 newtons

// ===================================================================
// Section 5: Compile-time error detection
// ===================================================================
//
// Uncomment to see a compile-time error:
//   meter + second → dimension mismatch!
//
//   static constexpr auto bad =
//       ann(E::var("x0") + E::var("t"), t_meter);
//   constexpr auto bad_fn = dim_typed_compile<bad, kin_env>();

// ===================================================================
// Section 6: Derived unit computation
// ===================================================================
//
// Work = Force * Distance: newton * meter = joule
//   dim(1,-2,1) * dim(1,0,0) = dim(2,-2,1)

static constexpr auto work_env =
    TypeEnv<128>{}.bind("f", t_newton).bind("d", t_meter);

static constexpr auto work = ann(E::var("f") * E::var("d"), t_joule);

constexpr auto work_fn = dim_typed_compile<work, work_env>();
static_assert(work_fn(50.0, 3.0) == 150.0); // 150 joules

// ===================================================================
// Section 7: Velocity from division
// ===================================================================
//
// v = distance / time: dim(1,0,0) / dim(0,1,0) = dim(1,-1,0) [m/s]

static constexpr auto vel_env =
    TypeEnv<128>{}.bind("d", t_meter).bind("t", t_second);

static constexpr auto velocity = ann(E::var("d") / E::var("t"), t_mps);

constexpr auto vel_fn = dim_typed_compile<velocity, vel_env>();
static_assert(vel_fn(100.0, 10.0) == 10.0); // 10 m/s

int main() {
    std::printf("=== Compile-Time Dimensional Analysis ===\n\n");

    std::printf("Section 2: Kinematics\n");
    std::printf("  x(t) = x0 + v0*t + 0.5*a*t^2\n");
    std::printf("  x(10, 5, 2, 9.8) = %.1f m\n\n", pos_fn(10.0, 5.0, 2.0, 9.8));

    std::printf("Section 3: Kinetic Energy\n");
    std::printf("  E = 0.5 * m * v^2\n");
    std::printf("  E(2, 3) = %.1f J\n\n", ke_fn(2.0, 3.0));

    std::printf("Section 4: Newton's Second Law\n");
    std::printf("  F = m * a\n");
    std::printf("  F(10, 9.8) = %.1f N\n\n", force_fn(10.0, 9.8));

    std::printf("Section 6: Work = Force * Distance\n");
    std::printf("  W(50, 3) = %.1f J\n\n", work_fn(50.0, 3.0));

    std::printf("Section 7: Velocity = Distance / Time\n");
    std::printf("  v(100, 10) = %.1f m/s\n\n", vel_fn(100.0, 10.0));

    std::printf("All dimensional analysis examples passed!\n");
    return 0;
}
