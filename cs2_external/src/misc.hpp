// language: C++20, file: misc.hpp, target: Windows 11 x64
#pragma once
#include "entities.hpp"
#include "config.hpp"
#include "memory.hpp"

class Misc {
public:
    void update(const EntityCache& cache, const Config& cfg, Memory* mem);
    // Triggerbot state machine (delay needs wall clock)
    void tick_triggerbot(const EntityCache& cache, const Config& cfg);

private:
    bool enemy_under_crosshair(const EntityCache& cache, int sw, int sh) const;
    void apply_no_flash(const EntityCache& cache, const Config& cfg, Memory* mem);

    DWORD64 tb_fire_time_  = 0;
    bool    tb_fired_      = false;
};
