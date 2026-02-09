#ifndef REFMACRO_TRANSFORMS_HPP
#define REFMACRO_TRANSFORMS_HPP

#include <optional>
#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/node_view.hpp>

namespace refmacro {

// --- to_expr: extract a subtree into a standalone Expr ---

namespace detail {

template <std::size_t Cap>
consteval int copy_subtree(AST<Cap>& dst, const AST<Cap>& src, int src_id) {
    ASTNode n = src.nodes[src_id];
    int new_children[8]{-1, -1, -1, -1, -1, -1, -1, -1};
    for (int i = 0; i < n.child_count; ++i) {
        new_children[i] = copy_subtree(dst, src, n.children[i]);
    }
    for (int i = 0; i < n.child_count; ++i) {
        n.children[i] = new_children[i];
    }
    return dst.add_node(n);
}

template <std::size_t Cap, typename Rule>
consteval Expression<Cap> rebuild_bottom_up(const AST<Cap>& ast, int id,
                                            Rule rule) {
    ASTNode n = ast.nodes[id];

    if (n.child_count == 0) {
        NodeView<Cap> view{ast, id};
        auto replacement = rule(view);
        if (replacement)
            return *replacement;
        Expression<Cap> leaf;
        leaf.id = leaf.ast.add_node(n);
        return leaf;
    }

    // Rebuild children first (bottom-up)
    Expression<Cap> child_exprs[8];
    for (int i = 0; i < n.child_count; ++i) {
        child_exprs[i] = rebuild_bottom_up<Cap>(ast, n.children[i], rule);
    }

    // Merge rebuilt children into a new AST
    Expression<Cap> rebuilt;
    int new_child_ids[8]{-1, -1, -1, -1, -1, -1, -1, -1};

    // Start with the first child's AST
    rebuilt.ast = child_exprs[0].ast;
    new_child_ids[0] = child_exprs[0].id;

    // Merge remaining children
    for (int i = 1; i < n.child_count; ++i) {
        int offset = rebuilt.ast.merge(child_exprs[i].ast);
        new_child_ids[i] = child_exprs[i].id + offset;
    }

    // Build the parent node with updated child indices
    ASTNode rebuilt_node = n;
    for (int i = 0; i < n.child_count; ++i) {
        rebuilt_node.children[i] = new_child_ids[i];
    }
    rebuilt.id = rebuilt.ast.add_node(rebuilt_node);

    // Try rule on rebuilt node
    NodeView<Cap> view{rebuilt.ast, rebuilt.id};
    auto replacement = rule(view);
    if (replacement)
        return *replacement;

    return rebuilt;
}

} // namespace detail

template <std::size_t Cap>
consteval Expression<Cap> to_expr(NodeView<Cap> root, NodeView<Cap> subtree) {
    Expression<Cap> result;
    result.id = detail::copy_subtree(result.ast, root.ast, subtree.id);
    return result;
}

// --- Tree comparison for fixed-point detection ---

namespace detail {
template <std::size_t Cap>
consteval bool trees_equal(const AST<Cap>& a, int a_id, const AST<Cap>& b,
                           int b_id) {
    const auto& na = a.nodes[a_id];
    const auto& nb = b.nodes[b_id];
    if (!str_eq(na.tag, nb.tag))
        return false;
    if (na.payload != nb.payload)
        return false;
    if (!str_eq(na.name, nb.name))
        return false;
    if (na.child_count != nb.child_count)
        return false;
    for (int i = 0; i < na.child_count; ++i) {
        if (!trees_equal(a, na.children[i], b, nb.children[i]))
            return false;
    }
    return true;
}
} // namespace detail

// --- rewrite: bottom-up rule application until fixed point ---

template <std::size_t Cap = 64, typename Rule>
consteval Expression<Cap> rewrite(Expression<Cap> e, Rule rule,
                                  int max_iters = 100) {
    for (int iter = 0; iter < max_iters; ++iter) {
        auto result = detail::rebuild_bottom_up<Cap>(e.ast, e.id, rule);
        if (detail::trees_equal(e.ast, e.id, result.ast, result.id)) {
            return result;
        }
        e = result;
    }
    return e;
}

// --- fold: bottom-up accumulation over AST ---

template <typename T, int MaxChildren = 8> struct FoldChildren {
    static constexpr int capacity = MaxChildren;
    T values[MaxChildren]{};
    int count{0};

    consteval T operator[](int i) const {
        if (i < 0 || i >= count)
            throw "FoldChildren: index out of bounds";
        return values[i];
    }
};

template <std::size_t Cap = 64, typename Visitor>
consteval auto fold(Expression<Cap> e, Visitor visitor) {
    using R = decltype(visitor(NodeView<Cap>{e.ast, 0}, FoldChildren<int>{}));
    auto recurse = [&](auto self, int id) consteval -> R {
        NodeView<Cap> view{e.ast, id};
        FoldChildren<R> children;
        auto n = e.ast.nodes[id];
        for (int i = 0; i < n.child_count; ++i) {
            if (children.count >= children.capacity)
                throw "FoldChildren: capacity exceeded";
            children.values[children.count++] = self(self, n.children[i]);
        }
        return visitor(view, children);
    };
    return recurse(recurse, e.id);
}

// --- transform: structural recursion with user visitor ---

template <std::size_t Cap = 64, typename Visitor>
consteval Expression<Cap> transform(Expression<Cap> e, Visitor visitor) {
    auto recurse = [&](auto self, int id) consteval -> Expression<Cap> {
        NodeView<Cap> view{e.ast, id};
        auto rec = [&](NodeView<Cap> child) consteval -> Expression<Cap> {
            return self(self, child.id);
        };
        return visitor(view, rec);
    };
    return recurse(recurse, e.id);
}

} // namespace refmacro

#endif // REFMACRO_TRANSFORMS_HPP
