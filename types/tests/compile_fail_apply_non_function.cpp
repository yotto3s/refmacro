// Compile-fail test: applying a non-function must be rejected.
// This file should FAIL to compile — the type checker throws at compile time.
#include <refmacro/control.hpp>
#include <reftype/check.hpp>
#include <reftype/types.hpp>

using E = refmacro::Expression<128>;

constexpr auto result =
    reftype::type_check(refmacro::apply<128>(E::var("x"), E::lit(5)),
                        reftype::TypeEnv<128>{}.bind("x", reftype::TInt));

int main() { (void)result; }
