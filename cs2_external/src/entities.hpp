// language: C++20, file: entities.hpp, target: CS2 Source 2, Windows 11 x64
#pragma once
#include "memory.hpp"
#include "offsets.hpp"
#include <array>
#include <vector>
#include <cmath>

struct Vec3 {
    float x = 0, y = 0, z = 0;
    float len()    const { return sqrtf(x*x + y*y + z*z); }
    float len2d()  const { return sqrtf(x*x + y*y); }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator*(float s)       const { return {x*s,   y*s,   z*s}; }
};

struct Matrix4x4 { float m[4][4]{}; };

struct BoneMat { // 3x4 row-major — position at [i][3]
    float m[3][4]{};
    Vec3 pos() const { return {m[0][3], m[1][3], m[2][3]}; }
};

struct PlayerEnt {
    uintptr_t ctrl   = 0;
    uintptr_t pawn   = 0;

    int   team       = 0;
    bool  alive      = false;
    bool  dormant    = false;
    int   health     = 0;
    int   armor      = 0;
    bool  helmet     = false;
    bool  scoped     = false;
    bool  defusing   = false;
    float flash      = 0.f;

    Vec3 origin{};
    Vec3 head{};

    uint16_t weapon_id    = 0;
    int      clip         = 0;
    int      reserve      = 0;
    bool     is_local     = false;

    Vec3 punch_angle{};
    Vec3 eye_angles{};
    int  shots_fired = 0;

    std::vector<uint16_t> util_ids; // grenade weapon IDs in inventory

    std::array<BoneMat, 32> bone{};

    uintptr_t obs_target = 0;
    char name[128]{};

    bool valid() const { return pawn && alive && health > 0 && !dormant; }
};

struct BombState {
    bool    planted      = false;
    bool    ticking      = false;
    bool    defused      = false;
    float   blow_time    = 0.f;
    float   timer_length = 40.f;
    float   defuse_end   = 0.f;
    int     site         = 0; // 0=A,1=B
    Vec3    pos{};
};

struct GameState {
    bool in_game    = false;
    bool warmup     = false;
    int  local_team = 0;
};

class EntityCache {
public:
    bool init(Memory* mem);
    void update(); // 64Hz loop

    const std::vector<PlayerEnt>& players()      const { return players_; }
    const PlayerEnt*               local()        const { return local_; }
    const Matrix4x4&               view_matrix()  const { return vm_; }
    const BombState&               bomb()         const { return bomb_; }

    // Addresses resolved at startup
    uintptr_t ent_system    = 0;
    uintptr_t local_ctrl    = 0; // pointer to CCSPlayerController*
    uintptr_t vm_addr       = 0; // pointer to float[16]
    uintptr_t c4_list       = 0; // pointer to CPlantedC4*

private:
    void update_vm();
    void update_entities();
    void update_bomb();
    void read_player(PlayerEnt& p, uintptr_t ctrl, uintptr_t pawn, uintptr_t ent_list);
    void read_bones(PlayerEnt& p, uintptr_t pawn);
    void read_weapon(PlayerEnt& p, uintptr_t pawn, uintptr_t ent_list);

    Memory* mem_     = nullptr;
    std::vector<PlayerEnt> players_;
    PlayerEnt* local_ = nullptr;
    Matrix4x4  vm_{};
    BombState  bomb_{};
};
