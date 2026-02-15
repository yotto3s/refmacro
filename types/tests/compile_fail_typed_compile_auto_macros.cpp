// Regression test: typed_compile preserves auto-tracked macros.
//
// Previously (F1 bug), typed_compile<expr>() failed when expr had
// auto-tracked macros (embedded in its type) but no explicit Macros
// were passed, because strip_types lost the embedded macros.
// Now typed_compile extracts embedded macros from expr's type and
// forwards them to compile.

#include <refmacro/math.hpp>
#include <reftype/strip.hpp>
#include <reftype/types.hpp>

using refmacro::Expression;
using E = Expression<128>;

// Expression with auto-tracked MAdd (embedded in type)
constexpr auto e = E::lit(2) + E::lit(3);

// This now works: typed_compile extracts MAdd from expr's type
constexpr auto f = reftype::typed_compile<e>();

static_assert(f() == 5);

int main() { return f() == 5 ? 0 : 1; }
