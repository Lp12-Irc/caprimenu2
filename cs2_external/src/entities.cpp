// language: C++20, file: entities.cpp, target: CS2 Source 2, Windows 11 x64
#include "entities.hpp"
#include <cstring>

bool EntityCache::init(Memory* mem) { mem_ = mem; return mem != nullptr; }

void EntityCache::update() {
    if (!mem_ || !mem_->is_valid()) return;
    update_vm();
    update_entities();
    update_bomb();
}

void EntityCache::update_vm() {
    if (vm_addr) vm_ = mem_->read<Matrix4x4>(vm_addr);
}

void EntityCache::update_entities() {
    players_.clear();
    local_ = nullptr;
    if (!ent_system) return;

    // Resolve current local controller pointer
    uintptr_t lctrl_val = local_ctrl ? mem_->read<uintptr_t>(local_ctrl) : 0;

    // Entity list: ent_system → +0x10 → array of chunk ptrs
    // chunk[k] covers entities [k*512 .. k*512+511]
    // slot: chunk_ptr + 0x78 * (index % 512)
    // entity ptr: slot + 0x10
    uintptr_t ent_list = mem_->read<uintptr_t>(ent_system + offsets::ENT_LIST_OFFSET);
    if (!ent_list) return;

    for (int i = 0; i < 64; ++i) {
        int chunk_idx = i / 512;
        uintptr_t chunk = mem_->read<uintptr_t>(ent_list + 0x8ULL * (chunk_idx + 1));
        if (!chunk) continue;

        uintptr_t slot  = chunk + offsets::ENT_IDENT_SIZE * (i % 512);
        uintptr_t ctrl  = mem_->read<uintptr_t>(slot + offsets::ENT_PAWN_PTR_OFFSET);
        if (!ctrl) continue;

        int team = mem_->read<int32_t>(ctrl + offsets::m_iTeamNum);
        if (team != 2 && team != 3) continue; // skip non-player entities

        bool alive = mem_->read<bool>(ctrl + offsets::m_bPawnIsAlive);

        // Resolve pawn handle → pawn pointer
        uint32_t pawn_h = mem_->read<uint32_t>(ctrl + offsets::m_hPawn);
        uintptr_t pawn  = 0;
        if (pawn_h != 0xFFFFFFFF) {
            int pi = (pawn_h >> 9) & 0x7FFF;
            uintptr_t pc = mem_->read<uintptr_t>(ent_list + 0x8ULL * (pi / 512 + 1));
            if (pc) pawn = mem_->read<uintptr_t>(pc + offsets::ENT_IDENT_SIZE * (pi % 512)
                                                    + offsets::ENT_PAWN_PTR_OFFSET);
        }

        PlayerEnt p{};
        p.ctrl     = ctrl;
        p.pawn     = pawn;
        p.team     = team;
        p.alive    = alive;
        p.is_local = (ctrl == lctrl_val);

        if (pawn && alive) read_player(p, ctrl, pawn, ent_list);

        // Player name (null-terminated on heap)
        uintptr_t nptr = mem_->read<uintptr_t>(ctrl + offsets::m_sSanitizedPlayerName);
        if (nptr) {
            auto nb = mem_->read_bytes(nptr, 127);
            if (!nb.empty()) {
                memcpy(p.name, nb.data(), nb.size());
                p.name[nb.size()] = '\0';
            }
        }

        players_.push_back(std::move(p));
    }

    // Stable pointer to local after vector is finalized
    for (auto& p : players_) {
        if (p.is_local) { local_ = &p; break; }
    }
}

