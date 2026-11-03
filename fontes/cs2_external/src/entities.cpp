#include "entities.hpp"
#include "map_collision.hpp"
#include "input.hpp"
#include <algorithm>
#include <cstring>

namespace {
template<class T,size_t N> T field(const std::array<uint8_t,N>& bytes,size_t offset) {
    T value{}; if (offset+sizeof(T)<=N) std::memcpy(&value,bytes.data()+offset,sizeof(T)); return value;
}
constexpr size_t ControllerBytes=(std::max)({size_t(offsets::m_bPawnHasHelmet+1),size_t(offsets::m_hPlayerPawn+4),
    size_t(offsets::m_hObserverPawn+4),size_t(offsets::m_sSanitizedPlayerName+sizeof(uintptr_t)),size_t(offsets::m_bPawnIsAlive+1)});
// The distant eye-angle/crosshair fields are optional, not part of every pawn read.
constexpr size_t PawnBytes=(std::max)({size_t(offsets::m_ArmorValue+sizeof(int)),
    size_t(offsets::m_pWeaponServices+sizeof(uintptr_t)),size_t(offsets::m_vecViewOffset+sizeof(Vec3)),
    size_t(offsets::m_pAimPunchServices+sizeof(uintptr_t)),size_t(offsets::m_entitySpottedState+offsets::m_bSpottedByMask+sizeof(uint64_t))});
}

