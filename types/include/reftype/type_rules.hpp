#ifndef REFTYPE_TYPE_RULES_HPP
#define REFTYPE_TYPE_RULES_HPP

#include <refmacro/str_utils.hpp>

namespace reftype {

// A type rule: associates an AST tag with a typing function.
// Parallel to defmacro in the core AST framework.
//
// fn signature: consteval TypeResult<Cap> fn(Expression<Cap> expr, TypeEnv<Cap> env)
template <typename RuleFn>
struct TypeRule {
    char tag[16]{};
    RuleFn fn;
};

// Define a type rule for a given AST tag.
template <typename F>
consteval auto def_typerule(const char* name, F fn) {
    TypeRule<F> rule{};
    refmacro::copy_str(rule.tag, name, sizeof(rule.tag));
    rule.fn = fn;
    return rule;
}

} // namespace reftype

#endif // REFTYPE_TYPE_RULES_HPP
