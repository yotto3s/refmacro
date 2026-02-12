// Compile-fail test: unbound variable lookup must be rejected.
// This file should FAIL to compile — the type checker throws at compile time.
#include <reftype/check.hpp>
#include <reftype/types.hpp>

using E = refmacro::Expression<128>;

// Empty environment — "x" is not bound
constexpr auto result = reftype::type_check(E::var("x"));

int main() { (void)result; }
