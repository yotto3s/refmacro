// 04_transforms.cpp — Rewriting and transforming AST nodes
//
// Shows: rewrite() with custom rules, transform() with visitor,
//        NodeView, to_expr(), chaining transforms.

#include <refmacro/refmacro.hpp>
#include <iostream>
#include <optional>

using namespace refmacro;

int main() {
    constexpr auto x = Expr<>::var("x");
    constexpr auto y = Expr<>::var("y");

    // --- rewrite: custom constant folding rule ---
    // Eliminate additions of zero: (e + 0) → e
    constexpr auto remove_add_zero = [](Expr<> e) consteval {
        return rewrite(e, [](NodeView<64> n) consteval -> std::optional<Expr<>> {
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
    static_assert(fn(3.0, 4.0) == 12.0);  // x * y

    // --- transform: scale all literals by 2 ---
    constexpr auto double_lits = [](Expr<> e) consteval {
        return transform(e, [](NodeView<64> n, auto recurse) consteval -> Expr<> {
            if (n.tag() == "lit")
                return Expr<>::lit(n.payload() * 2.0);
            if (n.tag() == "var")
                return Expr<>::var(n.name().data());
            if (n.tag() == "add")
                return recurse(n.child(0)) + recurse(n.child(1));
            if (n.tag() == "mul")
                return recurse(n.child(0)) * recurse(n.child(1));
            return Expr<>::lit(0.0);
        });
    };

    // f(x) = x * 3 + 1  →  x * 6 + 2
    constexpr auto e2 = x * 3.0 + 1.0;
    constexpr auto e2_doubled = e2 | double_lits;

    std::cout << "\nBefore doubling: " << pretty_print(e2).data << "\n";
    std::cout << "After doubling:  " << pretty_print(e2_doubled).data << "\n";

    constexpr auto fn2 = math_compile<e2_doubled>();
    static_assert(fn2(1.0) == 8.0);  // 1*6 + 2

    std::cout << "f(1) = " << fn2(1.0) << " (expected 8)\n";
}