bool EntityCache::init(Memory* mem) { mem_ = mem; return mem != nullptr; }
bool EntityCache::resolve() {
    if (!mem_ || !mem_->is_valid()) return false;
    client_ = mem_->get_module_base("client.dll");
    const auto engine = mem_->get_module_base("engine2.dll");
    if (!client_ || !engine) { status_ = "Aguardando modulos do CS2"; return false; }
    const auto build = mem_->read<uint32_t>(engine + game_offsets::dwBuildNumber);
    if (build != offsets::BUILD_NUMBER) {
        status_ = "Build incompativel: " + std::to_string(build) + "; esperado " + std::to_string(offsets::BUILD_NUMBER);
        return false;
    }
    const auto pe = mem_->read<uint32_t>(client_ + 0x3C);
    if (!pe || mem_->read<uint32_t>(client_ + pe + 8) != offsets::CLIENT_TIMESTAMP ||
        mem_->read<uint32_t>(client_ + pe + 0x50) != offsets::CLIENT_IMAGE_SIZE) {
        status_ = "client.dll incompativel com os offsets extraidos";
        return false;
    }
    ent_system = client_ + game_offsets::dwEntityList;
    local_ctrl = client_ + game_offsets::dwLocalPlayerController;
    vm_addr = client_ + game_offsets::dwViewMatrix;
    c4_list = client_ + game_offsets::dwPlantedC4;
    globals_addr = client_ + game_offsets::dwGlobalVars;
    view_angles_addr = client_ + game_offsets::dwViewAngles;
    return true;
}
uintptr_t EntityCache::entity_at(uint32_t index) const {
    if (!system_ || index > 0x7FFE) return 0;
    const auto chunk = (index>>9)==0 ? controller_chunk_ : mem_->read<uintptr_t>(system_ + offsets::ENT_LIST_OFFSET + 8ULL * (index >> 9));
    return chunk ? mem_->read<uintptr_t>(chunk + offsets::ENT_IDENT_SIZE * (index & 0x1FF)) : 0;
}
uintptr_t EntityCache::entity_handle(uint32_t handle) const {
    if (!handle || handle == UINT32_MAX) return 0;
    return entity_at(handle & 0x7FFF);
}
void EntityCache::update(CacheOptions options) {
    options_=options;
    ready_ = false;
    sampled_at_=input_clock_ms();
    if (!mem_ || !mem_->is_valid()) { status_ = "CS2 desconectado"; return; }
    if (!ent_system && !resolve()) return;
    system_ = mem_->read<uintptr_t>(ent_system);
    controller_chunk_=system_ ? mem_->read<uintptr_t>(system_+offsets::ENT_LIST_OFFSET) : 0;
    if (options_.items) update_items();
    else { items_.clear(); pending_items_.clear(); item_last_=0; }
    update_entities();
    if(options_.bomb)update_bomb();else bomb_={};
    update_vm();update_map();
    if (!system_) status_ = "Lista de entidades indisponivel";
    else if (!local()) status_ = "Aguardando jogador local / partida";
    else { ready_ = true; status_ = "CS2 build " + std::to_string(offsets::BUILD_NUMBER) + " | Jogadores: " + std::to_string(players_.size()); }
    const auto sens = mem_->read<uintptr_t>(client_ + game_offsets::dwSensitivity);
    const float value = mem_->read<float>(sens + game_offsets::dwSensitivity_sensitivity);
    if (std::isfinite(value) && value >= .01f && value <= 20.f) sensitivity_ = value;
}
void EntityCache::update_vm() {
    Matrix4x4 value{};
    if (current_view_matrix(value)) vm_ = value;
    else vm_ = {};
}
bool EntityCache::current_view_matrix(Matrix4x4& value) const {
    Matrix4x4 current{},verify{};
    if (!mem_ || !vm_addr) return false;
    bool stable=false;
    for (int attempt=0;attempt<2 && !stable;++attempt) {
        if (!mem_->read_into(vm_addr,current) || !mem_->read_into(vm_addr,verify)) return false;
        stable=std::memcmp(&current,&verify,sizeof(current))==0;
    }
    if (!stable) return false;
    for (const auto& row:current.m) for (const auto number:row) if (!std::isfinite(number)) return false;
    const auto& row=current.m[3];
    if (std::fabs(row[0])+std::fabs(row[1])+std::fabs(row[2])+std::fabs(row[3])<.001f) return false;
    value=current; return true;
}
void EntityCache::update_map() {
    if (GetTickCount64()<next_map_read_) return;
    next_map_read_=GetTickCount64()+500;
    const auto globals=mem_->read<uintptr_t>(globals_addr);
    const auto name_ptr=globals ? mem_->read<uintptr_t>(globals+offsets::GLOBAL_MAP_NAME) : 0;
    const auto bytes=name_ptr ? mem_->read_bytes(name_ptr,96) : std::vector<uint8_t>{};
    const auto end=std::find(bytes.begin(),bytes.end(),uint8_t{0});
    const std::string name(bytes.begin(),end);
    if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos) { map_name_.clear();collision_.reset(); return; }
    if (map_name_!=name) { items_.clear(); pending_items_.clear(); item_last_=0; next_item_scan_=0; }
    if(map_name_!=name)collision_=MapCollision::load(std::filesystem::path("assets/collision")/(name+".bvh"));
    map_name_=name;
}
bool EntityCache::clear_path(const Vec3& from,const Vec3& to) const {return collision_ && collision_->clear(from,to);}
void EntityCache::update_entities() {
    players_.clear(); local_index_=static_cast<size_t>(-1);
    if (!system_) return;
    const auto current=mem_->read<uintptr_t>(local_ctrl);
    std::array<uint8_t,64*offsets::ENT_IDENT_SIZE> controllers{};
    const bool controller_block=controller_chunk_ && mem_->read_buffer(controller_chunk_+offsets::ENT_IDENT_SIZE,controllers.data(),controllers.size());
    for (int i=1;i<=64;++i) {
        const auto ctrl=controller_block ? field<uintptr_t>(controllers,(i-1)*offsets::ENT_IDENT_SIZE) : entity_at(i);
        if (!ctrl) continue;
        const int team=mem_->read<uint8_t>(ctrl+offsets::m_iTeamNum);
        if (team!=2 && team!=3) continue;
        std::array<uint8_t,ControllerBytes> block{};
        if (!mem_->read_buffer(ctrl,block.data(),block.size())) continue;
        const auto alive=field<uint8_t>(block,offsets::m_bPawnIsAlive);
        if (alive>1) continue;
        PlayerEnt player{}; player.ctrl=ctrl; player.team=team; player.controller_index=i;
        player.is_local=ctrl==current; player.alive=alive==1;player.state_valid=!player.alive;
        player.helmet=field<uint8_t>(block,offsets::m_bPawnHasHelmet)==1;
        const auto pawn_handle=field<uint32_t>(block,offsets::m_hPlayerPawn);
        player.pawn_handle=pawn_handle;
        player.pawn=entity_handle(pawn_handle);
        if (!player.alive) {
            const auto observer=entity_handle(field<uint32_t>(block,offsets::m_hObserverPawn));
            if (observer) player.pawn=observer;
        }
        auto& detail=details_[i];
        if(detail.ctrl!=ctrl || detail.pawn!=player.pawn) {detail={};detail.ctrl=ctrl;detail.pawn=player.pawn;}
        if (player.pawn) read_player(player,ctrl,player.pawn,system_);
        if (player.alive && mem_->read<uint32_t>(ctrl+offsets::m_hPlayerPawn)!=pawn_handle) {
            player.health=0; player.bones_valid=false;player.state_valid=true;
        }
        const auto name_ptr=field<uintptr_t>(block,offsets::m_sSanitizedPlayerName);
        if(options_.names && (!options_.throttle_details || name_ptr!=detail.name_ptr || GetTickCount64()>=detail.next_name)) {
            detail.name={};detail.name_ptr=name_ptr;detail.next_name=GetTickCount64()+500+(i%16)*10;
            const auto bytes=name_ptr ? mem_->read_bytes(name_ptr,sizeof(player.name)-1) : std::vector<uint8_t>{};
            if(!bytes.empty())std::memcpy(detail.name.data(),bytes.data(),bytes.size());
        }
        if(options_.names)std::memcpy(player.name,detail.name.data(),sizeof(player.name));
        players_.push_back(std::move(player));
    }
    for (size_t i=0;i<players_.size();++i) if (players_[i].is_local) { local_index_=i; break; }
}
void EntityCache::read_player(PlayerEnt& p,uintptr_t,uintptr_t pawn,uintptr_t) {
    if (!p.alive) {
        if(!p.is_local && !options_.observers)return;
        const auto services=mem_->read<uintptr_t>(pawn+offsets::m_pObserverServices);
        if (services) p.obs_target=entity_handle(mem_->read<uint32_t>(services+offsets::m_hObserverTarget));
        return;
    }
    std::array<uint8_t,PawnBytes> block{};
    if (!mem_->read_buffer(pawn,block.data(),block.size()) || field<uint8_t>(block,offsets::m_iTeamNum)!=p.team) return;
    p.health=field<int>(block,offsets::m_iHealth);
    p.state_valid=p.health>=0 && p.health<=1000;
    if (p.health<1 || p.health>1000) { p.health=0; return; }
    p.velocity=field<Vec3>(block,offsets::m_vecAbsVelocity);
    p.velocity_valid=p.velocity.finite() && p.velocity.len()<=4096.f;
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
    if (p.is_local && options_.recoil && punch) p.punch_angle=mem_->read<Vec3>(punch+offsets::m_predictableBaseAngle)+mem_->read<Vec3>(punch+offsets::m_unpredictableBaseAngle);
    if(p.is_local)p.eye_angles=mem_->read<Vec3>(view_angles_addr);
    else if(options_.angles)p.eye_angles=mem_->read<Vec3>(pawn+offsets::m_angEyeAngles);
    p.shots_fired=field<int>(block,offsets::m_iShotsFired);
    if (p.is_local && options_.crosshair) {
        const auto index=mem_->read<uint32_t>(pawn+offsets::m_iIDEntIndex);
        if(index && index!=UINT32_MAX)p.crosshair_target=entity_at(index&0x7fff);
    }
    p.spotted_by=field<uint64_t>(block,offsets::m_entitySpottedState+offsets::m_bSpottedByMask);
    const auto scene=field<uintptr_t>(block,offsets::m_pGameSceneNode);
    std::array<uint8_t,offsets::m_bDormant+1-offsets::m_vecAbsOrigin> scene_block{};
    if (!scene || !mem_->read_buffer(scene+offsets::m_vecAbsOrigin,scene_block.data(),scene_block.size())) { p.health=0;p.state_valid=false;return; }
    p.scene=scene;
    p.origin=field<Vec3>(scene_block,0); p.dormant=field<uint8_t>(scene_block,offsets::m_bDormant-offsets::m_vecAbsOrigin)!=0;
    if (!p.origin.finite()) { p.health=0;p.state_valid=false;return; }
    if (options_.bones) read_bones(p,pawn);
    p.head=p.bones_valid ? p.bone[p.rig.target(0)].pos() : p.origin+Vec3{0,0,64};
    const auto view=field<Vec3>(block,offsets::m_vecViewOffset);
    p.eye=view.finite() && std::fabs(view.x)<=16 && std::fabs(view.y)<=16 && view.z>0 && view.z<100 ? p.origin+view : p.head;
    auto& detail=details_[p.controller_index];
    if(options_.weapons && (!options_.throttle_details || GetTickCount64()>=detail.next_weapon)) {
        read_weapon(p,pawn,system_);detail.next_weapon=GetTickCount64()+80+(p.controller_index%8)*5;
        detail.weapon_id=p.weapon_id;detail.clip=p.clip;detail.reserve=p.reserve;detail.utilities=p.util_ids;
    } else if(options_.weapons) {
        p.weapon_id=detail.weapon_id;p.clip=detail.clip;p.reserve=detail.reserve;
        if(options_.inventory)p.util_ids=detail.utilities;
    }
}
void EntityCache::read_bones(PlayerEnt& p, uintptr_t pawn) {
    const auto scene = p.scene;
    if (!scene) return;
    const auto binding = mem_->read<uintptr_t>(scene + offsets::m_modelState + offsets::m_hModel);
    p.model_binding=binding;
    const auto model = binding ? mem_->read<uintptr_t>(binding) : 0;
    if (model) {
        auto found=model_rigs_.find(model);
        if (found==model_rigs_.end()) {
            const int count=mem_->read<int>(model + offsets::MODEL_BONE_NAME_COUNT);
            const auto names=mem_->read<uintptr_t>(model + offsets::MODEL_BONE_NAME_DATA);
            if (count>0 && count<=512 && names) {
                std::vector<std::string> values; values.reserve(count);
                for (int i=0;i<count;++i) {
                    const auto ptr=mem_->read<uintptr_t>(names + i*8ULL);
                    const auto bytes=ptr ? mem_->read_bytes(ptr,80) : std::vector<uint8_t>{};
                    const auto end=std::find(bytes.begin(),bytes.end(),uint8_t{0});
                    values.emplace_back(bytes.begin(),end);
                }
                found=model_rigs_.emplace(model,bones::from_names(values)).first;
                auto info=std::make_shared<ModelInfo>();info->bones=values;
                const auto name_ptr=mem_->read<uintptr_t>(scene+offsets::m_modelState+offsets::m_ModelName);
                const auto text=name_ptr?mem_->read_bytes(name_ptr,260):std::vector<uint8_t>{};
                const auto terminator=std::find(text.begin(),text.end(),uint8_t{0});
                if(terminator!=text.end())info->name.assign(text.begin(),terminator);
                std::replace(info->name.begin(),info->name.end(),'\\','/');
                model_infos_[model]=std::move(info);
            }
        }
        if (found!=model_rigs_.end()) p.rig=found->second;
        auto info=model_infos_.find(model);
        if(info!=model_infos_.end() && info->second->name.empty()) {
            const auto ptr=mem_->read<uintptr_t>(scene+offsets::m_modelState+offsets::m_ModelName);
            const auto bytes=ptr?mem_->read_bytes(ptr,260):std::vector<uint8_t>{};
            const auto end=std::find(bytes.begin(),bytes.end(),uint8_t{0});
            if(end!=bytes.end() && end!=bytes.begin()) {
                auto replacement=std::make_shared<ModelInfo>(*info->second);
                replacement->name.assign(bytes.begin(),end);
                std::replace(replacement->name.begin(),replacement->name.end(),'\\','/');
                info->second=std::move(replacement);
            }
        }
        if(info!=model_infos_.end())p.model_info=info->second;
    }
    read_pose(p,scene);
}
bool EntityCache::read_pose(PlayerEnt& p,uintptr_t scene) const {
    const auto array=mem_->read<uintptr_t>(scene+offsets::m_modelState+offsets::m_pBoneArray);
    const int count=p.rig.read_count(), head_index=p.rig.target(0);
    p.bones_valid=false;
    if(!array || count<=0 || count>static_cast<int>(p.bone.size()) || head_index<0)return false;
    const size_t size=count*sizeof(BoneMat);
    std::array<BoneMat,128> verify{};
    if(!mem_->read_buffer(array,p.bone.data(),size) || !mem_->read_buffer(array,verify.data(),size))return false;
    if(std::memcmp(p.bone.data(),verify.data(),size)!=0) {
        if(!mem_->read_buffer(array,p.bone.data(),size) || std::memcmp(p.bone.data(),verify.data(),size)!=0)return false;
    }
    if(mem_->read<uintptr_t>(scene+offsets::m_modelState+offsets::m_pBoneArray)!=array)return false;
    Vec3 head{};p.bones_valid=p.bone_point(head_index,head) && head.z-p.origin.z>=8.f;
    if(p.bones_valid){p.head=head;p.pose_at=input_clock_ms();}
    return p.bones_valid;
}
bool EntityCache::current_local_team(int& team) const {
    const auto* player=local();
    if(!mem_ || !player || !player->valid() || !player->ctrl || !local_ctrl)return false;
    uintptr_t controller=0;uint32_t handle=0;uint8_t controller_team=0,pawn_team=0;
    if(!mem_->read_into(local_ctrl,controller) || controller!=player->ctrl ||
       !mem_->read_into(controller+offsets::m_hPlayerPawn,handle) || handle!=player->pawn_handle ||
       entity_handle(handle)!=player->pawn ||
       !mem_->read_into(controller+offsets::m_iTeamNum,controller_team) ||
       !mem_->read_into(player->pawn+offsets::m_iTeamNum,pawn_team) ||
       (pawn_team!=2 && pawn_team!=3) || controller_team!=pawn_team || pawn_team!=player->team)return false;
    team=pawn_team;return true;
}
bool EntityCache::current_observer_target(uintptr_t& target) const {
    target=0;const auto* p=local();
    if(!mem_ || !p || !p->ctrl)return false;
    uintptr_t controller=0;uint8_t alive=0;
    if(!mem_->read_into(local_ctrl,controller) || controller!=p->ctrl ||
       !mem_->read_into(controller+offsets::m_bPawnIsAlive,alive) || alive>1)return false;
    if(alive)return true;
    uint32_t observer_handle=0,target_handle=0;uintptr_t services=0;
    if(!mem_->read_into(controller+offsets::m_hObserverPawn,observer_handle))return false;
    const auto observer=entity_handle(observer_handle);
    if(!observer || !mem_->read_into(observer+offsets::m_pObserverServices,services) || !services ||
       !mem_->read_into(services+offsets::m_hObserverTarget,target_handle))return false;
    if(target_handle==0 || target_handle==0xFFFFFFFF)return true;
    target=entity_handle(target_handle);return target!=0;
}

