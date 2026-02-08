#ifndef REFMACRO_COMPILE_HPP
#define REFMACRO_COMPILE_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <tuple>
#include <utility>

namespace refmacro {

// --- VarMap: extract ordered unique variable names from AST ---

template <std::size_t MaxVars = 8>
struct VarMap {
    char names[MaxVars][16]{};
    std::size_t count{0};

    consteval bool contains(const char* name) const {
        for (std::size_t i = 0; i < count; ++i)
            if (str_eq(names[i], name)) return true;
        return false;
    }

    consteval void add(const char* name) {
        if (!contains(name)) {
            copy_str(names[count], name);
            ++count;
        }
    }

    consteval int index_of(const char* name) const {
        for (std::size_t i = 0; i < count; ++i)
            if (str_eq(names[i], name)) return static_cast<int>(i);
        return -1;
    }
};

template <std::size_t Cap>
consteval auto extract_var_map(const AST<Cap>& ast) {
    VarMap<> vm{};
    for (std::size_t i = 0; i < ast.count; ++i) {
        if (str_eq(ast.nodes[i].tag, "var"))
            vm.add(ast.nodes[i].name);
    }
    return vm;
}

// --- FixedStr: structural wrapper for char arrays (NTTP-compatible) ---

struct FixedStr {
    char data[16]{};

    consteval FixedStr() = default;
    consteval FixedStr(const char (&src)[16]) {
        for (int i = 0; i < 16; ++i)
            data[i] = src[i];
    }
};

// --- Macro dispatch: find macro by tag and apply ---

namespace detail {

// Apply the first macro whose tag matches. If none match, compile error.
template <FixedStr Tag, auto First, auto... Rest>
consteval auto apply_macro(auto children_tuple) {
    if constexpr (str_eq(Tag.data, First.tag)) {
        return std::apply(First.fn, children_tuple);
    } else {
        static_assert(sizeof...(Rest) > 0,
            "no macro defined for AST tag");
        return apply_macro<Tag, Rest...>(children_tuple);
    }
}

// --- Recursive compile ---

template <auto ast, int id, auto var_map, auto... Macros>
consteval auto compile_node() {
    constexpr auto n = ast.nodes[id];

    // Built-in: variable -> argument accessor
    if constexpr (str_eq(n.tag, "var")) {
        constexpr int idx = var_map.index_of(n.name);
        static_assert(idx >= 0, "unbound variable in AST");
        return [](auto... args) constexpr {
            return std::get<idx>(std::tuple{args...});
        };
    }
    // Built-in: literal -> constant
    else if constexpr (str_eq(n.tag, "lit")) {
        return [](auto...) constexpr { return n.payload; };
    }
    // Everything else: dispatch to macros
    else {
        // Compile children bottom-up, then dispatch to matching macro
        auto children = [&]<int... Cs>(std::integer_sequence<int, Cs...>) {
            return std::tuple{
                compile_node<ast, n.children[Cs], var_map, Macros...>()...
            };
        }(std::make_integer_sequence<int, n.child_count>{});

        return apply_macro<FixedStr{n.tag}, Macros...>(children);
    }
}

} // namespace detail

// --- Public API ---

template <auto e, auto... Macros>
consteval auto compile() {
    constexpr auto vm = extract_var_map(e.ast);
    return detail::compile_node<e.ast, e.id, vm, Macros...>();
}

} // namespace refmacro

#endif // REFMACRO_COMPILE_HPP
