#ifndef REFTYPE_CONSTRAINTS_HPP
#define REFTYPE_CONSTRAINTS_HPP

#include <refmacro/expr.hpp>
#include <refmacro/str_utils.hpp>

namespace reftype {

using refmacro::copy_str;
using refmacro::Expression;

// A single constraint: a boolean formula with an origin string for error
// reporting.
template <std::size_t Cap = 128> struct Constraint {
    Expression<Cap> formula{};
    char origin[64]{};
};

// Immutable set of constraints. add() and merge() return new sets.
// Available infrastructure for deferred constraint solving.
// Currently, the type checker calls is_subtype() directly at annotation points.
// This type is tested and ready for future extensions (e.g., global constraint
// collection).
template <std::size_t Cap = 128, std::size_t MaxConstraints = 32>
struct ConstraintSet {
    Constraint<Cap> constraints[MaxConstraints]{};
    std::size_t count{0};

    consteval ConstraintSet add(Expression<Cap> formula,
                                const char* origin) const {
        if (count >= MaxConstraints)
            throw "ConstraintSet capacity exceeded";
        ConstraintSet result = *this;
        result.constraints[result.count].formula = formula;
        copy_str(result.constraints[result.count].origin, origin, 64);
        result.count++;
        return result;
    }

    consteval ConstraintSet merge(const ConstraintSet& other) const {
        ConstraintSet result = *this;
        for (std::size_t i = 0; i < other.count; ++i) {
            if (result.count >= MaxConstraints)
                throw "ConstraintSet capacity exceeded on merge";
            result.constraints[result.count++] = other.constraints[i];
        }
        return result;
    }
};

} // namespace reftype

#endif // REFTYPE_CONSTRAINTS_HPP
