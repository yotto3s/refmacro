#ifndef REFMACRO_COMPILE_HPP
#define REFMACRO_COMPILE_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <tuple>
#include <utility>

namespace refmacro {

// --- VarMap: extract ordered unique variable names from AST ---

template <std::size_t MaxVars = 8> struct VarMap {
    char names[MaxVars][16]{};
    std::size_t count{0};

    consteval bool contains(const char* name) const {
        for (std::size_t i = 0; i < count; ++i)
            if (str_eq(names[i], name))
                return true;
        return false;
    }

    consteval void add(const char* name) {
        if (!contains(name)) {
            if (count >= MaxVars)
                throw "VarMap capacity exceeded";
            copy_str(names[count], name);
            ++count;
        }
    }

    consteval int index_of(const char* name) const {
        for (std::size_t i = 0; i < count; ++i)
            if (str_eq(names[i], name))
                return static_cast<int>(i);
        return -1;
    }
};

namespace detail {
template <std::size_t Cap>
consteval void collect_vars_dfs(const AST<Cap>& ast, int id, VarMap<>& vm,
                                VarMap<> bound = {}) {
    const auto& n = ast.nodes[id];
    if (str_eq(n.tag, "var")) {
        if (!bound.contains(n.name))
            vm.add(n.name);
        return;
    }
    // apply(lambda(param, body), val): param is bound inside body
    if (str_eq(n.tag, "apply") && n.child_count == 2) {
        const auto& fn_node = ast.nodes[n.children[0]];
        if (str_eq(fn_node.tag, "lambda") && fn_node.child_count == 2) {
            // Collect vars from value (param not yet bound)
            collect_vars_dfs(ast, n.children[1], vm, bound);
            // Collect vars from lambda body with param bound
            const auto& param_node = ast.nodes[fn_node.children[0]];
            VarMap<> inner_bound = bound;
            inner_bound.add(param_node.name);
            collect_vars_dfs(ast, fn_node.children[1], vm, inner_bound);
            return;
        }
    }
    for (int i = 0; i < n.child_count; ++i) {
        if (n.children[i] >= 0)
            collect_vars_dfs(ast, n.children[i], vm, bound);
    }
}
} // namespace detail

template <std::size_t Cap>
consteval auto extract_var_map(const AST<Cap>& ast, int root_id) {
    VarMap<> vm{};
    detail::collect_vars_dfs(ast, root_id, vm);
    return vm;
}

// --- TagStr: structural wrapper for char arrays (NTTP-compatible) ---

struct TagStr {
    char data[16]{};

    consteval TagStr() = default;
    consteval TagStr(const char (&src)[16]) {
        for (int i = 0; i < 16; ++i)
            data[i] = src[i];
    }
};

// --- Scope: compile-time tracking of locally-bound variables ---

struct Scope {
    static constexpr std::size_t MaxLocals = 8;
    TagStr names[MaxLocals]{};
    std::size_t count{0};

    consteval Scope push(TagStr name) const {
        if (count >= MaxLocals)
            throw "Scope capacity exceeded";
        Scope s = *this;
        s.names[s.count] = name;
        ++s.count;
        return s;
    }

    consteval int find(const char* name) const {
        // Search most-recent-first for correct shadowing
        for (int i = static_cast<int>(count) - 1; i >= 0; --i)
            if (str_eq(names[i].data, name))
                return i;
        return -1;
    }
};

// --- Macro dispatch: find macro by tag and apply ---

namespace detail {

// Apply the first macro whose tag matches. If none match, compile error.
template <TagStr Tag, auto First, auto... Rest>
consteval auto apply_macro(auto children_tuple) {
    if constexpr (str_eq(Tag.data, First.tag)) {
        return std::apply(First.fn, children_tuple);
    } else {
        static_assert(sizeof...(Rest) > 0, "no macro defined for AST tag");
        return apply_macro<Tag, Rest...>(children_tuple);
    }
}

// --- Recursive compile ---

template <auto ast, int id, auto var_map, auto scope, auto... Macros>
consteval auto compile_node(auto locals) {
    constexpr auto n = ast.nodes[id];

    // Built-in: variable -> local binding or argument accessor
    if constexpr (str_eq(n.tag, "var")) {
        constexpr int local_idx = scope.find(n.name);
        if constexpr (local_idx >= 0) {
            return std::get<local_idx>(locals);
        } else {
            constexpr int idx = var_map.index_of(n.name);
            static_assert(idx >= 0, "unbound variable in AST");
            return [](auto... args) constexpr {
                return std::get<idx>(std::tuple{args...});
            };
        }
    }
    // Built-in: literal -> constant
    else if constexpr (str_eq(n.tag, "lit")) {
        return [](auto...) constexpr { return n.payload; };
    }
    // Built-in: apply(lambda(param, body), val) -> let binding
    else if constexpr (str_eq(n.tag, "apply") && n.child_count == 2 &&
                       str_eq(ast.nodes[n.children[0]].tag, "lambda")) {
        constexpr auto lambda_node = ast.nodes[n.children[0]];
        static_assert(lambda_node.child_count == 2,
                      "malformed AST: lambda node must have 2 children "
                      "(param, body)");
        constexpr auto param_node = ast.nodes[lambda_node.children[0]];
        // Compile value with current scope
        auto val_fn =
            compile_node<ast, n.children[1], var_map, scope, Macros...>(locals);
        // Extend scope and locals
        constexpr auto new_scope = scope.push(TagStr{param_node.name});
        auto new_locals = std::tuple_cat(locals, std::tuple{val_fn});
        // Compile lambda body with extended scope
        return compile_node<ast, lambda_node.children[1], var_map, new_scope,
                            Macros...>(new_locals);
    }
    // Everything else: dispatch to macros
    else {
        // Compile children bottom-up, then dispatch to matching macro
        auto children = [&]<int... Cs>(std::integer_sequence<int, Cs...>) {
            return std::tuple{
                compile_node<ast, n.children[Cs], var_map, scope, Macros...>(
                    locals)...};
        }(std::make_integer_sequence<int, n.child_count>{});

        return apply_macro<TagStr{n.tag}, Macros...>(children);
    }
}

} // namespace detail

// --- Public API ---

template <auto e, auto... Macros> consteval auto compile() {
    constexpr auto vm = extract_var_map(e.ast, e.id);
    return detail::compile_node<e.ast, e.id, vm, Scope{}, Macros...>(
        std::tuple{});
}

} // namespace refmacro

#endif // REFMACRO_COMPILE_HPP