bool EntityCache::current_crosshair_target(uintptr_t& target) const {
    target=0;const auto* local_player=local();if(!mem_ || !local_player || !local_player->valid())return false;
    uintptr_t controller=0;uint32_t handle=0,index=0;uint8_t alive=0;
    if(!mem_->read_into(local_ctrl,controller) || controller!=local_player->ctrl ||
       !mem_->read_into(controller+offsets::m_bPawnIsAlive,alive) || alive!=1 ||
       !mem_->read_into(controller+offsets::m_hPlayerPawn,handle) || handle!=local_player->pawn_handle ||
       entity_handle(handle)!=local_player->pawn || !mem_->read_into(local_player->pawn+offsets::m_iIDEntIndex,index))return false;
    if(index==0 || index==UINT32_MAX)return true;
    const auto candidate=entity_at(index&0x7fff);
    for(const auto& player:players_)if(player.pawn==candidate && !player.is_local &&
        (index<0x8000 || player.pawn_handle==index)){target=candidate;break;}
    return true;
}
bool EntityCache::current_enemy(const PlayerEnt& player,int local_team) const {
    if(!mem_ || !player.valid() || player.is_local || (local_team!=2 && local_team!=3))return false;
    uint32_t handle=0;uint8_t controller_team=0,pawn_team=0;int health=0;
    return mem_->read_into(player.ctrl+offsets::m_hPlayerPawn,handle) && handle==player.pawn_handle &&
        entity_handle(handle)==player.pawn && mem_->read_into(player.ctrl+offsets::m_iTeamNum,controller_team) &&
        mem_->read_into(player.pawn+offsets::m_iTeamNum,pawn_team) && (pawn_team==2 || pawn_team==3) &&
        controller_team==pawn_team && pawn_team==player.team && pawn_team!=local_team &&
        mem_->read_into(player.pawn+offsets::m_iHealth,health) && health>0 && health<=1000;
}
bool EntityCache::refresh_pose(const PlayerEnt& source,PlayerEnt& out) const {
    if(!mem_ || !source.valid() || !source.scene || !source.ctrl)return false;
    const auto handle=mem_->read<uint32_t>(source.ctrl+offsets::m_hPlayerPawn);
    if(entity_handle(handle)!=source.pawn || mem_->read<int>(source.pawn+offsets::m_iHealth)<=0 || mem_->read<uint8_t>(source.pawn+offsets::m_iTeamNum)!=source.team)return false;
    const auto scene=mem_->read<uintptr_t>(source.pawn+offsets::m_pGameSceneNode);
    if(scene!=source.scene || mem_->read<uintptr_t>(scene+offsets::m_modelState+offsets::m_hModel)!=source.model_binding)return false;
    Vec3 origin{};
    if(!mem_->read_into(scene+offsets::m_vecAbsOrigin,origin) || !origin.finite() || mem_->read<uint8_t>(scene+offsets::m_bDormant)!=0)return false;
    out=source;out.origin=origin;out.eye=origin+(source.eye-source.origin);
    out.velocity_valid=mem_->read_into(source.pawn+offsets::m_vecAbsVelocity,out.velocity) && out.velocity.finite() && out.velocity.len()<=4096.f;
    if(!read_pose(out,scene))return false;
    Vec3 verify{};
    return mem_->read_into(scene+offsets::m_vecAbsOrigin,verify) && std::memcmp(&origin,&verify,sizeof(origin))==0 &&
        mem_->read<uint32_t>(source.ctrl+offsets::m_hPlayerPawn)==handle;
}
bool EntityCache::visual_pose(const PlayerEnt& source,PlayerEnt& out,ULONGLONG age_ms,bool validate_live) const {
    // Render-only continuity. Input/aim keep their independent strict freshness checks.
    if(!source.valid() || (!source.bones_valid && !source.collision_valid) || age_ms>VISUAL_GRACE_MS)return false;
    if(age_ms<=8 && !validate_live){out=source;return true;}
    if(!mem_ || !source.scene || !source.ctrl || !source.pawn_handle)return false;
    constexpr size_t ControllerStateBytes=(std::max)(size_t(offsets::m_hPlayerPawn+4),size_t(offsets::m_bPawnIsAlive+1));
    std::array<uint8_t,ControllerStateBytes> controller{};
    if(!mem_->read_buffer(source.ctrl,controller.data(),controller.size()) ||
       field<uint32_t>(controller,offsets::m_hPlayerPawn)!=source.pawn_handle || field<uint8_t>(controller,offsets::m_bPawnIsAlive)!=1)return false;
    constexpr size_t IdentityBytes=(std::max)({size_t(offsets::m_iHealth+4),size_t(offsets::m_iTeamNum+1),size_t(offsets::m_pGameSceneNode+sizeof(uintptr_t))});
    std::array<uint8_t,IdentityBytes> identity{};
    if(!mem_->read_buffer(source.pawn,identity.data(),identity.size()) ||
       (field<int>(identity,offsets::m_iHealth)<=0 || field<int>(identity,offsets::m_iHealth)>1000) || field<uint8_t>(identity,offsets::m_iTeamNum)!=source.team ||
       field<uintptr_t>(identity,offsets::m_pGameSceneNode)!=source.scene)return false;
    constexpr size_t SceneBytes=(std::max)({size_t(offsets::m_vecAbsOrigin+sizeof(Vec3)),size_t(offsets::m_bDormant+1),size_t(offsets::m_modelState+offsets::m_hModel+sizeof(uintptr_t))});
    std::array<uint8_t,SceneBytes> scene{};
    if(!mem_->read_buffer(source.scene,scene.data(),scene.size()) ||
       field<uint8_t>(scene,offsets::m_bDormant)!=0 ||
       field<uintptr_t>(scene,offsets::m_modelState+offsets::m_hModel)!=source.model_binding)return false;
    const auto origin=field<Vec3>(scene,offsets::m_vecAbsOrigin),delta=origin-source.origin;
    if(!origin.finite() || delta.len()>64.f)return false;
    out=source;out.health=field<int>(identity,offsets::m_iHealth);out.origin=origin;out.eye=source.eye+delta;out.head=source.head+delta;
    for(auto& bone:out.bone)bone.position=bone.position+delta;
    return true;
}

