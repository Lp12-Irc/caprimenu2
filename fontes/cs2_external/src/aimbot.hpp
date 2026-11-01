// language: C++20, file: aimbot.hpp, target: Windows 11 x64
// Standard aimbot: SendInput for mouse delta — no WPM, no handle write
// Command-level silent aim is not implemented by this class.
#pragma once
#include "entities.hpp"
#include "config.hpp"
#include "memory.hpp"
#include "input.hpp"

class Aimbot {
public:
    explicit Aimbot(InputApi input = {}) : input_(input) {}
    void update(const EntityCache& cache, const Config& cfg, Memory* mem);
    void reset() { prev_punch_={}; target_=target_ctrl_=0; target_since_=last_update_=0; mouse_x_=mouse_y_=0; awaiting_feedback_=false; }

private:
    Vec3  calc_angle(const Vec3& src, const Vec3& dst) const;
    float normalize(float a)                            const;
    bool get_bone_pos(const PlayerEnt& p,int bone,Vec3& point) const;
    bool send_mouse(float dpitch,float dyaw,float sens);
    Vec3 prev_punch_{};
    uintptr_t target_ = 0;
    uintptr_t target_ctrl_ = 0;
    ULONGLONG target_since_ = 0;
    ULONGLONG last_update_ = 0;
    float mouse_x_ = 0, mouse_y_ = 0;
    InputApi input_;
    bool awaiting_feedback_=false,last_projected_=false;
    Vec3 last_command_view_{};
    Matrix4x4 last_command_camera_{};
};
