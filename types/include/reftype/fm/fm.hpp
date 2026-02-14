#ifndef REFTYPE_FM_HPP
#define REFTYPE_FM_HPP

// FM solver umbrella include.
// solver.hpp directly includes disjunction.hpp, eliminate.hpp, and parser.hpp.
// Transitive dependency DAG:
//   solver.hpp → {disjunction.hpp, eliminate.hpp, parser.hpp}
//   disjunction.hpp → {eliminate.hpp, parser.hpp}
//   eliminate.hpp → {rounding.hpp, types.hpp}
//   rounding.hpp → types.hpp
//   parser.hpp → types.hpp
#include <reftype/fm/solver.hpp>

#endif // REFTYPE_FM_HPP
