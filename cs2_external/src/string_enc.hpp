// language: C++20, file: string_enc.hpp, target: Windows 11 x64, MSVC
// compile-time XOR — all sensitive strings live encrypted in .rdata, decrypt on use
#pragma once
#include <array>
#include <string>
#include <cstdint>

namespace enc {

constexpr uint8_t KEY = 0x4B; // change per build — rotate each release

template<size_t N>
struct xor_str {
    std::array<char, N> data{};

    consteval xor_str(const char (&s)[N]) {
        for (size_t i = 0; i < N; ++i)
            data[i] = s[i] ^ static_cast<char>(KEY);
    }

    [[nodiscard]] std::string decrypt() const {
        std::string r(N - 1, '\0');
        for (size_t i = 0; i < N - 1; ++i)
            r[i] = data[i] ^ static_cast<char>(KEY);
        return r;
    }
};

} // namespace enc

#define ENC(s) (::enc::xor_str<sizeof(s)>(s))
// Usage: auto name = ENC("cs2.exe").decrypt();
// In .rdata: only the XOR'd bytes appear — no plaintext
