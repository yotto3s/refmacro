#ifndef REFMACRO_PRETTY_PRINT_HPP
#define REFMACRO_PRETTY_PRINT_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>

namespace refmacro {

template <std::size_t N = 256>
struct FixedString {
    char data[N]{};
    std::size_t len{0};

    consteval FixedString() = default;
    consteval FixedString(const char* s) {
        while (s[len] != '\0' && len < N - 1) { data[len] = s[len]; ++len; }
    }

    consteval void append(const char* s) {
        for (std::size_t i = 0; s[i] != '\0' && len < N - 1; ++i)
            data[len++] = s[i];
    }
    consteval void append(const FixedString& o) {
        for (std::size_t i = 0; i < o.len && len < N - 1; ++i)
            data[len++] = o.data[i];
    }
    consteval void append_char(char c) { if (len < N - 1) data[len++] = c; }

    consteval void append_int(int v) {
        if (v < 0) { append_char('-'); v = -v; }
        if (v == 0) { append_char('0'); return; }
        char buf[20]{};
        int pos = 0;
        while (v > 0) { buf[pos++] = '0' + (v % 10); v /= 10; }
        for (int i = pos - 1; i >= 0; --i) append_char(buf[i]);
    }

    consteval void append_double(double v) {
        if (v < 0) { append_char('-'); v = -v; }
        int integer_part = static_cast<int>(v);
        double frac = v - integer_part;
        append_int(integer_part);
        if (frac > 0.0001) {
            append_char('.');
            for (int i = 0; i < 6 && frac > 0.0001; ++i) {
                frac *= 10.0;
                int digit = static_cast<int>(frac);
                append_char('0' + digit);
                frac -= digit;
            }
        }
    }

    consteval bool operator==(const char* s) const {
        for (std::size_t i = 0; i < len; ++i)
            if (data[i] != s[i]) return false;
        return s[len] == '\0';
    }
};

namespace detail {

consteval bool is_infix(const char* tag) {
    return str_eq(tag, "add") || str_eq(tag, "sub")
        || str_eq(tag, "mul") || str_eq(tag, "div");
}

consteval const char* infix_sym(const char* tag) {
    if (str_eq(tag, "add")) return " + ";
    if (str_eq(tag, "sub")) return " - ";
    if (str_eq(tag, "mul")) return " * ";
    if (str_eq(tag, "div")) return " / ";
    return " ? ";
}

template <std::size_t Cap>
consteval FixedString<256> pp_node(const AST<Cap>& ast, int id) {
    auto n = ast.nodes[id];
    FixedString<256> s;

    if (str_eq(n.tag, "lit")) { s.append_double(n.payload); return s; }
    if (str_eq(n.tag, "var")) { s.append(n.name); return s; }

    if (str_eq(n.tag, "neg") && n.child_count == 1) {
        s.append("(-");
        s.append(pp_node(ast, n.children[0]));
        s.append_char(')');
        return s;
    }

    if (is_infix(n.tag) && n.child_count == 2) {
        s.append_char('(');
        s.append(pp_node(ast, n.children[0]));
        s.append(infix_sym(n.tag));
        s.append(pp_node(ast, n.children[1]));
        s.append_char(')');
        return s;
    }

    // Generic: tag(child, child, ...)
    s.append(n.tag);
    s.append_char('(');
    for (int i = 0; i < n.child_count; ++i) {
        if (i > 0) s.append(", ");
        s.append(pp_node(ast, n.children[i]));
    }
    s.append_char(')');
    return s;
}

} // namespace detail

template <std::size_t Cap = 64>
consteval FixedString<256> pretty_print(const Expr<Cap>& e) {
    return detail::pp_node(e.ast, e.id);
}

} // namespace refmacro

#endif // REFMACRO_PRETTY_PRINT_HPP