EntityCache EntityCache::snapshot() const {
    EntityCache next;
    next.mem_=mem_;next.players_=players_;next.local_index_=local_index_;next.vm_=vm_;next.bomb_=bomb_;
    next.items_=items_;next.system_=system_;next.controller_chunk_=controller_chunk_;next.client_=client_;next.ready_=ready_;
    next.sensitivity_=sensitivity_;next.status_=status_;next.sampled_at_=sampled_at_;next.map_name_=map_name_;
    next.collision_=collision_;
    next.ent_system=ent_system;next.local_ctrl=local_ctrl;next.vm_addr=vm_addr;next.c4_list=c4_list;
    next.globals_addr=globals_addr;next.view_angles_addr=view_angles_addr;
    return next;
}
void EntityCache::read_weapon(PlayerEnt& p, uintptr_t pawn, uintptr_t) {
    const auto services = mem_->read<uintptr_t>(pawn + offsets::m_pWeaponServices);
    if (!services) return;
    const auto active = entity_handle(mem_->read<uint32_t>(services + offsets::m_hActiveWeapon));
    if (active) {
        p.weapon_id = mem_->read<uint16_t>(active + offsets::m_AttributeManager + offsets::m_Item + offsets::m_iItemDefinitionIndex);
        p.clip = mem_->read<int>(active + offsets::m_iClip1);
        p.reserve = mem_->read<int>(active + offsets::m_pReserveAmmo);
    }
    if (!options_.inventory) return;
    const auto count = mem_->read<int>(services + offsets::m_hMyWeapons);
    const auto data = mem_->read<uintptr_t>(services + offsets::m_hMyWeapons + 8);
    if (!data || count < 0 || count > 64) return;
    for (int i = 0; i < count; ++i) {
        const auto weapon = entity_handle(mem_->read<uint32_t>(data + i * 4ULL));
        if (!weapon) continue;
        const auto id = mem_->read<uint16_t>(weapon + offsets::m_AttributeManager + offsets::m_Item + offsets::m_iItemDefinitionIndex);
        if (weapons::is_utility(id)) p.util_ids.push_back(id);
    }
}
void EntityCache::update_items() {
    if (!system_) { items_.clear(); pending_items_.clear(); item_last_=0; return; }
    if (!item_last_) {
        if (GetTickCount64()<next_item_scan_) return;
        const auto highest=mem_->read<int>(system_ + game_offsets::dwGameEntitySystem_highestEntityIndex);
        if (highest<65 || highest>0x7FFE) { items_.clear(); return; }
        pending_items_.clear(); item_cursor_=65; item_last_=highest;
    }
    const int stop=(std::min)(item_last_+1,item_cursor_+128);
    for (int i=item_cursor_;i<stop;++i) {
        const auto ent = entity_at(i);
        if (!ent) continue;
        const auto identity = mem_->read<uintptr_t>(ent + offsets::m_pEntity);
        const auto name_ptr = identity ? mem_->read<uintptr_t>(identity + offsets::m_designerName) : 0;
        const auto name = name_ptr ? mem_->read_bytes(name_ptr, 64) : std::vector<uint8_t>{};
        if (name.size() < 7 || std::memcmp(name.data(), "weapon_", 7) != 0) continue;
        const auto owner = mem_->read<uint32_t>(ent + offsets::m_hOwnerEntity);
        if (owner != UINT32_MAX && entity_handle(owner)) continue;
        const auto scene = mem_->read<uintptr_t>(ent + offsets::m_pGameSceneNode);
        if (!scene) continue;
        WorldItem item;
        item.origin = mem_->read<Vec3>(scene + offsets::m_vecAbsOrigin);
        item.weapon_id = mem_->read<uint16_t>(ent + offsets::m_AttributeManager + offsets::m_Item + offsets::m_iItemDefinitionIndex);
        if (item.weapon_id && item.origin.finite()) pending_items_.push_back(item);
    }
    item_cursor_=stop;
    if (item_cursor_>item_last_) { items_.swap(pending_items_); pending_items_.clear(); item_last_=0; next_item_scan_=GetTickCount64()+250; }
}
bool EntityCache::current_bomb_state(BombState& result) const {
    result={};
    if (!mem_ || !c4_list || !globals_addr) return false;
    uintptr_t c4=0;
    if (!mem_->read_into(c4_list,c4)) return false;
    if (!c4) return true;
    constexpr size_t begin=offsets::m_bBombTicking;
    constexpr size_t end=(std::max)({size_t(offsets::m_bBombDefused+1),size_t(offsets::m_flDefuseCountDown+4),
        size_t(offsets::m_flDefuseLength+4),size_t(offsets::m_flTimerLength+4),size_t(offsets::m_nBombSite+4)});
    std::array<uint8_t,end-begin> block{};
    if (!mem_->read_buffer(c4+begin,block.data(),block.size())) return false;
    const auto ticking=field<uint8_t>(block,offsets::m_bBombTicking-begin);
    const auto defused=field<uint8_t>(block,offsets::m_bBombDefused-begin);
    const auto defusing=field<uint8_t>(block,offsets::m_bBeingDefused-begin);
    if (ticking>1 || defused>1 || defusing>1) return false;
    BombState next; next.entity=c4; next.ticking=ticking==1; next.defused=defused==1;
    next.blow_time=field<float>(block,offsets::m_flC4Blow-begin);
    next.timer_length=field<float>(block,offsets::m_flTimerLength-begin);
    next.defuse_end=field<float>(block,offsets::m_flDefuseCountDown-begin);
    next.defuse_length=field<float>(block,offsets::m_flDefuseLength-begin);
    next.site=field<int>(block,offsets::m_nBombSite-begin);
    const auto globals=mem_->read<uintptr_t>(globals_addr);
    if (!globals || !mem_->read_into(globals+offsets::GLOBAL_CURTIME,next.server_time) || !std::isfinite(next.server_time)) return false;
    if (mem_->read<uintptr_t>(c4_list)!=c4) return false;
    const float remaining=next.blow_time-next.server_time;
    next.clock_valid=next.server_time>=0 && std::isfinite(remaining) && std::isfinite(next.timer_length) &&
        next.timer_length>0 && next.timer_length<=3600 && remaining>=-2 && remaining<=next.timer_length+1;
    next.remaining=next.clock_valid ? (std::max)(0.f,remaining) : 0;
    next.planted=next.clock_valid && next.ticking && !next.defused && remaining>0;
    next.defusing=next.planted && defusing==1;
    const float defuse_remaining=next.defuse_end-next.server_time;
    next.defuse_valid=next.defusing && std::isfinite(next.defuse_end) && std::isfinite(next.defuse_length) &&
        next.defuse_length>0 && next.defuse_length<=60 && defuse_remaining>=0 && defuse_remaining<=next.defuse_length+.25f;
    if (next.defuse_valid) next.defuse_remaining=defuse_remaining;
    const auto scene=mem_->read<uintptr_t>(c4+offsets::m_pGameSceneNode);
    if (scene) next.pos=mem_->read<Vec3>(scene+offsets::m_vecAbsOrigin);
    result=next; return true;
}
void EntityCache::update_bomb() { current_bomb_state(bomb_); }
