// language: C++20, file: entities.hpp, target: CS2 Source 2, Windows 11 x64
#pragma once
#include "memory.hpp"
#include "offsets.hpp"
#include <array>
#include <vector>
#include <cmath>
#include <unordered_map>

struct Vec3 {
    float x = 0, y = 0, z = 0;
    float len()    const { return sqrtf(x*x + y*y + z*z); }
    float len2d()  const { return sqrtf(x*x + y*y); }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator*(float s)       const { return {x*s,   y*s,   z*s}; }
    bool finite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
};

struct Matrix4x4 { float m[4][4]{}; };

struct BoneMat { // Source 2 position, scale, quaternion; 32 bytes.
    Vec3 position{};
    float scale = 0;
    float rotation[4]{};
    Vec3 pos() const { return position; }
};
static_assert(sizeof(BoneMat) == 32);

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
    Vec3 eye{};
    bool bones_valid = false;
    bones::Layout rig;
    Vec3 collision_min{},collision_max{};
    bool collision_valid=false;
    uint64_t spotted_by = 0;
    int controller_index = 0;
    uintptr_t crosshair_target = 0;

    uint16_t weapon_id    = 0;
    int      clip         = 0;
    int      reserve      = 0;
    bool     is_local     = false;

    Vec3 punch_angle{};
    Vec3 eye_angles{};
    int  shots_fired = 0;

    std::vector<uint16_t> util_ids; // grenade weapon IDs in inventory

    std::array<BoneMat, 128> bone{};

    uintptr_t obs_target = 0;
    char name[128]{};

    bool valid() const { return pawn && alive && health > 0 && !dormant; }
    bool bone_point(int index,Vec3& result) const {
        if (index<0 || index>=static_cast<int>(bone.size()) || !origin.finite()) return false;
        const auto& value=bone[index]; const auto delta=value.pos()-origin;
        const float height=collision_valid ? collision_max.z : 84.f;
        if (!value.pos().finite() || !std::isfinite(value.scale) || value.scale<.01f || value.scale>4.f ||
            std::fabs(delta.x)>64.f || std::fabs(delta.y)>64.f || delta.z<-12.f || delta.z>height+12.f) return false;
        result=value.pos(); return true;
    }
    bool aim_point(int choice,Vec3& result) const {
        return bones_valid && bone_point(rig.target(choice),result) && result.z-origin.z>=8.f;
    }
};

struct BombState {
    uintptr_t entity=0;
    bool    planted      = false;
    bool    ticking      = false;
    bool    defused      = false;
    float   blow_time    = 0.f;
    float   timer_length = 40.f;
    float   defuse_end   = 0.f;
    int     site         = 0; // 0=A,1=B
    Vec3    pos{};
    float remaining = 0;
    float defuse_remaining = 0;
    bool clock_valid = false;
    bool defusing=false,defuse_valid=false;
    float defuse_length=0,server_time=0;
};

struct WorldItem { Vec3 origin{}; uint16_t weapon_id = 0; };

struct GameState {
    bool in_game    = false;
    bool warmup     = false;
    int  local_team = 0;
};
struct CacheOptions { bool bones=true,inventory=true,items=true; };

class EntityCache {
public:
    bool init(Memory* mem);
    bool resolve();
    void update(CacheOptions options={});
    bool current_bomb_state(BombState&) const;

    const std::vector<PlayerEnt>& players()      const { return players_; }
    const PlayerEnt* local() const { return local_index_ < players_.size() ? &players_[local_index_] : nullptr; }
    const Matrix4x4&               view_matrix()  const { return vm_; }
    bool current_view_matrix(Matrix4x4& value) const;
    const std::string& map_name() const { return map_name_; }
    const BombState&               bomb()         const { return bomb_; }
    const std::vector<WorldItem>& items() const { return items_; }
    bool ready() const { return ready_; }
    const std::string& status() const { return status_; }
    float sensitivity() const { return sensitivity_; }
    bool current_view_angles(Vec3& value) const {
        Vec3 current{};
        if (!mem_ || !view_angles_addr || !mem_->read_into(view_angles_addr,current) || !current.finite() ||
            std::fabs(current.x)>89.5f || std::fabs(current.y)>36000.f || std::fabs(current.z)>10.f) return false;
        value=current; return true;
    }
    ULONGLONG sampled_at() const { return sampled_at_; }
    uintptr_t entity_at(uint32_t index) const;
    uintptr_t entity_handle(uint32_t handle) const;

    // Addresses resolved at startup
    uintptr_t ent_system    = 0;
    uintptr_t local_ctrl    = 0; // pointer to CCSPlayerController*
    uintptr_t vm_addr       = 0; // pointer to float[16]
    uintptr_t c4_list       = 0; // pointer to CPlantedC4*
    uintptr_t globals_addr = 0;
    uintptr_t view_angles_addr = 0;

private:
    void update_vm();
    void update_entities();
    void update_bomb();
    void read_player(PlayerEnt& p, uintptr_t ctrl, uintptr_t pawn, uintptr_t ent_list);
    void read_bones(PlayerEnt& p, uintptr_t pawn);
    void read_weapon(PlayerEnt& p, uintptr_t pawn, uintptr_t ent_list);
    void update_items();
    void update_map();

    Memory* mem_     = nullptr;
    std::vector<PlayerEnt> players_;
    size_t local_index_ = static_cast<size_t>(-1);
    Matrix4x4  vm_{};
    BombState  bomb_{};
    std::vector<WorldItem> items_;
    uintptr_t system_ = 0;
    uintptr_t client_ = 0;
    bool ready_ = false;
    float sensitivity_ = 1.f;
    ULONGLONG last_items_ = 0;
    std::string status_ = "Aguardando dados do CS2";
    std::unordered_map<uintptr_t,bones::Layout> model_rigs_;
    ULONGLONG sampled_at_ = 0;
    std::string map_name_;
    ULONGLONG next_map_read_=0,next_item_scan_=0;
    std::vector<WorldItem> pending_items_;
    int item_cursor_=65,item_last_=0;
    CacheOptions options_;
};
