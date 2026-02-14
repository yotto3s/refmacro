#ifndef REFMACRO_AST_HPP
#define REFMACRO_AST_HPP

#include <cstddef>
#include <initializer_list>
#include <utility>

#include <refmacro/str_utils.hpp>

namespace refmacro {

// --- AST Node (structural type — works as NTTP) ---

struct ASTNode {
    char tag[16]{};
    double payload{};
    char name[16]{};
    int children[8]{-1, -1, -1, -1, -1, -1, -1, -1};
    int child_count{0};
};

// --- Flat AST Storage (structural type — works as NTTP) ---

template <std::size_t Cap = 64> struct AST {
    ASTNode nodes[Cap]{};
    std::size_t count{0};

    consteval int add_node(ASTNode n) {
        if (count >= Cap)
            throw "AST capacity exceeded";
        int idx = static_cast<int>(count);
        nodes[count++] = n;
        return idx;
    }

    consteval int add_tagged_node(const char* tag_name,
                                  std::initializer_list<int> children) {
        return add_tagged_node(tag_name, children.begin(), children.size());
    }

    consteval int add_tagged_node(const char* tag_name,
                                  const int* children, std::size_t n_children) {
        if (n_children > 8)
            throw "ASTNode supports at most 8 children";
        ASTNode n{};
        copy_str(n.tag, tag_name);
        for (std::size_t i = 0; i < n_children; ++i)
            n.children[i] = children[i];
        n.child_count = static_cast<int>(n_children);
        return add_node(n);
    }

    consteval int merge(const AST& other) {
        if (count + other.count > Cap)
            throw "AST capacity exceeded in merge";
        int offset = static_cast<int>(count);
        for (std::size_t i = 0; i < other.count; ++i) {
            ASTNode n = other.nodes[i];
            for (int c = 0; c < n.child_count; ++c) {
                if (n.children[c] >= 0)
                    n.children[c] += offset;
            }
            nodes[count++] = n;
        }
        return offset;
    }
};

} // namespace refmacro

#endif // REFMACRO_AST_HPP
