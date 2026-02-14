// F1: Multi-variable integer rounding with non-integer coefficients
// is unsound — over-tightens bounds, causing false UNSAT.
//
// rounding.hpp:53 — when term_count > 1, round_integer_bound falls
// back to coeff=1.0, but this is wrong when the target variable's
// coefficient is non-integer (e.g., 0.5).
//
// This test SHOULD compile when the bug is fixed.
// While the bug exists, it fails to compile because fm_is_unsat
// incorrectly returns true, failing the static_assert.

#include <reftype/fm/eliminate.hpp>
#include <reftype/fm/types.hpp>

using namespace reftype::fm;

consteval bool check() {
    InequalitySystem<> sys{};
    int x = sys.vars.find_or_add("x", true);
    int y = sys.vars.find_or_add("y", true);
    // 0.5*x + y - 0.3 >= 0  (x=1, y=0 → 0.5+0-0.3 = 0.2 >= 0 ✓)
    sys = sys.add(LinearInequality::make(
        {LinearTerm{x, 0.5}, LinearTerm{y, 1.0}}, -0.3, false));
    // -0.5*x - y + 0.8 >= 0  (x=1, y=0 → -0.5-0+0.8 = 0.3 >= 0 ✓)
    sys = sys.add(LinearInequality::make(
        {LinearTerm{x, -0.5}, LinearTerm{y, -1.0}}, 0.8, false));
    bool unsat = fm_is_unsat(sys);
    // x=1, y=0 is a valid integer solution, so SAT.
    // BUG: fm_is_unsat returns true due to over-tightened rounding.
    if (unsat) throw "BUG: system is SAT but FM says UNSAT";
    return true;
}

static_assert(check());

int main() { return 0; }
