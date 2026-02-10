#ifndef REFTYPE_FM_TYPES_HPP
#define REFTYPE_FM_TYPES_HPP

#include <cstddef>
#include <climits>
#include <initializer_list>
#include <optional>
#include <refmacro/str_utils.hpp>

namespace reftype::fm {

// A single term in a linear expression: coeff * var
struct LinearTerm {
    int var_id{-1};
    double coeff{0.0};
};

// Max terms stored per inequality. The FM elimination step must
// merge/simplify combined terms to fit within this cap.
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
    static consteval LinearInequality make(std::initializer_list<LinearTerm> ts,
                                           double c, bool s = false) {
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
template <std::size_t MaxVars = 16> struct VarInfo {
    static_assert(MaxVars <= INT_MAX, "MaxVars must fit in int (var_id is int)");

    char names[MaxVars][16]{};
    bool is_integer[MaxVars]{};
    std::size_t count{0};

    consteval int find_or_add(const char* name, bool integer = true) {
        for (std::size_t i = 0; i < count; ++i)
            if (refmacro::str_eq(names[i], name))
                return static_cast<int>(i);
        if (count >= MaxVars)
            throw "VarInfo capacity exceeded";
        if (refmacro::str_len(name) >= sizeof(names[0]))
            throw "VarInfo name too long";
        refmacro::copy_str(names[count], name, sizeof(names[0]));
        is_integer[count] = integer;
        return static_cast<int>(count++);
    }

    consteval std::optional<int> find(const char* name) const {
        for (std::size_t i = 0; i < count; ++i)
            if (refmacro::str_eq(names[i], name))
                return static_cast<int>(i);
        return std::nullopt;
    }
};

// A system of linear inequalities.
// MaxIneqs: max constraints. FM elimination can grow quadratically
// per variable eliminated — 64 handles moderate-size systems.
//
// Usage: add() returns an immutable copy with the new inequality appended.
// Since vars is copied along with the system, register all variables on
// the base system *before* chaining add() calls:
//   auto s = InequalitySystem<>{};
//   int x = s.vars.find_or_add("x");
//   auto s2 = s.add(ineq1).add(ineq2);  // s2.vars contains x
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
