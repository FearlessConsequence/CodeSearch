#pragma once
#include <filesystem>
#include <string>

namespace utf8 {
    inline std::string ToUtf8(const std::filesystem::path& p) {
        auto u8 = p.u8string();
        std::string result;
        result.reserve(u8.size());
        for (auto c : u8) result += static_cast<char>(c);
        return result;
    }

    inline size_t SafeCut(const std::string& s, size_t pos) {
        while (pos > 0 && pos < s.size() && ((unsigned char)s[pos] & 0xC0) == 0x80) pos--;
        return pos;
    }

    inline std::filesystem::path FromUtf8(const std::string& s) {
#if defined(__cpp_lib_char8_t)
        return std::filesystem::path(std::u8string(s.begin(), s.end()));
#else
        return std::filesystem::u8path(s);
#endif
    }
}