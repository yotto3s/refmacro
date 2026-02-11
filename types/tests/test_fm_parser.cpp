#include <gtest/gtest.h>
#include <refmacro/math.hpp>
#include <reftype/fm/parser.hpp>

using namespace reftype::fm;
using namespace refmacro;

// ============================================================
// Arithmetic parsing: parse_arith
// ============================================================

TEST(ParseArith, Literal) {
    static constexpr auto e = Expression<>::lit(5.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.constant == 5.0);
    static_assert(result.coeffs[0] == 0.0);
}

TEST(ParseArith, Variable) {
    static constexpr auto e = Expression<>::var("x");
    constexpr auto result = [] {
        VarInfo<> vars{};
        auto r = parse_arith<64>(NodeView{e.ast, e.id}, vars);
        return r;
    }();
    // var("x") → coeffs[0] = 1 (first var registered gets id 0)
    static_assert(result.coeffs[0] == 1.0);
    static_assert(result.constant == 0.0);
}

TEST(ParseArith, AddVarAndLit) {
    // x + 3
    static constexpr auto e =
        Expression<>::var("x") + Expression<>::lit(3.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.coeffs[0] == 1.0);
    static_assert(result.constant == 3.0);
}

TEST(ParseArith, SubVars) {
    // x - y
    static constexpr auto e =
        Expression<>::var("x") - Expression<>::var("y");
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.coeffs[0] == 1.0);   // x
    static_assert(result.coeffs[1] == -1.0);  // y
    static_assert(result.constant == 0.0);
}

TEST(ParseArith, MulByConstant) {
    // 2 * x
    static constexpr auto e =
        Expression<>::lit(2.0) * Expression<>::var("x");
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.coeffs[0] == 2.0);
    static_assert(result.constant == 0.0);
}

TEST(ParseArith, MulVarByConstantRHS) {
    // x * 3
    static constexpr auto e =
        Expression<>::var("x") * Expression<>::lit(3.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.coeffs[0] == 3.0);
}

TEST(ParseArith, Negation) {
    // -x
    static constexpr auto e = -Expression<>::var("x");
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.coeffs[0] == -1.0);
    static_assert(result.constant == 0.0);
}

TEST(ParseArith, DivByConstant) {
    // x / 2
    static constexpr auto e =
        Expression<>::var("x") / Expression<>::lit(2.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.coeffs[0] == 0.5);
    static_assert(result.constant == 0.0);
}

TEST(ParseArith, ComplexExpression) {
    // 2*x + 3*y - 5
    static constexpr auto e =
        Expression<>::lit(2.0) * Expression<>::var("x") +
        Expression<>::lit(3.0) * Expression<>::var("y") -
        Expression<>::lit(5.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.coeffs[0] == 2.0);   // x
    static_assert(result.coeffs[1] == 3.0);   // y
    static_assert(result.constant == -5.0);
}

// ============================================================
// Comparison parsing (single clause)
// ============================================================

TEST(ParseFormula, GreaterThan) {
    // x > 0 → single clause, one inequality: x > 0
    static constexpr auto e = Expression<>::var("x") > 0.0;
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == true);
    static_assert(result.system().ineqs[0].terms[0].coeff == 1.0);
    static_assert(result.system().ineqs[0].constant == 0.0);
}

TEST(ParseFormula, GreaterEqual) {
    // x >= 3 → x - 3 >= 0
    static constexpr auto e = Expression<>::var("x") >= 3.0;
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == false);
    static_assert(result.system().ineqs[0].terms[0].coeff == 1.0);
    static_assert(result.system().ineqs[0].constant == -3.0);
}

TEST(ParseFormula, LessThan) {
    // x < 5 → 5 - x > 0 → terms: {x, -1}, constant: 5, strict: true
    static constexpr auto e = Expression<>::var("x") < 5.0;
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == true);
    static_assert(result.system().ineqs[0].terms[0].coeff == -1.0);
    static_assert(result.system().ineqs[0].constant == 5.0);
}

