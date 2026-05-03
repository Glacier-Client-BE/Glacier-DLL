#pragma once

namespace Glacier::sdk {

struct Vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
    constexpr Vec4() = default;
    constexpr Vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
};

} // namespace Glacier::sdk
