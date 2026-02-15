#ifndef REFMACRO_MACRO_HPP
#define REFMACRO_MACRO_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>

namespace refmacro {

// --- NTTP string wrapper for tag literals ---

template <std::size_t N> struct FixedString {
    char data[N]{};
    consteval FixedString(const char (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = s[i];
    }
};

// --- MacroSpec: type-level macro identity ---

template <FixedString Tag, typename CompileFn> struct MacroSpec {
    static constexpr auto tag = Tag;
    static consteval auto compile_fn() { return CompileFn{}; }
};

// --- MacroCaller: callable with AST creation + macro tracking ---

template <typename Spec> struct MacroCaller {
    using spec_type = Spec;

    // Backward-compatible fields (same layout as old Macro)
    char tag[16]{};
    decltype(Spec::compile_fn()) fn{};

    consteval MacroCaller() { copy_str(tag, Spec::tag.data); }

    // Nullary
    consteval auto operator()() const {
        constexpr MacroCaller self{};
        Expression<64, self> result;
        result.id = result.ast.add_tagged_node(tag, {});
        return result;
    }

    // Unary
    template <std::size_t Cap, auto... Ms>
    consteval auto operator()(Expression<Cap, Ms...> c0) const {
        constexpr MacroCaller self{};
        Expression<Cap, self, Ms...> result;
        result.ast = c0.ast;
        result.id = result.ast.add_tagged_node(tag, {c0.id});
        return result;
    }

    // Binary
    template <std::size_t Cap, auto... Ms1, auto... Ms2>
    consteval auto operator()(Expression<Cap, Ms1...> c0,
                              Expression<Cap, Ms2...> c1) const {
        constexpr MacroCaller self{};
        Expression<Cap, self, Ms1..., Ms2...> result;
        result.ast = c0.ast;
        int off = result.ast.merge(c1.ast);
        result.id = result.ast.add_tagged_node(tag, {c0.id, c1.id + off});
        return result;
    }

    // Ternary
    template <std::size_t Cap, auto... Ms1, auto... Ms2, auto... Ms3>
    consteval auto operator()(Expression<Cap, Ms1...> c0,
                              Expression<Cap, Ms2...> c1,
                              Expression<Cap, Ms3...> c2) const {
        constexpr MacroCaller self{};
        Expression<Cap, self, Ms1..., Ms2..., Ms3...> result;
        result.ast = c0.ast;
        int off1 = result.ast.merge(c1.ast);
        int off2 = result.ast.merge(c2.ast);
        result.id = result.ast.add_tagged_node(
            tag, {c0.id, c1.id + off1, c2.id + off2});
        return result;
    }

    // Quaternary
    template <std::size_t Cap, auto... Ms1, auto... Ms2, auto... Ms3,
              auto... Ms4>
    consteval auto
    operator()(Expression<Cap, Ms1...> c0, Expression<Cap, Ms2...> c1,
               Expression<Cap, Ms3...> c2, Expression<Cap, Ms4...> c3) const {
        constexpr MacroCaller self{};
        Expression<Cap, self, Ms1..., Ms2..., Ms3..., Ms4...> result;
        result.ast = c0.ast;
        int off1 = result.ast.merge(c1.ast);
        int off2 = result.ast.merge(c2.ast);
        int off3 = result.ast.merge(c3.ast);
        result.id = result.ast.add_tagged_node(
            tag, {c0.id, c1.id + off1, c2.id + off2, c3.id + off3});
        return result;
    }
};

// --- New defmacro: tag as NTTP ---

template <FixedString Tag, typename CompileFn>
consteval auto defmacro(CompileFn) {
    return MacroCaller<MacroSpec<Tag, CompileFn>>{};
}

} // namespace refmacro

#endif // REFMACRO_MACRO_HPP
