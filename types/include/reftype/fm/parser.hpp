#ifndef REFTYPE_FM_PARSER_HPP
#define REFTYPE_FM_PARSER_HPP

#include <cstddef>
#include <refmacro/expr.hpp>
#include <refmacro/node_view.hpp>
#include <refmacro/str_utils.hpp>
#include <reftype/fm/types.hpp>

namespace reftype::fm {

// --- LinearExpr: intermediate coefficient vector ---

template <std::size_t MaxVars = 16> struct LinearExpr {
    double coeffs[MaxVars]{};
    double constant{0.0};
};

template <std::size_t MaxVars>
consteval LinearExpr<MaxVars> add_expr(const LinearExpr<MaxVars>& a,
                                       const LinearExpr<MaxVars>& b) {
    LinearExpr<MaxVars> r{};
    for (std::size_t i = 0; i < MaxVars; ++i)
        r.coeffs[i] = a.coeffs[i] + b.coeffs[i];
    r.constant = a.constant + b.constant;
    return r;
}

template <std::size_t MaxVars>
consteval LinearExpr<MaxVars> negate_expr(const LinearExpr<MaxVars>& a) {
    LinearExpr<MaxVars> r{};
    for (std::size_t i = 0; i < MaxVars; ++i)
        r.coeffs[i] = -a.coeffs[i];
    r.constant = -a.constant;
    return r;
}

template <std::size_t MaxVars>
consteval LinearExpr<MaxVars> sub_expr(const LinearExpr<MaxVars>& a,
                                       const LinearExpr<MaxVars>& b) {
    return add_expr(a, negate_expr(b));
}

template <std::size_t MaxVars>
consteval LinearExpr<MaxVars> scale_expr(const LinearExpr<MaxVars>& a,
                                         double factor) {
    LinearExpr<MaxVars> r{};
    for (std::size_t i = 0; i < MaxVars; ++i)
        r.coeffs[i] = a.coeffs[i] * factor;
    r.constant = a.constant * factor;
    return r;
}

template <std::size_t MaxVars>
consteval bool is_constant_expr(const LinearExpr<MaxVars>& a) {
    for (std::size_t i = 0; i < MaxVars; ++i)
        if (a.coeffs[i] != 0.0)
            return false;
    return true;
}

// --- ParseResult: DNF of InequalitySystem clauses ---

template <std::size_t MaxClauses = 8, std::size_t MaxIneqs = 64,
          std::size_t MaxVars = 16>
struct ParseResult {
    InequalitySystem<MaxIneqs, MaxVars> clauses[MaxClauses]{};
    std::size_t clause_count{0};

    consteval bool is_conjunctive() const { return clause_count == 1; }

    consteval const InequalitySystem<MaxIneqs, MaxVars>& system() const {
        if (clause_count != 1)
            throw "ParseResult::system(): not a conjunctive formula";
        return clauses[0];
    }

