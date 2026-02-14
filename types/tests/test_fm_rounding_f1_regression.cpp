// Regression test for F1: multi-variable integer rounding with
// non-integer coefficients was unsound (over-tightened bounds,
// causing false UNSAT).
//
// This test compiles and runs successfully now that the rounding bug
// is fixed. If the bug reappears, the static_assert below fails at
// compile time.

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
    if (unsat)
        throw "BUG: system is SAT but FM says UNSAT";
    return true;
}

static_assert(check());

int main() { return 0; }
