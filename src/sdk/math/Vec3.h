#pragma once
#include <cmath>

namespace Glacier::sdk {

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
    constexpr Vec3() = default;
    constexpr Vec3(float a, float b, float c) : x(a), y(b), z(c) {}

    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s)       const { return { x * s,   y * s,   z * s   }; }
    Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
    float length()   const { return std::sqrt(x*x + y*y + z*z); }
    float lengthSq() const { return x*x + y*y + z*z; }
    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    Vec3  cross(const Vec3& o) const {
        return { y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x };
    }
    Vec3 normalized() const {
        float l = length();
        return l > 1e-6f ? Vec3{ x/l, y/l, z/l } : Vec3{};
    }
};

// 3D integer block position (used for the player's targeted block, etc.)
struct BlockPos {
    int x = 0, y = 0, z = 0;
};

} // namespace Glacier::sdk
