// Compile-fail test: Bool operands in arithmetic must be rejected.
// Bool is excluded from numeric ops despite Bool <: Int in the subtype lattice.
// This file should FAIL to compile — the type checker throws at compile time.
#include <reftype/check.hpp>
#include <reftype/types.hpp>

using E = refmacro::Expression<128>;

constexpr auto result = reftype::type_check(E::var("x") + E::var("y"),
                                            reftype::TypeEnv<128>{}
                                                .bind("x", reftype::TBool)
                                                .bind("y", reftype::TBool));

int main() { (void)result; }
