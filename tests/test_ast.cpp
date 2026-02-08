#include <refmacro/ast.hpp>
#include <gtest/gtest.h>

using namespace refmacro;

TEST(StringUtils, CopyAndCompare) {
    constexpr auto result = [] consteval {
        char buf[16]{};
        copy_str(buf, "hello");
        return str_eq(buf, "hello");
    }();
    static_assert(result);
}

TEST(StringUtils, Inequality) {
    static_assert(!str_eq("abc", "xyz"));
    static_assert(str_eq("abc", "abc"));
}

TEST(ASTNode, DefaultConstructed) {
    constexpr ASTNode n{};
    static_assert(n.payload == 0.0);
    static_assert(n.child_count == 0);
    static_assert(n.scope == 0);
}

TEST(ASTNode, LitNode) {
    constexpr auto n = [] consteval {
        ASTNode node{};
        copy_str(node.tag, "lit");
        node.payload = 42.0;
        return node;
    }();
    static_assert(str_eq(n.tag, "lit"));
    static_assert(n.payload == 42.0);
}

TEST(ASTNode, VarNode) {
    constexpr auto n = [] consteval {
        ASTNode node{};
        copy_str(node.tag, "var");
        copy_str(node.name, "x");
        return node;
    }();
    static_assert(str_eq(n.tag, "var"));
    static_assert(str_eq(n.name, "x"));
}

// Verify ASTNode works as NTTP (critical for Backend dispatch)
template <ASTNode N>
consteval double nttp_payload() { return N.payload; }

TEST(ASTNode, WorksAsNTTP) {
    constexpr auto n = [] consteval {
        ASTNode node{};
        copy_str(node.tag, "lit");
        node.payload = 99.0;
        return node;
    }();
    static_assert(nttp_payload<n>() == 99.0);
}

TEST(AST, AddNode) {
    constexpr auto ast = [] consteval {
        AST<16> a{};
        ASTNode n{};
        copy_str(n.tag, "lit");
        n.payload = 7.0;
        a.add_node(n);
        return a;
    }();
    static_assert(ast.count == 1);
    static_assert(ast.root == 0);
    static_assert(ast.nodes[0].payload == 7.0);
}

TEST(AST, AddTaggedNode) {
    constexpr auto ast = [] consteval {
        AST<16> a{};

        ASTNode lit1{};
        copy_str(lit1.tag, "lit");
        lit1.payload = 1.0;
        int id1 = a.add_node(lit1);

        ASTNode lit2{};
        copy_str(lit2.tag, "lit");
        lit2.payload = 2.0;
        int id2 = a.add_node(lit2);

        a.add_tagged_node("add", {id1, id2});
        return a;
    }();
    static_assert(ast.count == 3);
    static_assert(str_eq(ast.nodes[2].tag, "add"));
    static_assert(ast.nodes[2].child_count == 2);
    static_assert(ast.nodes[2].children[0] == 0);
    static_assert(ast.nodes[2].children[1] == 1);
}

TEST(AST, Merge) {
    constexpr auto result = [] consteval {
        AST<16> a{};
        ASTNode n1{};
        copy_str(n1.tag, "lit");
        n1.payload = 1.0;
        a.add_node(n1);

        AST<16> b{};
        ASTNode n2{};
        copy_str(n2.tag, "lit");
        n2.payload = 2.0;
        b.add_node(n2);

        int offset = a.merge(b);
        return std::pair{a, offset};
    }();
    static_assert(result.first.count == 2);
    static_assert(result.second == 1);
    static_assert(result.first.nodes[1].payload == 2.0);
}
