# Documentation Design for feature/refinement-types

> **Date:** 2026-02-15
> **Status:** Approved

## Goal

Create user-facing documentation for the refinement type system implemented on the `feature/refinement-types` branch.

## Approach: Layered README Pair

### 1. Root README.md

Add a "Refinement Types" section after the existing "Features" list:
- One-line description
- Short code snippet (ann + pos_int + typed_full_compile)
- Bullet list of capabilities
- Link to `types/README.md`
- Add `reftype` entry to Architecture table

### 2. types/README.md (new file)

| Section | Content |
|---------|---------|
| Header | One-line description + Lisp philosophy framing |
| Quick Start | 3 examples: type annotation, refinement type, compile-time error |
| Type Constructors | API table: tint, tbool, treal, tref, tarr, ann, pos_int |
| Pipeline | type_check / strip_types / compile stages; typed_compile, typed_full_compile |
| Extensibility | def_typerule(tag, fn) with example |
| Subtype Checking | Base widening, refinement implication, arrow contravariance |
| Architecture | Pipeline diagram, header table, FM solver overview |
| Building | CMake instructions |

## Non-Goals

- PR description (handled separately)
- Internal implementation docs (already exist in docs/plans/)
- Full FM solver theory writeup