TEST(ParseFormula, LessEqual) {
    // x <= 10 → 10 - x >= 0
    static constexpr auto e = Expression<>::var("x") <= 10.0;
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == false);
    static_assert(result.system().ineqs[0].terms[0].coeff == -1.0);
    static_assert(result.system().ineqs[0].constant == 10.0);
}

TEST(ParseFormula, Equality) {
    // x == 5 → (x - 5 >= 0) AND (5 - x >= 0)
    static constexpr auto e = Expression<>::var("x") == 5.0;
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 2);
    // First: x - 5 >= 0
    static_assert(result.system().ineqs[0].terms[0].coeff == 1.0);
    static_assert(result.system().ineqs[0].constant == -5.0);
    static_assert(result.system().ineqs[0].strict == false);
    // Second: -x + 5 >= 0
    static_assert(result.system().ineqs[1].terms[0].coeff == -1.0);
    static_assert(result.system().ineqs[1].constant == 5.0);
    static_assert(result.system().ineqs[1].strict == false);
}

// ============================================================
// Conjunction
// ============================================================

TEST(ParseFormula, Conjunction) {
    // x > 0 && x < 5 → 1 clause, 2 inequalities
    static constexpr auto e =
        (Expression<>::var("x") > 0.0) && (Expression<>::var("x") < 5.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 2);
}

TEST(ParseFormula, ConjunctionTwoVars) {
    // x > 0 && y > 0 → 1 clause, 2 inequalities, 2 variables
    static constexpr auto e =
        (Expression<>::var("x") > 0.0) && (Expression<>::var("y") > 0.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 2);
    static_assert(result.system().vars.count == 2);
}

// ============================================================
// Disjunction
// ============================================================

TEST(ParseFormula, Disjunction) {
    // x > 0 || x < -1 → 2 clauses
    static constexpr auto e =
        (Expression<>::var("x") > 0.0) || (Expression<>::var("x") < -1.0);
    constexpr auto result = parse_to_system(e);
    static_assert(!result.is_conjunctive());
    static_assert(result.clause_count == 2);
    static_assert(result.clauses[0].count == 1);
    static_assert(result.clauses[1].count == 1);
}

// ============================================================
// Negation
// ============================================================

TEST(ParseFormula, NegateGt) {
    // !(x > 0) → x <= 0 → -x >= 0
    static constexpr auto e = !(Expression<>::var("x") > 0.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == false);
    // le(x, 0) → 0 - x >= 0 → coeff = -1, constant = 0
    static_assert(result.system().ineqs[0].terms[0].coeff == -1.0);
    static_assert(result.system().ineqs[0].constant == 0.0);
}

TEST(ParseFormula, NegateGe) {
    // !(x >= 3) → x < 3 → 3 - x > 0
    static constexpr auto e = !(Expression<>::var("x") >= 3.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == true);
    static_assert(result.system().ineqs[0].terms[0].coeff == -1.0);
    static_assert(result.system().ineqs[0].constant == 3.0);
}

TEST(ParseFormula, NegateLt) {
    // !(x < 5) → x >= 5 → x - 5 >= 0
    static constexpr auto e = !(Expression<>::var("x") < 5.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == false);
    static_assert(result.system().ineqs[0].terms[0].coeff == 1.0);
    static_assert(result.system().ineqs[0].constant == -5.0);
}

TEST(ParseFormula, NegateLe) {
    // !(x <= 5) → x > 5 → x - 5 > 0
    static constexpr auto e = !(Expression<>::var("x") <= 5.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == true);
    static_assert(result.system().ineqs[0].terms[0].coeff == 1.0);
    static_assert(result.system().ineqs[0].constant == -5.0);
}

TEST(ParseFormula, NegateEq) {
    // !(x == 5) → (x < 5) OR (x > 5) → 2 clauses
    static constexpr auto e = !(Expression<>::var("x") == 5.0);
    constexpr auto result = parse_to_system(e);
    static_assert(!result.is_conjunctive());
    static_assert(result.clause_count == 2);
    // Both clauses have 1 strict inequality
    static_assert(result.clauses[0].count == 1);
    static_assert(result.clauses[0].ineqs[0].strict == true);
    static_assert(result.clauses[1].count == 1);
    static_assert(result.clauses[1].ineqs[0].strict == true);
}

