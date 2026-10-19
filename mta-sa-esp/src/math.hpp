#pragma once
#include <array>
#include <cmath>

namespace esp {
struct Vec3 { float x{}, y{}, z{}; };
struct Vec2 { float x{}, y{}; };
using ViewMatrix = std::array<float, 16>;
inline bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
inline float distance(Vec3 a, Vec3 b) noexcept {
    return std::hypot(a.x-b.x, a.y-b.y, a.z-b.z);
}
// GTA's RenderWare matrix already includes the viewport bias. It is not an NDC matrix.
inline bool project(Vec3 world, const ViewMatrix& matrix, float width, float height,
                    float near_clip, Vec2& screen) noexcept {
    screen = {};
    if (!finite(world) || !std::isfinite(width) || !std::isfinite(height) ||
        width <= 0 || height <= 0 || !std::isfinite(near_clip) || near_clip <= 0) return false;
    const float depth = world.x*matrix[2] + world.y*matrix[6] + world.z*matrix[10] + matrix[14];
    if (!std::isfinite(depth) || depth <= near_clip) return false;
    const float x = (world.x*matrix[0] + world.y*matrix[4] + world.z*matrix[8] + matrix[12]) * width/depth;
    const float y = (world.x*matrix[1] + world.y*matrix[5] + world.z*matrix[9] + matrix[13]) * height/depth;
    if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1000000 || std::abs(y) > 1000000) return false;
    screen = {x,y};
    return true;
}
}
