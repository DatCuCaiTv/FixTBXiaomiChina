// obf.h - ma hoa chuoi luc bien dich (XOR + rolling key), muc do vua phai
// Muc dich: chuoi trong file exe khong lo ra khi dung lenh "strings", nhung KHONG
// dung ky thuat nang (khong pack, khong anti-debug) de tranh bi antivirus bao nham.
#pragma once
#include <windows.h>
#include <stdint.h>

namespace obf {

// Khoa rolling: sinh tu mot seed co dinh + vi tri ky tu
constexpr uint8_t seedByte(size_t i, uint8_t seed) {
    return (uint8_t)((seed * 131u + i * 2654435761u) ^ (i << 3) ^ (seed >> 1));
}

template<size_t N, uint8_t SEED>
struct XS {
    char enc[N];

    constexpr XS(const char* s) : enc{} {
        for (size_t i = 0; i < N; ++i)
            enc[i] = (char)(s[i] ^ seedByte(i, SEED));
    }

    // Giai ma vao buffer cua nguoi goi (khong dung cap phat dong)
    void dec(char* out) const {
        for (size_t i = 0; i < N; ++i)
            out[i] = (char)(enc[i] ^ seedByte(i, SEED));
    }
};

} // namespace obf

// Bien dich chuoi ma hoa: XSTR("abc") tra ve const char* (buffer tinh, giai ma lan dau)
#define XSTR(str) ([]() -> const char* {                       \
        static constexpr auto _e = obf::XS<sizeof(str),         \
            (uint8_t)(__LINE__ * 7 + __COUNTER__ * 13 + 0x5A)>(str); \
        static char _b[sizeof(str)] = {0};                       \
        static bool _done = false;                               \
        if (!_done) { _e.dec(_b); _done = true; }                \
        return _b;                                               \
    }())