TEST(ParseFormula, DeMorganAnd) {
    // !(x > 0 && y > 0) → (x <= 0) || (y <= 0) → 2 clauses
    static constexpr auto e =
        !((Expression<>::var("x") > 0.0) && (Expression<>::var("y") > 0.0));
    constexpr auto result = parse_to_system(e);
    static_assert(!result.is_conjunctive());
    static_assert(result.clause_count == 2);
    static_assert(result.clauses[0].count == 1);
    static_assert(result.clauses[1].count == 1);
}

TEST(ParseFormula, DeMorganOr) {
    // !(x > 0 || y > 0) → (x <= 0) && (y <= 0) → 1 clause, 2 ineqs
    static constexpr auto e =
        !((Expression<>::var("x") > 0.0) || (Expression<>::var("y") > 0.0));
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 2);
}

TEST(ParseFormula, DoubleNegation) {
    // !!(x > 0) → x > 0
    static constexpr auto e = !!(Expression<>::var("x") > 0.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == true);
    static_assert(result.system().ineqs[0].terms[0].coeff == 1.0);
}

// ============================================================
// Distribution: conjunction over disjunction
// ============================================================

TEST(ParseFormula, DistributeConjOverDisj) {
    // (x > 0 || x < -1) && y > 0
    // → 2 clauses: {x>0, y>0} and {x<-1, y>0}
    static constexpr auto e =
        ((Expression<>::var("x") > 0.0) || (Expression<>::var("x") < -1.0)) &&
        (Expression<>::var("y") > 0.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.clause_count == 2);
    static_assert(result.clauses[0].count == 2);  // x>0 AND y>0
    static_assert(result.clauses[1].count == 2);  // x<-1 AND y>0
}

// ============================================================
// Two-variable comparisons
// ============================================================

TEST(ParseFormula, TwoVarComparison) {
    // x > y → x - y > 0
    static constexpr auto e =
        Expression<>::var("x") > Expression<>::var("y");
    constexpr auto result = parse_to_system(e);
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().ineqs[0].strict == true);
    static_assert(result.system().ineqs[0].term_count == 2);
    // x has coeff 1, y has coeff -1
    static_assert(result.system().ineqs[0].terms[0].coeff == 1.0);
    static_assert(result.system().ineqs[0].terms[1].coeff == -1.0);
}

// ============================================================
// Variable tracking
// ============================================================

TEST(ParseFormula, VarsRegistered) {
    // x > 0 && y < 10 — vars should track both x and y
    static constexpr auto e =
        (Expression<>::var("x") > 0.0) && (Expression<>::var("y") < 10.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.system().vars.count == 2);
}

TEST(ParseFormula, SharedVarsAcrossDisjunction) {
    // (x > 0 || y > 0) — after full parse, all clauses share the same VarInfo
    static constexpr auto e =
        (Expression<>::var("x") > 0.0) || (Expression<>::var("y") > 0.0);
    constexpr auto result = parse_to_system(e);
    static_assert(result.clause_count == 2);
    // Both clauses see both variables after VarInfo propagation
    static_assert(result.clauses[0].vars.count == 2);
    static_assert(result.clauses[1].vars.count == 2);
}

// ============================================================
// parse_to_system with caller-supplied VarInfo (real-valued)
// ============================================================

TEST(ParseToSystemVarInfo, RealValuedVariables) {
    // Pre-register x as real, then parse x > 0
    static constexpr auto e = Expression<>::var("x") > 0.0;
    constexpr auto result = [] {
        VarInfo<> vars{};
        vars.find_or_add("x", false);  // real-valued
        return parse_to_system(e, vars);
    }();
    static_assert(result.is_conjunctive());
    static_assert(result.system().count == 1);
    static_assert(result.system().vars.is_integer[0] == false);
    static_assert(result.system().ineqs[0].terms[0].coeff == 1.0);
}