void EntityCache::read_player(PlayerEnt& p, uintptr_t ctrl,
                               uintptr_t pawn, uintptr_t ent_list) {
    p.health    = mem_->read<int32_t>(pawn + offsets::m_iHealth);
    p.armor     = mem_->read<int32_t>(pawn + offsets::m_ArmorValue);
    p.helmet    = mem_->read<bool>   (pawn + offsets::m_bHasHelmet);
    p.scoped    = mem_->read<bool>   (pawn + offsets::m_bIsScoped);
    p.defusing  = mem_->read<bool>   (pawn + offsets::m_bIsDefusing);
    p.flash     = mem_->read<float>  (pawn + offsets::m_flFlashAlpha);
    p.obs_target= mem_->read<uintptr_t>(pawn + offsets::m_hObserverTarget);

    p.punch_angle = mem_->read<Vec3>(pawn + offsets::m_aimPunchAngle);
    p.eye_angles  = mem_->read<Vec3>(pawn + offsets::m_angEyeAngles);
    p.shots_fired = mem_->read<int32_t>(pawn + offsets::m_iIDShotsFired);

    // World position via game scene node
    uintptr_t sn = mem_->read<uintptr_t>(pawn + offsets::m_pGameSceneNode);
    if (sn) p.origin = mem_->read<Vec3>(sn + offsets::m_vecAbsOrigin);

    read_bones(p, pawn);

    // Head from bone 6, fallback to origin + 64 units
    if (p.bone[bones::HEAD].m[0][3] != 0.f || p.bone[bones::HEAD].m[1][3] != 0.f)
        p.head = p.bone[bones::HEAD].pos();
    else { p.head = p.origin; p.head.z += 64.f; }

    read_weapon(p, pawn, ent_list);
}

void EntityCache::read_bones(PlayerEnt& p, uintptr_t pawn) {
    uintptr_t sn = mem_->read<uintptr_t>(pawn + offsets::m_pGameSceneNode);
    if (!sn) return;
    uintptr_t bone_arr = mem_->read<uintptr_t>(sn + offsets::m_modelState
                                                   + offsets::m_pBoneArray);
    if (!bone_arr) return;

    // Single RPM for all 32 bones
    auto raw = mem_->read_bytes(bone_arr, sizeof(BoneMat) * 32);
    if (raw.size() >= sizeof(BoneMat) * 32)
        memcpy(p.bone.data(), raw.data(), sizeof(BoneMat) * 32);
}

void EntityCache::read_weapon(PlayerEnt& p, uintptr_t pawn, uintptr_t ent_list) {
    uint32_t wh = mem_->read<uint32_t>(pawn + offsets::m_pClippingWeapon);
    if (wh == 0xFFFFFFFF) return;

    int wi = (wh >> 9) & 0x7FFF;
    uintptr_t wc = mem_->read<uintptr_t>(ent_list + 0x8ULL * (wi / 512 + 1));
    if (!wc) return;
    uintptr_t went = mem_->read<uintptr_t>(wc + offsets::ENT_IDENT_SIZE * (wi % 512)
                                              + offsets::ENT_PAWN_PTR_OFFSET);
    if (!went) return;

    p.weapon_id = mem_->read<uint16_t>(went + offsets::m_AttributeManager
                                            + offsets::m_iItemDefinitionIndex);
    p.clip      = mem_->read<int32_t>(went + offsets::m_iClip1);
    p.reserve   = mem_->read<int32_t>(went + offsets::m_pReserveAmmoCount);
}

void EntityCache::update_bomb() {
    bomb_ = {};
    if (!c4_list) return;
    uintptr_t c4 = mem_->read<uintptr_t>(c4_list);
    if (!c4) return;

    bomb_.planted      = true;
    bomb_.ticking      = mem_->read<bool> (c4 + offsets::m_bBombTicking);
    bomb_.defused      = mem_->read<bool> (c4 + offsets::m_bBombDefused);
    bomb_.blow_time    = mem_->read<float>(c4 + offsets::m_flC4Blow);
    bomb_.timer_length = mem_->read<float>(c4 + offsets::m_flTimerLength);
    bomb_.defuse_end   = mem_->read<float>(c4 + offsets::m_flDefuseCountDown);
    bomb_.site         = mem_->read<int32_t>(c4 + offsets::m_nBombSite);

    uintptr_t sn = mem_->read<uintptr_t>(c4 + offsets::m_pGameSceneNode);
    if (sn) bomb_.pos = mem_->read<Vec3>(sn + offsets::m_vecAbsOrigin);
}
