// 04_transforms.cpp — Rewriting, transforming, and folding AST nodes
//
// Shows: rewrite() with custom rules, transform() with visitor,
//        fold() for bottom-up accumulation, NodeView, to_expr(),
//        chaining transforms.

#include <iostream>
#include <optional>
#include <refmacro/refmacro.hpp>

using namespace refmacro;

int main() {
    constexpr auto x = Expr::var("x");
    constexpr auto y = Expr::var("y");

    // --- rewrite: custom constant folding rule ---
    // Eliminate additions of zero: (e + 0) → e
    constexpr auto remove_add_zero = [](Expr e) consteval {
        return rewrite(e, [](NodeView<64> n) consteval -> std::optional<Expr> {
            if (n.tag() == "add" && n.child_count() == 2) {
                if (n.child(1).tag() == "lit" && n.child(1).payload() == 0.0)
                    return to_expr(n, n.child(0));
                if (n.child(0).tag() == "lit" && n.child(0).payload() == 0.0)
                    return to_expr(n, n.child(1));
            }
            return std::nullopt;
        });
    };

    constexpr auto e1 = (x + 0.0) * (0.0 + y);
    constexpr auto e1_clean = e1 | remove_add_zero;

    std::cout << "Before: " << pretty_print(e1).data << "\n";
    std::cout << "After:  " << pretty_print(e1_clean).data << "\n";

    constexpr auto fn = math_compile<e1_clean>();
    static_assert(fn(3.0, 4.0) == 12.0); // x * y

    // --- transform: scale all literals by 2 ---
    constexpr auto double_lits = [](Expr e) consteval {
        return transform(e, [](NodeView<64> n, auto recurse) consteval -> Expr {
            if (n.tag() == "lit")
                return Expr::lit(n.payload() * 2.0);
            if (n.tag() == "var")
                return Expr::var(n.name().data());
            if (n.tag() == "add")
                return recurse(n.child(0)) + recurse(n.child(1));
            if (n.tag() == "mul")
                return recurse(n.child(0)) * recurse(n.child(1));
            return Expr::lit(0.0);
        });
    };

    // f(x) = x * 3 + 1  →  x * 6 + 2
    constexpr auto e2 = x * 3.0 + 1.0;
    constexpr auto e2_doubled = e2 | double_lits;

    std::cout << "\nBefore doubling: " << pretty_print(e2).data << "\n";
    std::cout << "After doubling:  " << pretty_print(e2_doubled).data << "\n";

    constexpr auto fn2 = math_compile<e2_doubled>();
    static_assert(fn2(1.0) == 8.0); // 1*6 + 2

    std::cout << "f(1) = " << fn2(1.0) << " (expected 8)\n";

    // --- fold: count nodes in an expression ---
    // fold() walks bottom-up, passing each node its children's results.
    constexpr auto e3 = (x + y) * x;
    constexpr auto node_count =
        fold(e3, [](NodeView<64>, auto children) consteval {
            int sum = 0;
            for (int i = 0; i < children.count; ++i)
                sum += children[i];
            return sum + 1;
        });
    static_assert(node_count == 5); // mul, add, x, y, x

    std::cout << "\n(x + y) * x has " << node_count << " nodes\n";

    // --- fold: collect variable names ---
    // Accumulate into VarMap — demonstrates folding into a structured type.
    constexpr auto vars = fold(e3, [](NodeView<64> n, auto children) consteval {
        VarMap<> vm{};
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

    std::cout << "(x + y) * x uses " << vars.count << " variables: ";
    for (std::size_t i = 0; i < vars.count; ++i) {
        if (i > 0)
            std::cout << ", ";
        std::cout << vars.names[i];
    }
    std::cout << "\n";
}
