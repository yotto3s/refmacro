#ifndef REFTYPE_TYPE_ENV_HPP
#define REFTYPE_TYPE_ENV_HPP

#include <refmacro/expr.hpp>
#include <refmacro/str_utils.hpp>

namespace reftype {

using refmacro::copy_str;
using refmacro::Expression;
using refmacro::str_eq;
using refmacro::str_len;

// Compile-time map from variable names to their types (as Expression ASTs).
// Immutable: bind() returns a new TypeEnv. Shadowing via reverse-order lookup.
template <std::size_t Cap = 128, int MaxBindings = 16> struct TypeEnv {
    char names[MaxBindings][16]{};
    Expression<Cap> types[MaxBindings]{};
    int count{0};

    // Append binding (supports shadowing — later bindings win)
    consteval TypeEnv bind(const char* name, Expression<Cap> type) const {
        if (count >= MaxBindings)
            throw "TypeEnv capacity exceeded";
        if (str_len(name) >= sizeof(names[0]))
            throw "TypeEnv name too long";
        TypeEnv result = *this;
        copy_str(result.names[result.count], name, sizeof(result.names[0]));
        result.types[result.count] = type;
        result.count++;
        return result;
    }

    // Reverse-order search for correct shadowing
    consteval Expression<Cap> lookup(const char* name) const {
        for (int i = count - 1; i >= 0; --i)
            if (str_eq(names[i], name))
                return types[i];
        throw "type error: unbound variable";
    }

    consteval bool has(const char* name) const {
        for (int i = count - 1; i >= 0; --i)
            if (str_eq(names[i], name))
                return true;
        return false;
    }
};

} // namespace reftype

#endif // REFTYPE_TYPE_ENV_HPP
