from pathlib import Path
p=Path(__file__).parent.parent/'fontes/cs2_external/src/entities.cpp'
s=p.read_text(encoding='utf-8')
start=s.index('void EntityCache::update_entities()'); end=s.index('void EntityCache::read_bones(',start)
s=s[:start]+r'''void EntityCache::update_entities() {
    players_.clear(); local_index_=static_cast<size_t>(-1);
    if (!system_) return;
    const auto current=mem_->read<uintptr_t>(local_ctrl);
    for (int i=1;i<=64;++i) {
        const auto ctrl=entity_at(i);
        if (!ctrl) continue;
        const int team=mem_->read<uint8_t>(ctrl+offsets::m_iTeamNum);
        if (team!=2 && team!=3) continue;
        std::array<uint8_t,ControllerBytes> block{};
        if (!mem_->read_buffer(ctrl,block.data(),block.size())) continue;
        const auto alive=field<uint8_t>(block,offsets::m_bPawnIsAlive);
        if (alive>1) continue;
        PlayerEnt player{}; player.ctrl=ctrl; player.team=team; player.controller_index=i;
        player.is_local=ctrl==current; player.alive=alive==1;
        player.helmet=field<uint8_t>(block,offsets::m_bPawnHasHelmet)==1;
        const auto pawn_handle=field<uint32_t>(block,offsets::m_hPlayerPawn);
        player.pawn=entity_handle(pawn_handle);
        if (!player.alive) {
            const auto observer=entity_handle(field<uint32_t>(block,offsets::m_hObserverPawn));
            if (observer) player.pawn=observer;
        }
        if (player.pawn) read_player(player,ctrl,player.pawn,system_);
        if (player.alive && mem_->read<uint32_t>(ctrl+offsets::m_hPlayerPawn)!=pawn_handle) {
            player.health=0; player.bones_valid=false;
        }
        const auto name_ptr=field<uintptr_t>(block,offsets::m_sSanitizedPlayerName);
        if (name_ptr) {
            const auto bytes=mem_->read_bytes(name_ptr,sizeof(player.name)-1);
            if (!bytes.empty()) std::memcpy(player.name,bytes.data(),bytes.size());
        }
        players_.push_back(std::move(player));
    }
    for (size_t i=0;i<players_.size();++i) if (players_[i].is_local) { local_index_=i; break; }
}
void EntityCache::read_player(PlayerEnt& p,uintptr_t,uintptr_t pawn,uintptr_t) {
    if (!p.alive) {
        const auto services=mem_->read<uintptr_t>(pawn+offsets::m_pObserverServices);
        if (services) p.obs_target=entity_handle(mem_->read<uint32_t>(services+offsets::m_hObserverTarget));
        return;
    }
    std::array<uint8_t,PawnBytes> block{};
    if (!mem_->read_buffer(pawn,block.data(),block.size()) || field<uint8_t>(block,offsets::m_iTeamNum)!=p.team) return;
    p.health=field<int>(block,offsets::m_iHealth);
    if (p.health<1 || p.health>1000) { p.health=0; return; }
    p.armor=field<int>(block,offsets::m_ArmorValue);
    p.scoped=field<uint8_t>(block,offsets::m_bIsScoped)==1;
    p.defusing=field<uint8_t>(block,offsets::m_bIsDefusing)==1;
    p.flash=field<float>(block,offsets::m_flFlashOverlayAlpha);
    const auto collision=field<uintptr_t>(block,offsets::m_pCollision);
    std::array<uint8_t,offsets::m_vecMaxs+sizeof(Vec3)-offsets::m_vecMins> collision_block{};
    if (collision && mem_->read_buffer(collision+offsets::m_vecMins,collision_block.data(),collision_block.size())) {
        p.collision_min=field<Vec3>(collision_block,0); p.collision_max=field<Vec3>(collision_block,offsets::m_vecMaxs-offsets::m_vecMins);
        const auto extent=p.collision_max-p.collision_min;
        p.collision_valid=p.collision_min.finite() && p.collision_max.finite() && extent.x>=8 && extent.x<=128 &&
            extent.y>=8 && extent.y<=128 && extent.z>=16 && extent.z<=128;
    }
    const auto punch=field<uintptr_t>(block,offsets::m_pAimPunchServices);
    if (punch) p.punch_angle=mem_->read<Vec3>(punch+offsets::m_predictableBaseAngle)+mem_->read<Vec3>(punch+offsets::m_unpredictableBaseAngle);
    p.eye_angles=p.is_local ? mem_->read<Vec3>(view_angles_addr) : field<Vec3>(block,offsets::m_angEyeAngles);
    p.shots_fired=field<int>(block,offsets::m_iShotsFired);
    if (p.is_local) {
        const int index=field<int>(block,offsets::m_iIDEntIndex);
        if (index>0 && index<0x7FFF) p.crosshair_target=entity_at(static_cast<uint32_t>(index));
    }
    p.spotted_by=field<uint64_t>(block,offsets::m_entitySpottedState+offsets::m_bSpottedByMask);
    const auto scene=field<uintptr_t>(block,offsets::m_pGameSceneNode);
    std::array<uint8_t,offsets::m_bDormant+1-offsets::m_vecAbsOrigin> scene_block{};
    if (!scene || !mem_->read_buffer(scene+offsets::m_vecAbsOrigin,scene_block.data(),scene_block.size())) { p.health=0; return; }
    p.origin=field<Vec3>(scene_block,0); p.dormant=field<uint8_t>(scene_block,offsets::m_bDormant-offsets::m_vecAbsOrigin)!=0;
    if (!p.origin.finite()) { p.health=0; return; }
    if (options_.bones) read_bones(p,pawn);
    p.head=p.bones_valid ? p.bone[p.rig.target(0)].pos() : p.origin+Vec3{0,0,64};
    const auto view=field<Vec3>(block,offsets::m_vecViewOffset);
    p.eye=view.finite() && std::fabs(view.x)<=16 && std::fabs(view.y)<=16 && view.z>0 && view.z<100 ? p.origin+view : p.head;
    read_weapon(p,pawn,system_);
}
''' +s[end:]
p.write_text(s,encoding='utf-8')
