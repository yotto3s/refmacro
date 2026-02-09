# Phase 2: Type Environment

> **Status:** Work in progress — this plan should be refined before implementation.

**Goal:** Build a compile-time map from variable names to their types (as Expression ASTs).

**File:** `include/refmacro/types/type_env.hpp`

**Depends on:** Phase 1 (type representation)

---

## Decisions Made

- TypeEnv stores types as Expression ASTs (types are data, same as everything else)
- Template-parametric on `Cap` and `MaxBindings`
- Used by Pass 1 (annotate_types) to thread environment top-down
- **Immutable API:** `bind()` returns a new TypeEnv (clean for recursive scoping)
- **Structural type:** all members are structural → NTTP-compatible for free
- `bind()` appends (no dedup) — shadowing works via reverse-order lookup

## Data Structure

```cpp
namespace refmacro::types {

template <std::size_t Cap = 128, int MaxBindings = 16>
struct TypeEnv {
    char names[MaxBindings][16]{};
    Expression<Cap> types[MaxBindings]{};
    int count{0};

    // Append binding (supports shadowing — later bindings win)
    consteval TypeEnv bind(const char* name, Expression<Cap> type) const {
        TypeEnv result = *this;
        copy_str(name, result.names[result.count]);
        result.types[result.count] = type;
        result.count++;
        return result;
    }

    // Reverse-order search for correct shadowing
    consteval Expression<Cap> lookup(const char* name) const {
        for (int i = count - 1; i >= 0; --i)
            if (str_eq(names[i], name))
                return types[i];
        throw "type error: unbound variable";
    }

    consteval bool has(const char* name) const {
        for (int i = count - 1; i >= 0; --i)
            if (str_eq(names[i], name))
                return true;
        return false;
    }
};

} // namespace refmacro::types
```

## Design Notes

- No `remove`/`unbind` needed — immutable style means scoping is handled by passing different envs to different subtrees
- No `merge` needed — envs are threaded through traversal, not combined
- `MaxBindings=16` matches existing `VarMap` convention
- Reverse-order `lookup` gives correct shadowing semantics

## Testing Strategy

- Bind variables, look them up, verify correct types returned
- Test shadowing: bind "x" twice, lookup returns latest binding
- Test lookup failure: `lookup("unknown")` throws in consteval
- Test with type expressions of various complexity (tint, tref, tarr)
- Verify empty env has count=0, has() returns false
- Verify TypeEnv is structural (use as NTTP if needed)
