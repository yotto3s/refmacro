// Compile-fail test: adding meter + second must produce a compile error.
// The dimensional type rule detects the mismatch and throws at consteval time.

#include <refmacro/math.hpp>
#include <reftype/dim.hpp>

using refmacro::Expression;
using E = Expression<128>;

static constexpr auto env = reftype::TypeEnv<128>{}
                                .bind("x", reftype::dim::t_meter)
                                .bind("t", reftype::dim::t_second);

// meter + second = dimension mismatch → consteval throw
static constexpr auto bad =
    reftype::ann(E::var("x") + E::var("t"), reftype::dim::t_meter);
constexpr auto fn = reftype::dim::dim_typed_compile<bad, env>();

int main() { return static_cast<int>(fn(1.0, 2.0)); }
