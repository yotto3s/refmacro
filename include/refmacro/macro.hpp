#ifndef REFMACRO_MACRO_HPP
#define REFMACRO_MACRO_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>

namespace refmacro {

// Macro: an AST node type with deferred lowering.
//
// - Calling a Macro builds an AST node (no lowering happens).
// - The lowering function `fn` is applied later by compile().
// - `fn` receives compiled children (lambdas) and returns a lambda.
// - CompileFn must be a stateless lambda (structural -> NTTP-compatible).
template <typename CompileFn> struct Macro {
    char tag[16]{};
    CompileFn fn{};

    // Build AST nodes -- lowering is deferred
    consteval Expr operator()(std::same_as<Expr> auto... children) const
        requires(sizeof...(children) <= 8)
    {
        return make_node(tag, children...);
    }
};

template <typename CompileFn>
consteval auto defmacro(const char* name, CompileFn) {
    Macro<CompileFn> m{};
    copy_str(m.tag, name);
    return m;
}

} // namespace refmacro

#endif // REFMACRO_MACRO_HPP
