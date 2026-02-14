#ifndef REFTYPE_TYPERULE_HPP
#define REFTYPE_TYPERULE_HPP

#include <refmacro/str_utils.hpp>

namespace reftype {

// TypeRule: a type-checking rule for an AST node tag.
// Parallel to refmacro::Macro<CompileFn>.
//
// - tag: the AST node tag this rule handles (e.g., "add", "cond")
// - fn: a stateless generic lambda with signature:
//     (const Expression<Cap>& expr, const TypeEnv<Cap>& env, auto synth_rec)
//         -> TypeResult<Cap>
//   where synth_rec(const Expression<Cap>&, const TypeEnv<Cap>&)
//         -> TypeResult<Cap>
//   performs recursive type synthesis through the full rule set.
template <typename SynthFn> struct TypeRule {
    char tag[16]{};
    SynthFn fn{};
};

template <typename SynthFn>
consteval auto def_typerule(const char* name, SynthFn) {
    TypeRule<SynthFn> r{};
    refmacro::copy_str(r.tag, name);
    return r;
}

} // namespace reftype

#endif // REFTYPE_TYPERULE_HPP
