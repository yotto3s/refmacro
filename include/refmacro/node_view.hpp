#ifndef REFMACRO_NODE_VIEW_HPP
#define REFMACRO_NODE_VIEW_HPP

#include <refmacro/ast.hpp>
#include <string_view>

namespace refmacro {

template <std::size_t Cap>
struct NodeView {
    const AST<Cap>& ast;
    int id;

    consteval auto tag() const -> std::string_view { return ast.nodes[id].tag; }
    consteval auto name() const -> std::string_view { return ast.nodes[id].name; }
    consteval double payload() const { return ast.nodes[id].payload; }
    consteval int scope() const { return ast.nodes[id].scope; }
    consteval int child_count() const { return ast.nodes[id].child_count; }
    consteval NodeView child(int i) const {
        return NodeView{ast, ast.nodes[id].children[i]};
    }
};

template <std::size_t Cap>
NodeView(const AST<Cap>&, int) -> NodeView<Cap>;

} // namespace refmacro

#endif // REFMACRO_NODE_VIEW_HPP
