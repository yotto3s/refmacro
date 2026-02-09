# Phase 8: Error Reporting

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Provide meaningful, structured compile-time diagnostics when type checking fails.

**Files:** Integrated into type checking code (check.hpp, subtype.hpp)

**Depends on:** Phase 7 (full pipeline working)

---

## Decisions Made

- Structured error messages with context: constraint, types involved, source expression
- Build messages with `FixedString<>` (existing infrastructure)
- Use GCC's consteval `throw` string display
- Constraint `origin` field (from Phase 3) provides context

## Error Message Format

```
type error: <category>
  expected: <expected_type>
  actual:   <actual_type>
  at:       <source_context>
  because:  <constraint_explanation>
```

Example:
```
type error: subtype check failed
  expected: {#v : Int | #v > 0}
  actual:   Int
  at:       annotation on (x + 1)
  because:  cannot prove x + 1 > 0 from context
```

## Error Categories

| Category | When | Message |
|----------|------|---------|
| Subtype failure | `ann(e, T)` where synth(e) </: T | "expected T, actual S" |
| Type mismatch | e.g., Bool where Numeric expected | "expected Numeric, got Bool" |
| Unresolvable constraint | FM can't prove implication | "cannot prove P from context" |
| Non-linear formula | `var * var` in refinement predicate | "non-linear term in refinement" |
| Unbound variable | Variable not in TypeEnv | "unbound variable: x" |
| Incompatible join | MCond branches with incompatible types | "cannot join T1 and T2" |

## Implementation

```cpp
namespace refmacro::types {

template <int N = 512>
consteval void report_error(const char* category,
                            const char* expected,
                            const char* actual,
                            const char* context) {
    FixedString<N> msg{};
    msg.append("type error: ");
    msg.append(category);
    msg.append("\n  expected: ");
    msg.append(expected);
    msg.append("\n  actual:   ");
    msg.append(actual);
    msg.append("\n  at:       ");
    msg.append(context);
    throw msg.data();
}

// Use pretty_print to render types in error messages
template <std::size_t Cap>
consteval void assert_subtype(Expression<Cap> sub, Expression<Cap> super,
                              const char* context) {
    if (!is_subtype(sub, super)) {
        constexpr auto sub_str = pretty_print(sub);
        constexpr auto super_str = pretty_print(super);
        report_error("subtype check failed",
                     super_str.data(), sub_str.data(), context);
    }
}

} // namespace refmacro::types
```

## Integration with Constraint Origin

The `Constraint::origin` field (Phase 3) provides the `context` for error messages:

```cpp
for (int i = 0; i < result.constraints.count; ++i) {
    auto& c = result.constraints.constraints[i];
    if (!fm_is_valid(c.formula)) {
        report_error("constraint unsatisfied",
                     "valid", "unprovable",
                     c.origin);
    }
}
```

## GCC Error Display

GCC shows `throw` strings in consteval error messages like:
```
error: expression '<throw-expression>' is not a constant expression
note: 'throw "type error: subtype check failed\n  expected: ..."' called
```

The full string is visible in the note. `FixedString<512>` should be enough for most messages.

## Testing Strategy

- Well-typed programs compile without errors
- Ill-typed programs: verify `result.valid == false` via static_assert
- Error message content: manual verification (compile and read GCC output)
- Each error category produces a distinct, readable message
- Nested annotations show correct source context
- Constraint origin strings are meaningful
