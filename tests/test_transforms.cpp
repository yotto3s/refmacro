#include <gtest/gtest.h>
#include <refmacro/math.hpp>
#include <refmacro/transforms.hpp>

using namespace refmacro;

consteval auto root_tag(const Expr<>& e) -> std::string_view {
    return e.ast.nodes[e.id].tag;
}
consteval double root_payload(const Expr<>& e) {
    return e.ast.nodes[e.id].payload;
}

// --- rewrite tests ---

TEST(Rewrite, Identity) {
    constexpr auto e = Expr<>::var("x") + Expr<>::lit(1.0);
    constexpr auto result =
        rewrite(e, [](NodeView<64>) consteval -> std::optional<Expr<>> {
            return std::nullopt;
        });
    static_assert(root_tag(result) == "add");
}

TEST(Rewrite, AddZero) {
    constexpr auto e = Expr<>::var("x") + Expr<>::lit(0.0);
    constexpr auto result =
        rewrite(e, [](NodeView<64> n) consteval -> std::optional<Expr<>> {
            if (n.tag() == "add" && n.child(1).tag() == "lit" &&
                n.child(1).payload() == 0.0)
                return to_expr(n, n.child(0));
            return std::nullopt;
        });
    static_assert(root_tag(result) == "var");
}

TEST(Rewrite, ConstantFold) {
    constexpr auto e = Expr<>::lit(3.0) + Expr<>::lit(4.0);
    constexpr auto result =
        rewrite(e, [](NodeView<64> n) consteval -> std::optional<Expr<>> {
            if (n.tag() == "add" && n.child(0).tag() == "lit" &&
                n.child(1).tag() == "lit")
                return Expr<>::lit(n.child(0).payload() + n.child(1).payload());
            return std::nullopt;
        });
    static_assert(root_payload(result) == 7.0);
}

// --- transform tests ---

TEST(Transform, DoubleLiterals) {
    constexpr auto e = Expr<>::lit(3.0) + Expr<>::lit(4.0);
    constexpr auto result =
        transform(e, [](NodeView<64> n, auto recurse) consteval -> Expr<> {
            if (n.tag() == "lit")
                return Expr<>::lit(n.payload() * 2.0);
            if (n.tag() == "add")
                return recurse(n.child(0)) + recurse(n.child(1));
            return Expr<>::lit(0.0);
        });
    static_assert(root_tag(result) == "add");
    constexpr auto lhs =
        result.ast.nodes[result.ast.nodes[result.id].children[0]];
    constexpr auto rhs =
        result.ast.nodes[result.ast.nodes[result.id].children[1]];
    static_assert(lhs.payload == 6.0);
    static_assert(rhs.payload == 8.0);
}

TEST(Transform, IdentityPreservesStructure) {
    constexpr auto e = Expr<>::var("x") * Expr<>::var("y");
    constexpr auto result =
        transform(e, [](NodeView<64> n, auto recurse) consteval -> Expr<> {
            if (n.tag() == "var")
                return Expr<>::var(n.name().data());
            if (n.tag() == "mul")
                return recurse(n.child(0)) * recurse(n.child(1));
            return Expr<>::lit(0.0);
        });
    static_assert(root_tag(result) == "mul");
}