    consteval ParseResult
    add_clause(const InequalitySystem<MaxIneqs, MaxVars>& sys) const {
        if (clause_count >= MaxClauses)
            throw "DNF clause limit exceeded";
        ParseResult r = *this;
        r.clauses[r.clause_count++] = sys;
        return r;
    }
};

// Make a single-clause ParseResult from an InequalitySystem
template <std::size_t MaxClauses = 8, std::size_t MaxIneqs = 64,
          std::size_t MaxVars = 16>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
single_clause(const InequalitySystem<MaxIneqs, MaxVars>& sys) {
    ParseResult<MaxClauses, MaxIneqs, MaxVars> r{};
    r.clauses[0] = sys;
    r.clause_count = 1;
    return r;
}

// Merge two InequalitySystems into one (conjunction within a clause)
// Takes vars from whichever system has more (right is always a superset
// since vars accumulate left-to-right during parsing).
template <std::size_t MaxIneqs, std::size_t MaxVars>
consteval InequalitySystem<MaxIneqs, MaxVars>
merge_systems(const InequalitySystem<MaxIneqs, MaxVars>& a,
              const InequalitySystem<MaxIneqs, MaxVars>& b) {
    auto r = a;
    if (b.vars.count > r.vars.count)
        r.vars = b.vars;
    for (std::size_t i = 0; i < b.count; ++i)
        r = r.add(b.ineqs[i]);
    return r;
}

// Conjoin: cross-product of clauses (DNF distribution)
template <std::size_t MaxClauses, std::size_t MaxIneqs, std::size_t MaxVars>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
conjoin(const ParseResult<MaxClauses, MaxIneqs, MaxVars>& left,
        const ParseResult<MaxClauses, MaxIneqs, MaxVars>& right) {
    ParseResult<MaxClauses, MaxIneqs, MaxVars> r{};
    for (std::size_t i = 0; i < left.clause_count; ++i)
        for (std::size_t j = 0; j < right.clause_count; ++j) {
            if (r.clause_count >= MaxClauses)
                throw "DNF clause limit exceeded";
            r.clauses[r.clause_count++] =
                merge_systems(left.clauses[i], right.clauses[j]);
        }
    return r;
}

// Disjoin: concatenate clauses
template <std::size_t MaxClauses, std::size_t MaxIneqs, std::size_t MaxVars>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
disjoin(const ParseResult<MaxClauses, MaxIneqs, MaxVars>& left,
        const ParseResult<MaxClauses, MaxIneqs, MaxVars>& right) {
    ParseResult<MaxClauses, MaxIneqs, MaxVars> r{};
    for (std::size_t i = 0; i < left.clause_count; ++i) {
        if (r.clause_count >= MaxClauses)
            throw "DNF clause limit exceeded";
        r.clauses[r.clause_count++] = left.clauses[i];
    }
    for (std::size_t i = 0; i < right.clause_count; ++i) {
        if (r.clause_count >= MaxClauses)
            throw "DNF clause limit exceeded";
        r.clauses[r.clause_count++] = right.clauses[i];
    }
    return r;
}

// --- LinearExpr → LinearInequality conversion ---

// Convert (lhs - rhs) into a LinearInequality: sum(terms) + constant OP 0
template <std::size_t MaxVars>
consteval LinearInequality to_inequality(const LinearExpr<MaxVars>& lhs,
                                         const LinearExpr<MaxVars>& rhs,
                                         bool strict) {
    auto diff = sub_expr(lhs, rhs);
    LinearInequality ineq{};
    ineq.strict = strict;
    ineq.constant = diff.constant;
    for (std::size_t i = 0; i < MaxVars; ++i) {
        if (diff.coeffs[i] != 0.0) {
            if (ineq.term_count >= MaxTermsPerIneq)
                throw "too many variable terms in inequality";
            ineq.terms[ineq.term_count++] =
                LinearTerm{static_cast<int>(i), diff.coeffs[i]};
        }
    }
    return ineq;
}

// --- Arithmetic parser ---

template <std::size_t Cap, std::size_t MaxVars>
consteval LinearExpr<MaxVars>
parse_arith(refmacro::NodeView<Cap> node, VarInfo<MaxVars>& vars) {
    auto t = node.tag();

    if (t == "lit") {
        LinearExpr<MaxVars> r{};
        r.constant = node.payload();
        return r;
    }

    if (t == "var") {
        LinearExpr<MaxVars> r{};
        int id = vars.find_or_add(node.name().data());
        r.coeffs[id] = 1.0;
        return r;
    }

    if (t == "add") {
        auto a = parse_arith<Cap>(node.child(0), vars);
        auto b = parse_arith<Cap>(node.child(1), vars);
        return add_expr(a, b);
    }

    if (t == "sub") {
        auto a = parse_arith<Cap>(node.child(0), vars);
        auto b = parse_arith<Cap>(node.child(1), vars);
        return sub_expr(a, b);
    }

    if (t == "neg") {
        auto a = parse_arith<Cap>(node.child(0), vars);
        return negate_expr(a);
    }

    if (t == "mul") {
        auto a = parse_arith<Cap>(node.child(0), vars);
        auto b = parse_arith<Cap>(node.child(1), vars);
        if (is_constant_expr(a))
            return scale_expr(b, a.constant);
        if (is_constant_expr(b))
            return scale_expr(a, b.constant);
        throw "non-linear: variable * variable";
    }

    if (t == "div") {
        auto a = parse_arith<Cap>(node.child(0), vars);
        auto b = parse_arith<Cap>(node.child(1), vars);
        if (!is_constant_expr(b))
            throw "non-linear: division by variable";
        if (b.constant == 0.0)
            throw "division by zero";
        return scale_expr(a, 1.0 / b.constant);
    }

    throw "unsupported node in arithmetic expression";
}

// --- Formula parser ---

// Forward declaration for mutual recursion
template <std::size_t Cap, std::size_t MaxClauses, std::size_t MaxIneqs,
          std::size_t MaxVars>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
parse_negated(refmacro::NodeView<Cap> node, VarInfo<MaxVars>& vars);

// Build a ParseResult from a comparison (optionally negated)
template <std::size_t Cap, std::size_t MaxClauses, std::size_t MaxIneqs,
          std::size_t MaxVars>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
parse_comparison(refmacro::NodeView<Cap> node, VarInfo<MaxVars>& vars,
                 bool negate) {
    auto t = node.tag();
    auto lhs = parse_arith<Cap>(node.child(0), vars);
    auto rhs = parse_arith<Cap>(node.child(1), vars);

    if (t == "eq" && !negate) {
        // a == b → (a - b >= 0) AND (b - a >= 0)
        auto ineq1 = to_inequality(lhs, rhs, false);
        auto ineq2 = to_inequality(rhs, lhs, false);
        InequalitySystem<MaxIneqs, MaxVars> sys{};
        sys.vars = vars;
        sys = sys.add(ineq1);
        sys = sys.add(ineq2);
        return single_clause<MaxClauses>(sys);
    }

    if (t == "eq" && negate) {
        // !(a == b) → (a < b) OR (a > b)
        auto ineq_lt = to_inequality(rhs, lhs, true);
        auto ineq_gt = to_inequality(lhs, rhs, true);
        InequalitySystem<MaxIneqs, MaxVars> sys_lt{};
        sys_lt.vars = vars;
        sys_lt = sys_lt.add(ineq_lt);
        InequalitySystem<MaxIneqs, MaxVars> sys_gt{};
        sys_gt.vars = vars;
        sys_gt = sys_gt.add(ineq_gt);
        ParseResult<MaxClauses, MaxIneqs, MaxVars> r{};
        r.clauses[0] = sys_lt;
        r.clauses[1] = sys_gt;
        r.clause_count = 2;
        return r;
    }

    // For non-eq comparisons, determine the direction
    LinearInequality ineq{};
    if ((t == "gt" && !negate) || (t == "le" && negate)) {
        ineq = to_inequality(lhs, rhs, true);   // lhs - rhs > 0
    } else if ((t == "ge" && !negate) || (t == "lt" && negate)) {
        ineq = to_inequality(lhs, rhs, false);   // lhs - rhs >= 0
    } else if ((t == "lt" && !negate) || (t == "ge" && negate)) {
        ineq = to_inequality(rhs, lhs, true);   // rhs - lhs > 0
    } else if ((t == "le" && !negate) || (t == "gt" && negate)) {
        ineq = to_inequality(rhs, lhs, false);   // rhs - lhs >= 0
    } else {
        throw "unsupported comparison tag";
    }

    InequalitySystem<MaxIneqs, MaxVars> sys{};
    sys.vars = vars;
    sys = sys.add(ineq);
    return single_clause<MaxClauses>(sys);
}

template <std::size_t Cap, std::size_t MaxClauses = 8,
          std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
parse_formula(refmacro::NodeView<Cap> node, VarInfo<MaxVars>& vars) {
    auto t = node.tag();

    if (t == "gt" || t == "ge" || t == "lt" || t == "le" || t == "eq")
        return parse_comparison<Cap, MaxClauses, MaxIneqs>(node, vars, false);

    if (t == "land") {
        auto left =
            parse_formula<Cap, MaxClauses, MaxIneqs>(node.child(0), vars);
        auto right =
            parse_formula<Cap, MaxClauses, MaxIneqs>(node.child(1), vars);
        return conjoin(left, right);
    }

    if (t == "lor") {
        auto left =
            parse_formula<Cap, MaxClauses, MaxIneqs>(node.child(0), vars);
        auto right =
            parse_formula<Cap, MaxClauses, MaxIneqs>(node.child(1), vars);
        return disjoin(left, right);
    }

    if (t == "lnot")
        return parse_negated<Cap, MaxClauses, MaxIneqs>(node.child(0), vars);

    throw "unsupported node in refinement predicate";
}

template <std::size_t Cap, std::size_t MaxClauses, std::size_t MaxIneqs,
          std::size_t MaxVars>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
parse_negated(refmacro::NodeView<Cap> node, VarInfo<MaxVars>& vars) {
    auto t = node.tag();

    // Negated comparisons
    if (t == "gt" || t == "ge" || t == "lt" || t == "le" || t == "eq")
        return parse_comparison<Cap, MaxClauses, MaxIneqs>(node, vars, true);

    // De Morgan: !(a && b) → !a || !b
    if (t == "land") {
        auto left =
            parse_negated<Cap, MaxClauses, MaxIneqs>(node.child(0), vars);
        auto right =
            parse_negated<Cap, MaxClauses, MaxIneqs>(node.child(1), vars);
        return disjoin(left, right);
    }

    // De Morgan: !(a || b) → !a && !b
    if (t == "lor") {
        auto left =
            parse_negated<Cap, MaxClauses, MaxIneqs>(node.child(0), vars);
        auto right =
            parse_negated<Cap, MaxClauses, MaxIneqs>(node.child(1), vars);
        return conjoin(left, right);
    }

    // Double negation: !!a → a
    if (t == "lnot")
        return parse_formula<Cap, MaxClauses, MaxIneqs>(node.child(0), vars);

    throw "unsupported node in negated formula";
}

// --- Top-level API ---

template <std::size_t Cap, std::size_t MaxClauses = 8,
          std::size_t MaxIneqs = 64, std::size_t MaxVars = 16>
consteval ParseResult<MaxClauses, MaxIneqs, MaxVars>
parse_to_system(const refmacro::Expression<Cap>& formula) {
    VarInfo<MaxVars> vars{};
    auto result = parse_formula<Cap, MaxClauses, MaxIneqs>(
        refmacro::NodeView<Cap>{formula.ast, formula.id}, vars);
    // Propagate the final VarInfo (with all variables discovered during
    // parsing) back to every clause, so cross-clause algorithms in
    // Phase 6f see a consistent variable registry.
    for (std::size_t i = 0; i < result.clause_count; ++i)
        result.clauses[i].vars = vars;
    return result;
}

} // namespace reftype::fm

#endif // REFTYPE_FM_PARSER_HPP
