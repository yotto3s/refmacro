#include <gtest/gtest.h>

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/str_utils.hpp>
#include <reftype/subtype.hpp>
#include <reftype/types.hpp>

using refmacro::ASTNode;
using refmacro::copy_str;
using refmacro::Expression;
using reftype::is_subtype;
using reftype::join;
using reftype::types_equal;

using E = Expression<128>;

// Minimal tdim constructor for testing (will move to dim.hpp later)
template <std::size_t Cap = 128>
consteval Expression<Cap> tdim(int l, int t, int m) {
    Expression<Cap> result;
    ASTNode ln{};
    copy_str(ln.tag, "lit");
    ln.payload = l;
    ASTNode tn{};
    copy_str(tn.tag, "lit");
    tn.payload = t;
    ASTNode mn{};
    copy_str(mn.tag, "lit");
    mn.payload = m;
    int li = result.ast.add_node(ln);
    int ti = result.ast.add_node(tn);
    int mi = result.ast.add_node(mn);
    result.id = result.ast.add_tagged_node("tdim", {li, ti, mi});
    return result;
}

// --- Subtype: tdim ---

TEST(DimSubtype, SameDimIsSubtype) {
    static constexpr auto meter = tdim(1, 0, 0);
    static constexpr auto meter2 = tdim(1, 0, 0);
    static_assert(is_subtype(meter, meter2));
}

TEST(DimSubtype, DifferentDimNotSubtype) {
    static constexpr auto meter = tdim(1, 0, 0);
    static constexpr auto second = tdim(0, 1, 0);
    static_assert(!is_subtype(meter, second));
}

TEST(DimSubtype, DimNotSubtypeOfBase) {
    static constexpr auto meter = tdim(1, 0, 0);
    static_assert(!is_subtype(meter, reftype::TInt));
    static_assert(!is_subtype(meter, reftype::TReal));
}

// --- Join: tdim ---

TEST(DimJoin, SameDimJoinsToSelf) {
    static constexpr auto meter = tdim(1, 0, 0);
    static constexpr auto result = join(meter, meter);
    static_assert(types_equal(result, meter));
}
