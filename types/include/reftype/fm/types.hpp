#ifndef REFTYPE_FM_TYPES_HPP
#define REFTYPE_FM_TYPES_HPP

#include <refmacro/ast.hpp>

namespace reftype::fm {

// A single term in a linear expression: coeff * var
struct LinearTerm {
    int var_id{-1};
    double coeff{0.0};
};

// A linear inequality: sum(terms) + constant OP 0
// Normalized form: all inequalities are "expr >= 0" or "expr > 0"
// For "expr <= 0": negate all terms and constant, keep strictness
// For "expr = 0": represented as two inequalities (>= 0 and <= 0)
struct LinearInequality {
    LinearTerm terms[8]{};
    int term_count{0};
    double constant{0.0};
    bool strict{false}; // true for < and >, false for <= and >=
};

// Variable metadata: name + integer/real type
template <int MaxVars = 16>
struct VarInfo {
    char names[MaxVars][16]{};
    bool is_integer[MaxVars]{};
    int count{0};

    consteval int find_or_add(const char* name, bool integer = true) {
        for (int i = 0; i < count; ++i)
            if (refmacro::str_eq(names[i], name)) return i;
        refmacro::copy_str(names[count], name);
        is_integer[count] = integer;
        return count++;
    }

    consteval int find(const char* name) const {
        for (int i = 0; i < count; ++i)
            if (refmacro::str_eq(names[i], name)) return i;
        return -1;
    }
};

// A system of linear inequalities
template <int MaxIneqs = 64, int MaxVars = 16>
struct InequalitySystem {
    LinearInequality ineqs[MaxIneqs]{};
    int count{0};
    VarInfo<MaxVars> vars{};

    consteval InequalitySystem add(LinearInequality ineq) const {
        InequalitySystem result = *this;
        result.ineqs[result.count++] = ineq;
        return result;
    }
};

} // namespace reftype::fm

#endif // REFTYPE_FM_TYPES_HPP
