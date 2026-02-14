// Compile-fail test: standalone lambda without annotation must be rejected.
// Lambda type cannot be inferred without an annotation.
// This file should FAIL to compile — the type checker throws at compile time.
#include <refmacro/control.hpp>
#include <reftype/check.hpp>
#include <reftype/types.hpp>

using E = refmacro::Expression<128>;

constexpr auto result =
    reftype::type_check(refmacro::lambda<128>("x", E::var("x") + E::lit(1)));

int main() { (void)result; }
