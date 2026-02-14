// Compile-fail test: wider_base() must reject non-base types.
// This file should FAIL to compile — wider_base throws at compile time
// when called with a refined type (tref).
#include <reftype/subtype.hpp>
#include <reftype/types.hpp>

using E = refmacro::Expression<128>;

constexpr auto ref = reftype::tref(reftype::TInt, E::var("#v") > E::lit(0));
constexpr auto result = reftype::wider_base(ref, reftype::TInt);

int main() { (void)result; }
