#pragma once
#include <cstdint>
#include <algorithm>

namespace Glacier::sdk {

// Float RGBA in 0..1.  Constructible from a packed 0xAARRGGBB.
struct Color {
    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
    constexpr Color() = default;
    constexpr Color(float R, float G, float B, float A = 1.f) : r(R), g(G), b(B), a(A) {}

    static constexpr Color fromARGB(std::uint32_t v) {
        return {
            ((v >> 16) & 0xFF) / 255.f,
            ((v >>  8) & 0xFF) / 255.f,
            ( v        & 0xFF) / 255.f,
            ((v >> 24) & 0xFF) / 255.f
        };
    }

    constexpr std::uint32_t toARGB() const {
        auto c = [](float f) { return static_cast<std::uint32_t>(std::clamp(f, 0.f, 1.f) * 255.f + 0.5f); };
        return (c(a) << 24) | (c(r) << 16) | (c(g) << 8) | c(b);
    }
    // ImU32 expects 0xAABBGGRR
    constexpr std::uint32_t toImU32() const {
        auto c = [](float f) { return static_cast<std::uint32_t>(std::clamp(f, 0.f, 1.f) * 255.f + 0.5f); };
        return (c(a) << 24) | (c(b) << 16) | (c(g) << 8) | c(r);
    }
};

} // namespace Glacier::sdk
