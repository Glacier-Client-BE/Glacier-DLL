#pragma once
#include <cmath>

namespace Glacier::sdk {

struct Vec2 {
    float x = 0.f, y = 0.f;
    constexpr Vec2() = default;
    constexpr Vec2(float a, float b) : x(a), y(b) {}

    Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    Vec2 operator*(float s)       const { return { x * s,   y * s   }; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    float length() const { return std::sqrt(x*x + y*y); }
    float dot(const Vec2& o) const { return x*o.x + y*o.y; }
};

} // namespace Glacier::sdk
