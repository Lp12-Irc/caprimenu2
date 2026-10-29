// language: C++20, file: esp.hpp, target: Windows 11 x64
// All rendering via ImGui background draw list — no separate D2D
#pragma once
#include "entities.hpp"
#include "config.hpp"
struct ImDrawList;

struct Scr { float x, y; bool vis; };

class Esp {
public:
    Scr w2s(const Vec3& world, const Matrix4x4& vm, int sw, int sh) const;

    void render(const EntityCache& cache, const Config& cfg,
                ImDrawList* dl, int sw, int sh);

private:
    void draw_box       (ImDrawList*, const Scr& head, const Scr& feet, ImU32 col, float thick, int style);
    void draw_skeleton  (ImDrawList*, const PlayerEnt&, const Matrix4x4&, int sw, int sh, ImU32 col, float thick);
    void draw_healthbar (ImDrawList*, const Scr& head, const Scr& feet, int hp, bool num);
    void draw_snapline  (ImDrawList*, const Scr& feet, int sw, int sh, ImU32 col);
    void draw_text_info (ImDrawList*, const PlayerEnt&, const Scr& head, const Scr& feet, int sw);
    void draw_radar     (ImDrawList*, const EntityCache&, const Config&, int sw, int sh);
    void draw_crosshair (ImDrawList*, const Config&, int sw, int sh);
    void draw_fov_circle(ImDrawList*, float fov, int sw, int sh);
    void draw_bomb_timer(ImDrawList*, const BombState&, const Config&, int sw, int sh);
    void draw_spec_list (ImDrawList*, const EntityCache&, int sw, int sh);
};
