// Compile-fail test: typed_full_compile should fail at type check time
// when an invalid type annotation is used.
//
// ann(lit(0), {#v : Int | #v > 0}) — 0 is not > 0, so this is invalid.
// typed_full_compile should static_assert with "type check failed".

#include <reftype/strip.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using E = Expression<128>;

// lit(0) annotated as positive int — invalid (0 is not > 0)
static constexpr auto e =
    reftype::ann(E::lit(0), reftype::pos_int());
constexpr auto f = reftype::typed_full_compile<e>();

int main() { return static_cast<int>(f()); }
