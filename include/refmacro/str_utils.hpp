#ifndef REFMACRO_STR_UTILS_HPP
#define REFMACRO_STR_UTILS_HPP

#include <cstddef>

namespace refmacro {

consteval void copy_str(char* dst, const char* src, std::size_t max_len = 16) {
    std::size_t i = 0;
    for (; i < max_len - 1 && src[i] != '\0'; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

consteval bool str_eq(const char* a, const char* b) {
    for (std::size_t i = 0;; ++i) {
        if (a[i] != b[i])
            return false;
        if (a[i] == '\0')
            return true;
    }
}

consteval std::size_t str_len(const char* s) {
    std::size_t len = 0;
    while (s[len] != '\0')
        ++len;
    return len;
}

} // namespace refmacro

#endif // REFMACRO_STR_UTILS_HPP
