#include <gtest/gtest.h>
#include <refmacro/control.hpp>
#include <refmacro/math.hpp>
#include <refmacro/transforms.hpp>

using namespace refmacro;

consteval auto root_tag(const Expr& e) -> std::string_view {
    return e.ast.nodes[e.id].tag;
}
consteval double root_payload(const Expr& e) {
    return e.ast.nodes[e.id].payload;
}

// --- rewrite tests ---

TEST(Rewrite, Identity) {
    constexpr auto e = Expr::var("x") + Expr::lit(1.0);
    constexpr auto result =
        rewrite(e, [](NodeView<64>) consteval -> std::optional<Expr> {
            return std::nullopt;
        });
    static_assert(root_tag(result) == "add");
}

TEST(Rewrite, AddZero) {
    constexpr auto e = Expr::var("x") + Expr::lit(0.0);
    constexpr auto result =
        rewrite(e, [](NodeView<64> n) consteval -> std::optional<Expr> {
            if (n.tag() == "add" && n.child(1).tag() == "lit" &&
                n.child(1).payload() == 0.0)
                return to_expr(n, n.child(0));
            return std::nullopt;
        });
    static_assert(root_tag(result) == "var");
}

TEST(Rewrite, ConstantFold) {
    constexpr auto e = Expr::lit(3.0) + Expr::lit(4.0);
    constexpr auto result =
        rewrite(e, [](NodeView<64> n) consteval -> std::optional<Expr> {
            if (n.tag() == "add" && n.child(0).tag() == "lit" &&
                n.child(1).tag() == "lit")
                return Expr::lit(n.child(0).payload() + n.child(1).payload());
            return std::nullopt;
        });
    static_assert(root_payload(result) == 7.0);
}

// --- transform tests ---

TEST(Transform, DoubleLiterals) {
    constexpr auto e = Expr::lit(3.0) + Expr::lit(4.0);
    constexpr auto result =
        transform(e, [](NodeView<64> n, auto recurse) consteval -> Expr {
            if (n.tag() == "lit")
                return Expr::lit(n.payload() * 2.0);
            if (n.tag() == "add")
                return recurse(n.child(0)) + recurse(n.child(1));
            return Expr::lit(0.0);
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
    constexpr auto e = Expr::var("x") * Expr::var("y");
    constexpr auto result =
        transform(e, [](NodeView<64> n, auto recurse) consteval -> Expr {
            if (n.tag() == "var")
                return Expr::var(n.name().data());
            if (n.tag() == "mul")
                return recurse(n.child(0)) * recurse(n.child(1));
            return Expr::lit(0.0);
        });
    static_assert(root_tag(result) == "mul");
}

// --- fold tests ---

TEST(Fold, CountNodes) {
    // lit(3) + lit(4) => 3 nodes: add, lit, lit
    constexpr auto e = Expr::lit(3.0) + Expr::lit(4.0);
    constexpr auto count = fold(e, [](NodeView<64>, auto children) consteval {
        int sum = 0;
        for (int i = 0; i < children.count; ++i)
            sum += children[i];
        return sum + 1; // this node
    });
    static_assert(count == 3);
}

TEST(Fold, SumLiterals) {
    constexpr auto e = Expr::lit(3.0) + Expr::lit(4.0);
    constexpr auto total = fold(e, [](NodeView<64> n, auto children) consteval {
        double sum = 0.0;
        for (int i = 0; i < children.count; ++i)
            sum += children[i];
        if (n.tag() == "lit")
            sum += n.payload();
        return sum;
    });
    static_assert(total == 7.0);
}

TEST(Fold, CollectVarNames) {
    constexpr auto e = (Expr::var("x") + Expr::var("y")) * Expr::var("x");
    constexpr auto vars = fold(e, [](NodeView<64> n, auto children) consteval {
        VarMap<> vm;
        for (int i = 0; i < children.count; ++i) {
            if constexpr (requires { children[i].count; }) {
                auto child_vm = children[i];
                for (std::size_t j = 0; j < child_vm.count; ++j)
                    vm.add(child_vm.names[j]);
            }
        }
        if (n.tag() == "var")
            vm.add(n.name().data());
        return vm;
    });
    static_assert(vars.count == 2);
    static_assert(vars.contains("x"));
    static_assert(vars.contains("y"));
}

TEST(Fold, TreeDepth) {
    constexpr auto e = (Expr::var("x") + Expr::var("y")) * Expr::var("z");
    constexpr auto depth = fold(e, [](NodeView<64>, auto children) consteval {
        int max_child = 0;
        for (int i = 0; i < children.count; ++i) {
            if (children[i] > max_child)
                max_child = children[i];
        }
        return max_child + 1;
    });
    static_assert(depth == 3);
}

TEST(Fold, PipeOperator) {
    constexpr auto count_nodes = [](auto e) consteval {
        return fold(e, [](NodeView<64>, auto children) consteval {
            int sum = 0;
            for (int i = 0; i < children.count; ++i)
                sum += children[i];
            return sum + 1;
        });
    };
    constexpr auto e = Expr::lit(1.0) + Expr::lit(2.0);
    constexpr auto count = e | count_nodes;
    static_assert(count == 3);
}

TEST(Fold, CountNodesWithLet) {
    // let x = 5 in (x + 1) => apply(lambda(x, add(x, 1)), 5)
    // nodes: apply, lambda, var(x-param), add, var(x), lit(1), lit(5) = 7
    constexpr auto e =
        let_("x", Expr::lit(5.0), Expr::var("x") + Expr::lit(1.0));
    constexpr auto count = fold(e, [](NodeView<64>, auto children) consteval {
        int sum = 0;
        for (int i = 0; i < children.count; ++i)
            sum += children[i];
        return sum + 1;
    });
    static_assert(count == 7);
}
