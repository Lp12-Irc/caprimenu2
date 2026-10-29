// language: C++20, file: aimbot.hpp, target: Windows 11 x64
// Standard aimbot: SendInput for mouse delta — no WPM, no handle write
// Silent aim: WPM to CUserCmd viewangles — requires write handle (detection risk noted)
#pragma once
#include "entities.hpp"
#include "config.hpp"
#include "memory.hpp"

class Aimbot {
public:
    void update(const EntityCache& cache, const Config& cfg, Memory* mem);

private:
    Vec3  calc_angle(const Vec3& src, const Vec3& dst) const;
    float normalize(float a)                            const;
    Vec3  get_bone_pos(const PlayerEnt& p, int bone)   const;
    void  send_mouse(float dpitch, float dyaw, float sens);
    void  do_silent_aim(const PlayerEnt& target, int bone,
                        const PlayerEnt& local, Memory* mem);
    void  do_rcs(const Vec3& punch, float intensity, float sens,
                 Memory* mem, uintptr_t pawn);

    float prev_shots_ = 0;
};