TEST(ParseToSystemVarInfo, RealNewVarInheritsType) {
    // Pre-register x as real; y discovered during parsing should also be real
    static constexpr auto e =
        (Expression<>::var("x") > 0.0) && (Expression<>::var("y") < 5.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        vars.find_or_add("x", false);  // real-valued
        return parse_to_system(e, vars);
    }();
    static_assert(result.is_conjunctive());
    static_assert(result.system().vars.count == 2);
    static_assert(result.system().vars.is_integer[0] == false);  // x
    static_assert(result.system().vars.is_integer[1] == false);  // y
}

TEST(ParseToSystemVarInfo, RealDisjunction) {
    // Real-valued variables with disjunction
    static constexpr auto e =
        (Expression<>::var("x") > 0.0) || (Expression<>::var("x") < -1.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        vars.find_or_add("x", false);
        return parse_to_system(e, vars);
    }();
    static_assert(result.clause_count == 2);
    // Both clauses should have real-valued x after VarInfo propagation
    static_assert(result.clauses[0].vars.is_integer[0] == false);
    static_assert(result.clauses[1].vars.is_integer[0] == false);
}

TEST(ParseToSystemVarInfo, IntegerDefault) {
    // Zero-arg overload defaults to integer — verify behavior matches
    static constexpr auto e = Expression<>::var("x") > 0.0;
    constexpr auto result_default = parse_to_system(e);
    constexpr auto result_explicit = [] {
        VarInfo<> vars{};
        return parse_to_system(e, vars);
    }();
    static_assert(result_default.system().vars.is_integer[0] == true);
    static_assert(result_explicit.system().vars.is_integer[0] == true);
    static_assert(result_default.system().count == result_explicit.system().count);
}

// ============================================================
// Error boundary tests
// ============================================================
// The following parse errors are consteval — they produce compile-time
// failures and cannot be tested at runtime:
//   - mul(var, var) → "non-linear: variable * variable"
//   - div(a, var)   → "non-linear: division by variable"
//   - div(a, 0)     → "division by zero"
//   - unknown tag   → "unsupported node in refinement predicate"
// The tests below verify the boundary: expressions just inside the
// valid region compile and parse correctly.

TEST(ParseArith, MulConstantTimesConstant) {
    // 2 * 3 → constant 6 (both sides constant, no non-linear error)
    static constexpr auto e =
        Expression<>::lit(2.0) * Expression<>::lit(3.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.constant == 6.0);
    static_assert(is_constant_expr(result));
}

TEST(ParseArithReal, InheritsRealType) {
    // Pre-register x as real, parse 2*x + y - 3
    // Verify coefficients AND that both x,y are real in resulting VarInfo
    static constexpr auto e =
        Expression<>::lit(2.0) * Expression<>::var("x") +
        Expression<>::var("y") - Expression<>::lit(3.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        vars.find_or_add("x", false);  // real
        auto r = parse_arith<64>(NodeView{e.ast, e.id}, vars);
        return std::pair{r, vars};
    }();
    static_assert(result.first.coeffs[0] == 2.0);   // x
    static_assert(result.first.coeffs[1] == 1.0);    // y
    static_assert(result.first.constant == -3.0);
    static_assert(result.second.count == 2);
    static_assert(result.second.is_integer[0] == false);  // x: real
    static_assert(result.second.is_integer[1] == false);  // y: inherits real
}

TEST(ParseArith, DivByNonZeroConstant) {
    // x / 3 → valid (divisor is non-zero constant)
    static constexpr auto e =
        Expression<>::var("x") / Expression<>::lit(3.0);
    constexpr auto result = [] {
        VarInfo<> vars{};
        return parse_arith<64>(NodeView{e.ast, e.id}, vars);
    }();
    static_assert(result.coeffs[0] == 1.0 / 3.0);
}
