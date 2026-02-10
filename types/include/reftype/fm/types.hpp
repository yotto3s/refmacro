#ifndef REFTYPE_FM_TYPES_HPP
#define REFTYPE_FM_TYPES_HPP

#include <cstddef>
#include <refmacro/ast.hpp>

namespace reftype::fm {

// A single term in a linear expression: coeff * var
struct LinearTerm {
    int var_id{-1};
    double coeff{0.0};
};

// Max terms per inequality. FM elimination combines two inequalities,
// so the result can have up to 2*(MaxTerms-1) terms in the worst case.
// 8 is sufficient for typical refinement-type constraints.
inline constexpr std::size_t MaxTermsPerIneq = 8;

// A linear inequality: sum(terms) + constant OP 0
// Normalized form: all inequalities are "expr >= 0" or "expr > 0"
// For "expr <= 0": negate all terms and constant, keep strictness
// For "expr = 0": represented as two inequalities (>= 0 and <= 0)
struct LinearInequality {
    LinearTerm terms[MaxTermsPerIneq]{};
    std::size_t term_count{0};
    double constant{0.0};
    bool strict{false}; // true for < and >, false for <= and >=

    // Build an inequality from terms, enforcing term_count invariant.
    static consteval LinearInequality
    make(std::initializer_list<LinearTerm> ts, double c, bool s = false) {
        LinearInequality result{};
        if (ts.size() > MaxTermsPerIneq)
            throw "LinearInequality: too many terms";
        for (auto& t : ts)
            result.terms[result.term_count++] = t;
        result.constant = c;
        result.strict = s;
        return result;
    }
};

// Variable metadata: name + integer/real type.
// MaxVars: max variables tracked. 16 covers most refinement-type systems.
template <std::size_t MaxVars = 16>
struct VarInfo {
    char names[MaxVars][16]{};
    bool is_integer[MaxVars]{};
    std::size_t count{0};

    consteval int find_or_add(const char* name, bool integer = true) {
        for (std::size_t i = 0; i < count; ++i)
            if (refmacro::str_eq(names[i], name))
                return static_cast<int>(i);
        if (count >= MaxVars)
            throw "VarInfo capacity exceeded";
        refmacro::copy_str(names[count], name);
        is_integer[count] = integer;
        return static_cast<int>(count++);
    }

    consteval int find(const char* name) const {
        for (std::size_t i = 0; i < count; ++i)
            if (refmacro::str_eq(names[i], name))
                return static_cast<int>(i);
        return -1;
    }
};

// A system of linear inequalities.
// MaxIneqs: max constraints. FM elimination can grow quadratically
// per variable eliminated — 64 handles moderate-size systems.
template <std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
struct InequalitySystem {
    LinearInequality ineqs[MaxIneqs]{};
    std::size_t count{0};
    VarInfo<MaxVars> vars{};

    consteval InequalitySystem add(LinearInequality ineq) const {
        if (count >= MaxIneqs)
            throw "InequalitySystem capacity exceeded";
        InequalitySystem result = *this;
        result.ineqs[result.count++] = ineq;
        return result;
    }
};

} // namespace reftype::fm

#endif // REFTYPE_FM_TYPES_HPP
