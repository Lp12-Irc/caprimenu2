#pragma once
#include "entities.hpp"
#include <cmath>

namespace head_gate {
enum class Result { Unavailable, Outside, Inside };
// Conservative bone-centered region, not the server's model-specific hitbox.
inline constexpr float base_radius = 2.5f;
inline Result test(const PlayerEnt& player, const Matrix4x4& camera, float margin) {
    Vec3 head{};
    if (!std::isfinite(margin) || margin < .3f || margin > 1.f || !player.aim_point(0, head))
        return Result::Unavailable;
    for (const auto& row : camera.m) for (float value : row)
        if (!std::isfinite(value)) return Result::Unavailable;
    Vec3 axes[3];
    float lengths[3]{};
    const int rows[] = {0, 1, 3};
    for (int i = 0; i < 3; ++i) {
        const auto& row = camera.m[rows[i]];
        Vec3 axis{row[0], row[1], row[2]};
        lengths[i] = axis.len();
        if (!std::isfinite(lengths[i]) || lengths[i] < .001f) return Result::Unavailable;
        axes[i] = axis * (1.f / lengths[i]);
    }
    auto dot = [](const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; };
    if (std::fabs(dot(axes[0], axes[1])) > .01f || std::fabs(dot(axes[0], axes[2])) > .01f ||
        std::fabs(dot(axes[1], axes[2])) > .01f) return Result::Unavailable;
    auto project = [&](int row) {
        return camera.m[row][0]*head.x + camera.m[row][1]*head.y +
               camera.m[row][2]*head.z + camera.m[row][3];
    };
    const float depth = project(3) / lengths[2];
    const float horizontal = project(0) / lengths[0];
    const float vertical = project(1) / lengths[1];
    const float radius = base_radius * margin * player.bone[player.rig.target(0)].scale;
    if (!std::isfinite(depth) || !std::isfinite(horizontal) || !std::isfinite(vertical) ||
        !std::isfinite(radius)) return Result::Unavailable;
    // Normalized camera planes measure distance from the center ray in world units.
    return depth > radius && std::hypot(horizontal, vertical) <= radius ? Result::Inside : Result::Outside;
}
}
