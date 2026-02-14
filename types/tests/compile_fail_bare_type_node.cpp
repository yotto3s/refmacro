// Compile-fail test: bare type nodes outside annotations should cause
// a consteval throw (compile error) when strip_types is called.

#include <reftype/strip.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using E = Expression<128>;

// TInt is a bare type node — strip_types should reject it
constexpr auto bad = reftype::strip_types(reftype::TInt);

int main() { return static_cast<int>(bad.id); }
