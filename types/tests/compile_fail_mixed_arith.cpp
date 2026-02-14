// Compile-fail test: mixed-type arithmetic (Int + Real) must be rejected.
// This file should FAIL to compile — the type checker throws at compile time.
#include <reftype/check.hpp>
#include <reftype/types.hpp>

using E = refmacro::Expression<128>;

constexpr auto result = reftype::type_check(
    E::var("x") + E::var("y"),
    reftype::TypeEnv<128>{}.bind("x", reftype::TInt).bind("y", reftype::TReal));

int main() { (void)result; }
