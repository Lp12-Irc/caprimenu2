#include "entities.hpp"
#include "aimbot.hpp"
#include "misc.hpp"
#include "esp.hpp"
#include "fov_geometry.hpp"
#include "head_gate.hpp"
#include <limits>
#include "game_viewport.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <filesystem>
#include "render_capture.hpp"

static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T, class Buffer> static void put(Buffer& buffer, size_t offset, T value) {
    check(offset + sizeof(T) <= buffer.size(), "fixture overflow");
    std::memcpy(buffer.data() + offset, &value, sizeof(value));
}
static std::vector<INPUT> sent;
static ULONGLONG clock_ms = 1000;
static bool held = true;
static int held_key=0;
static bool accept_input=true;
static Vec3* simulated_view=nullptr;
static UINT WINAPI record_input(UINT count, LPINPUT input, int) {
    sent.insert(sent.end(), input, input+count);
    if (!accept_input) return 0;
    if (simulated_view) for (UINT i=0;i<count;++i) if (input[i].mi.dwFlags&MOUSEEVENTF_MOVE) {
        simulated_view->x+=input[i].mi.dy*.022f; simulated_view->y-=input[i].mi.dx*.022f;
    }
    return count;
}
static SHORT WINAPI key_state(int key) { return held && (!held_key || key==held_key) ? static_cast<SHORT>(0x8000) : 0; }
static ULONGLONG WINAPI test_clock() { return clock_ms; }
int main(int argc,char** argv) {
    Memory memory;
    try {
        char path[MAX_PATH]{}; GetModuleFileNameA(nullptr, path, MAX_PATH);
        check(memory.attach(std::filesystem::path(path).filename().string()), "self read handle");
        std::vector<uint8_t> system(0x3000), chunk0(0x70*512), chunk1(0x70*512);
        std::vector<uint8_t> local_ctrl(0x1000), enemy_ctrl(0x1000), observer_ctrl(0x1000);
        std::vector<uint8_t> local_pawn(0x4000), enemy_pawn(0x4000), observer_pawn(0x4000);
        std::vector<uint8_t> local_scene(0x300), enemy_scene(0x300), observer_services(0x100), weapon_services(0x100), weapon(0x2000);
        std::array<BoneMat,32> local_bones{}, enemy_bones{};
        const auto addr=[](const auto& buffer) { return reinterpret_cast<uintptr_t>(buffer.data()); };
        put(system,0x10,addr(chunk0)); put(system,0x18,addr(chunk1));
        put(chunk0,0x70,addr(local_ctrl)); put(chunk0,0xE0,addr(enemy_ctrl)); put(chunk0,0x150,addr(observer_ctrl));
        put(chunk0,0x70*70,addr(local_pawn)); put(chunk1,0x70*(600-512),addr(enemy_pawn)); put(chunk0,0x70*71,addr(observer_pawn));
        put(local_ctrl,offsets::m_iTeamNum,uint8_t{2}); put(enemy_ctrl,offsets::m_iTeamNum,uint8_t{3}); put(observer_ctrl,offsets::m_iTeamNum,uint8_t{2});
        put(local_ctrl,offsets::m_bPawnIsAlive,true); put(enemy_ctrl,offsets::m_bPawnIsAlive,true);
        put(local_ctrl,offsets::m_hPlayerPawn,uint32_t{0x460000|70}); put(enemy_ctrl,offsets::m_hPlayerPawn,uint32_t{0x460000|600});
        put(observer_ctrl,offsets::m_hObserverPawn,uint32_t{71});
        put(local_pawn,offsets::m_iHealth,100); put(enemy_pawn,offsets::m_iHealth,87);
        put(local_pawn,offsets::m_iTeamNum,uint8_t{2}); put(enemy_pawn,offsets::m_iTeamNum,uint8_t{3});
        put(local_pawn,offsets::m_pGameSceneNode,addr(local_scene)); put(enemy_pawn,offsets::m_pGameSceneNode,addr(enemy_scene));
        put(local_scene,offsets::m_vecAbsOrigin,Vec3{10,20,30}); put(enemy_scene,offsets::m_vecAbsOrigin,Vec3{100,200,300});
        local_bones[bones::HEAD].position={10,20,94}; enemy_bones[bones::HEAD].position={100,200,364};
        local_bones[bones::HEAD].scale=enemy_bones[bones::HEAD].scale=1;
        put(local_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(local_bones));
        put(enemy_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(enemy_bones));
        put(observer_pawn,offsets::m_pObserverServices,addr(observer_services)); put(observer_services,offsets::m_hObserverTarget,uint32_t{70});
        put(local_pawn,offsets::m_pWeaponServices,addr(weapon_services)); put(weapon_services,offsets::m_hActiveWeapon,uint32_t{72});
        put(chunk0,0x70*72,addr(weapon)); put(weapon,offsets::m_AttributeManager+offsets::m_Item+offsets::m_iItemDefinitionIndex,uint16_t{7});
        put(weapon,offsets::m_iClip1,30); put(weapon,offsets::m_pReserveAmmo,90);
        uintptr_t system_ptr=addr(system), local_ptr=addr(local_ctrl);
        EntityCache cache; cache.init(&memory);
        cache.ent_system=reinterpret_cast<uintptr_t>(&system_ptr); cache.local_ctrl=reinterpret_cast<uintptr_t>(&local_ptr);
        cache.update();
        check(cache.ready() && cache.players().size()==3,"controller iteration");
        check(cache.local() && cache.local()->pawn==addr(local_pawn),"handle serial mask");
        check(cache.players()[1].pawn==addr(enemy_pawn) && cache.players()[1].health==87,"cross-chunk handle");
        check(cache.players()[1].bones_valid && cache.players()[1].head.z==364,"32-byte transform positions");
        check(cache.players()[2].obs_target==addr(local_pawn),"dead observer target resolution");
        // The viewed pawn must remain available with the spectator list disabled.
        put(local_ctrl,offsets::m_bPawnIsAlive,false);
        put(local_ctrl,offsets::m_hObserverPawn,uint32_t{71});
        put(observer_services,offsets::m_hObserverTarget,uint32_t{0x460000|600});
        CacheOptions no_spectator_list;no_spectator_list.observers=false;cache.update(no_spectator_list);
        uintptr_t viewed=0;
        check(cache.local()->obs_target==addr(enemy_pawn) && cache.current_observer_target(viewed) && viewed==addr(enemy_pawn),
              "camera subject remains known when spectator list is disabled");
        const auto spectator_snapshot=cache.snapshot();
        put(observer_services,offsets::m_hObserverTarget,uint32_t{70});
        check(spectator_snapshot.current_observer_target(viewed) && viewed==addr(local_pawn),
              "spectator switch is read live without waiting for an old entity snapshot");
        put(local_ctrl,offsets::m_bPawnIsAlive,true);
        put(local_ctrl,offsets::m_hObserverPawn,uint32_t{0});cache.update();
        check(cache.current_observer_target(viewed) && viewed==0,"respawn clears the watched-player exclusion");

        check(cache.local()->weapon_id==7 && cache.local()->clip==30 && cache.local()->reserve==90,"active weapon service and nested item");
        check(cache.entity_handle(UINT32_MAX)==0 && cache.entity_handle(0)==0,"invalid handles");
        CacheOptions lean;lean.items=lean.inventory=lean.recoil=lean.bomb=lean.observers=false;
        lean.weapons=lean.names=lean.angles=lean.crosshair=false;
        cache.update(lean);
        check(cache.local()->valid() && cache.players()[1].bones_valid && cache.local()->weapon_id==0 &&
            cache.local()->crosshair_target==0,"lean cache preserves health and bones without unused details");
        cache.update();
        // A current pose must see movement after publication, and reject a reused or dead pawn.
        const auto published=cache.snapshot();
        const auto previous_head=published.players()[1].head;
        enemy_bones[bones::HEAD].position.x+=5;
        PlayerEnt refreshed;
        check(published.refresh_pose(published.players()[1],refreshed) && refreshed.head.x==previous_head.x+5,"fresh pose sees movement after snapshot publication");
        check(published.players()[1].head.x==previous_head.x,"fresh pose does not mutate the published snapshot");
        put(enemy_pawn,offsets::m_iHealth,0);
        check(!published.refresh_pose(published.players()[1],refreshed),"fresh pose rejects death after publication");
        put(enemy_pawn,offsets::m_iHealth,87);
        put(enemy_ctrl,offsets::m_hPlayerPawn,uint32_t{0});
        check(!published.refresh_pose(published.players()[1],refreshed),"fresh pose rejects changed pawn handles");
        put(enemy_ctrl,offsets::m_hPlayerPawn,uint32_t{0x460000|600});
        enemy_bones[bones::HEAD].position.x-=5;
        put(enemy_scene,offsets::m_modelState+offsets::m_pBoneArray,uintptr_t{0});
        check(!published.refresh_pose(published.players()[1],refreshed),"aim still rejects an incomplete pose");
        check(published.visual_pose(published.players()[1],refreshed,12) && refreshed.head.z==previous_head.z,"ESP bridges one incomplete pose using a validated recent sample");
        check(published.visual_pose(published.players()[1],refreshed,40),"ESP bridges a low-FPS frame without another bone read");
        check(!published.visual_pose(published.players()[1],refreshed,121),"ESP cannot retain an expired pose beyond 120 ms");
        put(enemy_pawn,offsets::m_iHealth,0);
        check(!published.visual_pose(published.players()[1],refreshed,12),"ESP bridge cannot retain a dead player");
        put(enemy_pawn,offsets::m_iHealth,87);
        put(enemy_ctrl,offsets::m_hPlayerPawn,uint32_t{0});
        check(!published.visual_pose(published.players()[1],refreshed,12),"ESP bridge cannot retain a replaced player");
        put(enemy_ctrl,offsets::m_hPlayerPawn,uint32_t{0x460000|600});
        put(enemy_scene,offsets::m_vecAbsOrigin,Vec3{900,200,300});
        check(!published.visual_pose(published.players()[1],refreshed,12),"ESP bridge rejects large position discontinuities");
        put(enemy_scene,offsets::m_vecAbsOrigin,Vec3{100,200,300});
        put(enemy_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(enemy_bones));
        put(enemy_scene,offsets::m_vecAbsOrigin,Vec3{124,200,300});
        memory.profile_reads(true);memory.reset_read_stats();
        check(published.visual_pose(published.players()[1],refreshed,60) && refreshed.head.x==previous_head.x+24,"Visual bridge follows measured origin movement without extrapolation");
        check(memory.read_stats().calls<=4,"Visual bridge repeats expensive bone-array reads");
        memory.profile_reads(false);
        check(published.players()[1].head.x==previous_head.x,"Visual bridge mutates the shared snapshot");
        put(enemy_scene,offsets::m_vecAbsOrigin,Vec3{100,200,300});
        // Names, rather than constant indices, must identify the body in a differently ordered asset.
        const std::vector<std::string> canonical={"root_motion","pelvis","spine_0","spine_1","spine_2","spine_3","neck_0","head_0",
            "clavicle_L","arm_upper_L","arm_lower_L","hand_L","clavicle_R","arm_upper_R","arm_lower_R","hand_R",
            "jiggle_primary","leg_upper_L","leg_lower_L","ankle_L","leg_upper_R","leg_lower_R","ankle_R","wpnPivot","wpn","eyeball_l","eyeball_r","eye_target"};
        const auto body=bones::from_names(canonical);
        check(body.target(0)==7 && body.target(1)==6 && body.joint[bones::LHip]==17 && body.joint[bones::RAnkle]==22,"named head, neck and legs match the live model");
        for (const auto& edge:bones::CONNECTIONS) for (const int joint:edge)
            check(body.joint[joint]>=1 && body.joint[joint]<=22 && body.joint[joint]!=16,"accessory and eye-target bones are excluded");
        std::vector<uint8_t> model(0x300), binding(8), name_ptrs(64*8);
        std::array<std::array<char,80>,64> name_text{};
        std::array<BoneMat,128> remapped_bones{};
        for (size_t i=0;i<canonical.size();++i) {
            strcpy_s(name_text[i+32].data(),name_text[i+32].size(),canonical[i].c_str());
            put(name_ptrs,(i+32)*8,reinterpret_cast<uintptr_t>(name_text[i+32].data()));
        }
        put(model,offsets::MODEL_BONE_NAME_COUNT,64); put(model,offsets::MODEL_BONE_NAME_DATA,addr(name_ptrs));
        const char model_name[]="agents/models/ctm_sas/ctm_sas.vmdl";
        put(local_scene,offsets::m_modelState+offsets::m_ModelName,reinterpret_cast<uintptr_t>(model_name));
        put(binding,0,addr(model)); put(local_scene,offsets::m_modelState+offsets::m_hModel,addr(binding));
        remapped_bones[7+32].position={10,20,94};
        remapped_bones[7+32].scale=1;
        put(local_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(remapped_bones)); cache.update();
        check(cache.local()->model_info && cache.local()->model_info->name==model_name &&
              cache.local()->model_info->bones.size()==64,"live model identity and complete bone names");
        check(cache.local()->rig.named && cache.local()->rig.target(0)==39 && cache.local()->head.z==94,"live bone array follows model-specific names");
        put(local_scene,offsets::m_modelState+offsets::m_hModel,uintptr_t{0});
        put(local_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(local_bones)); cache.update();
        // Exercise input behavior with a recording sink: nothing reaches the real mouse.
        const InputApi input{record_input,key_state,test_clock};
        enemy_bones[bones::HEAD].position={110,30,94};
        put(enemy_scene,offsets::m_vecAbsOrigin,Vec3{110,30,30}); cache.update();
        Config cfg; cfg.aim_visibility=false;cfg.aim_target=0;cfg.aim=true; cfg.aim_ads_only=false; cfg.aim_fov=15; cfg.aim_smooth=1; cfg.aim_delay_ms=100;
        Aimbot aim(input);
        held=false;memory.reset_read_stats();aim.update(cache,cfg,&memory);
        check(memory.read_stats().calls==0,"unpressed aim key performs no remote identity reads");held=true;
        aim.update(cache,cfg,&memory);
        check(sent.empty(),"aim waits without sleeping");
        clock_ms=1099; aim.update(cache,cfg,&memory); check(sent.empty(),"aim acquisition delay respected");
        clock_ms=1100; aim.update(cache,cfg,&memory);
        check(sent.size()==1 && sent.back().mi.dx<0 && sent.back().mi.dwFlags==MOUSEEVENTF_MOVE,"Source yaw maps to negative mouse X");
        sent.clear();aim.reset();clock_ms=1200;aim.update(cache,cfg,&memory);
        enemy_bones[bones::HEAD].scale=0;cache.update();clock_ms=1240;aim.update(cache,cfg,&memory);
        check(sent.empty(),"missing pose never receives a correction");
        enemy_bones[bones::HEAD].scale=1;cache.update();clock_ms=1300;aim.update(cache,cfg,&memory);
        check(sent.size()==1,"one missing pose does not restart target acquisition delay");
        sent.clear();aim.reset();clock_ms=1400;aim.update(cache,cfg,&memory);aim.suspend();
        clock_ms=1500;aim.update(cache,cfg,&memory);
        check(sent.size()==1,"brief sampling pause preserves acquisition without moving on stale data");
        cfg.aim_delay_ms=0;cfg.aim_sens=5;cfg.aim_auto_sens=true;
        sent.clear();aim.reset();clock_ms=1600;aim.update(cache,cfg,&memory);check(!sent.empty(),"automatic sensitivity accepts current game scale");
        const auto automatic=sent.back().mi.dx;cfg.aim_auto_sens=false;
        sent.clear();aim.reset();clock_ms=1700;aim.update(cache,cfg,&memory);
        check(!sent.empty() && std::abs(automatic)>std::abs(sent.back().mi.dx)*4,"automatic sensitivity avoids a second manual sensitivity multiplier");
        cfg.aim_auto_sens=true;cfg.aim_sens=1;
        // Menus must keep their own pointer and input. Failed cursor queries fail closed.
        check(gameplay_cursor_captured(0,true),"hidden cursor permits gameplay input");
        check(!gameplay_cursor_captured(CURSOR_SHOWING,true) && !gameplay_cursor_captured(CURSOR_SUPPRESSED,true) &&
            !gameplay_cursor_captured(0,false),"free or unknown cursor blocks gameplay automation");
        // A snapshot can outlive a team switch or controller replacement between two cache polls.
        put(enemy_ctrl,offsets::m_iTeamNum,uint8_t{2});put(enemy_pawn,offsets::m_iTeamNum,uint8_t{2});
        sent.clear();aim.reset();aim.update(cache,cfg,&memory);
        check(sent.empty(),"a cached enemy that joined our team receives no aim command");
        put(enemy_ctrl,offsets::m_iTeamNum,uint8_t{3});put(enemy_pawn,offsets::m_iTeamNum,uint8_t{3});
        put(local_ctrl,offsets::m_iTeamNum,uint8_t{3});put(local_pawn,offsets::m_iTeamNum,uint8_t{3});
        sent.clear();aim.reset();aim.update(cache,cfg,&memory);
        check(sent.empty(),"a local team switch invalidates the old target snapshot");
        put(local_ctrl,offsets::m_iTeamNum,uint8_t{2});put(local_pawn,offsets::m_iTeamNum,uint8_t{2});
        local_ptr=addr(observer_ctrl);sent.clear();aim.reset();aim.update(cache,cfg,&memory);
        check(sent.empty(),"controller replacement invalidates gameplay input");local_ptr=addr(local_ctrl);
        enemy_bones[bones::HEAD].position.z=40;cache.update();
        sent.clear();aim.reset();aim.update(cache,cfg,&memory);
        check(sent.empty(),"a head entry at ankle height cannot move the mouse");
        enemy_bones[bones::HEAD].position.z=94;cache.update();
        sent.clear();aim.reset();aim.update(cache,cfg,&memory);
        check(!sent.empty(),"validated enemy head recovers after invalid pose");
        sent.clear(); aim.reset(); cfg.aim_target=1; cfg.aim_fov=1; cfg.aim_delay_ms=0;
        aim.update(cache,cfg,&memory); check(sent.empty(),"distance targeting still enforces FOV");
        cfg.aim=false; cfg.rcs=true;
        std::vector<uint8_t> punch(0x100);
        put(local_pawn,offsets::m_pAimPunchServices,addr(punch));
        put(punch,offsets::m_predictableBaseAngle,Vec3{1.5f,1.f,0});
        put(local_pawn,offsets::m_iShotsFired,3); cache.update();
        aim.update(cache,cfg,&memory);
        check(sent.size()==1 && sent.back().mi.dy<0 && sent.back().mi.dx>0,"standalone recoil compensation works with aimbot off");
        sent.clear(); aim.update(cache,cfg,&memory);
        check(sent.empty(),"unchanged recoil is not repeatedly accumulated");
        Matrix4x4 vm{}; vm.m[0][0]=.001f; vm.m[0][3]=-.11f; vm.m[1][1]=.001f; vm.m[1][3]=-.03f; vm.m[3][3]=1;
        cache.vm_addr=reinterpret_cast<uintptr_t>(&vm); cache.update();
        Misc misc(input); cfg.tb_interval_ms=20; cfg.tb_head_only=false; cfg.rcs=false; cfg.triggerbot=true; cfg.tb_delay_ms=75; cfg.tb_release_ms=20;
        clock_ms=1900; misc.update(cache,cfg,&memory,800,600); check(sent.empty(),"screen overlap alone cannot trigger a shot");
        put(local_pawn,offsets::m_iIDEntIndex,600); cache.update();
        clock_ms=2000; misc.update(cache,cfg,&memory,800,600); check(sent.empty(),"trigger acquisition delay");
        clock_ms=2074; misc.update(cache,cfg,&memory,800,600); check(sent.empty(),"trigger does not fire early");
        clock_ms=2075; misc.update(cache,cfg,&memory,800,600);
        check(sent.size()==1 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTDOWN,"trigger uses game crosshair entity and presses");
        clock_ms=2094; misc.update(cache,cfg,&memory,800,600); check(sent.size()==1,"trigger release is nonblocking");
        clock_ms=2095; misc.update(cache,cfg,&memory,800,600);
        check(sent.size()==2 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTUP,"trigger releases on deadline");
        clock_ms=2170; misc.update(cache,cfg,&memory,800,600); misc.reset();
        check(sent.back().mi.dwFlags==MOUSEEVENTF_LEFTUP,"pause cannot leave injected button held");
        check(memory.attach_write(std::filesystem::path(path).filename().string()),"self write handle");
        cfg.triggerbot=false; cfg.no_flash=true;
        put(local_pawn,offsets::m_flFlashMaxAlpha,255.f);
        misc.update(cache,cfg,&memory,800,600);
        check(memory.read<float>(addr(local_pawn)+offsets::m_flFlashMaxAlpha)==0.f,"no flash changes the intended alpha field");
        cfg.no_flash=false; misc.update(cache,cfg,&memory,800,600);
        check(memory.read<float>(addr(local_pawn)+offsets::m_flFlashMaxAlpha)==255.f,"disabling no flash restores original alpha");
        // Configured keys work alone; release cancels an injected click immediately.
        cfg.triggerbot=true; cfg.tb_key=VK_F8; cfg.tb_delay_ms=0; held_key=VK_F8;
        sent.clear(); clock_ms=2200; misc.update(cache,cfg,&memory,800,600);
        check(sent.size()==1 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTDOWN,"zero-delay trigger fires on its configured key alone");
        held=false; clock_ms=2201; misc.update(cache,cfg,&memory,800,600);
        check(sent.size()==2 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTUP,"key release immediately releases a held click");
        held=true; accept_input=false; misc.update(cache,cfg,&memory,800,600);
        check(misc.trigger_state()==Misc::TriggerState::InputRejected,"rejected mouse input is observable");
        accept_input=true; clock_ms=2203; misc.update(cache,cfg,&memory,800,600);
        check(misc.trigger_state()==Misc::TriggerState::Pressed,"trigger recovers after rejected input"); misc.reset();
        // Revalidate the real crosshair value instead of firing from a cached target.
        sent.clear();put(local_pawn,offsets::m_iIDEntIndex,uint32_t{0xffffffff});clock_ms=2300;
        misc.update(cache,cfg,&memory,800,600);check(sent.empty(),"stale cached crosshair cannot fire after aim leaves target");
        put(local_pawn,offsets::m_iIDEntIndex,uint32_t{0x470000|600});
        misc.update(cache,cfg,&memory,800,600);check(sent.empty(),"crosshair handle with mismatched serial cannot fire");
        put(local_pawn,offsets::m_iIDEntIndex,uint32_t{0x460000|600});
        misc.update(cache,cfg,&memory,800,600);check(sent.size()==1,"full matching crosshair handle resolves enemy");misc.reset();
        put(local_pawn,offsets::m_iIDEntIndex,600);sent.clear();cfg.tb_delay_ms=75;
        Misc continuity(input);clock_ms=3000;continuity.update(cache,cfg,&memory,800,600);
        clock_ms=3040;continuity.suspend(cfg);check(sent.empty(),"short cache gap cannot emit a press");
        clock_ms=3075;continuity.update(cache,cfg,&memory,800,600);check(sent.size()==1,"brief cache gap does not restart acquisition delay");
        clock_ms=3095;continuity.update(cache,cfg,&memory,800,600);check(sent.size()==2 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTUP,"release occurs without another press in the same tick");
        cfg.tb_delay_ms=0;clock_ms=3096;continuity.update(cache,cfg,&memory,800,600);check(sent.size()==2,"release interval is observable before next click");
        clock_ms=3115;continuity.update(cache,cfg,&memory,800,600);check(sent.size()==3,"trigger resumes after release interval");continuity.reset();
        {
            Misc instant(input);Config immediate=cfg;immediate.tb_head_only=false;
            immediate.tb_delay_ms=0;immediate.tb_interval_ms=0;immediate.tb_release_ms=16;
            sent.clear();clock_ms=3200;instant.update(cache,immediate,&memory,800,600);
            check(sent.size()==1 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTDOWN,"zero-delay trigger fires on the first eligible update");
            clock_ms=3216;instant.update(cache,immediate,&memory,800,600);
            check(sent.size()==2 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTUP,"fast click still emits a separate release");
            clock_ms=3218;instant.update(cache,immediate,&memory,800,600);
            check(sent.size()==3 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTDOWN,"zero interval repeats without requiring key reactivation");
            instant.reset();sent.clear();
        }
        // Head-only checks use the live pose and camera; the input sink records all clicks.
        {
        const auto saved_camera=vm;
        vm={};vm.m[0][1]=1;vm.m[0][3]=-30;vm.m[1][2]=1;vm.m[1][3]=-74;vm.m[3][0]=1;
        cfg.tb_head_only=true;cfg.tb_delay_ms=75;cfg.tb_release_ms=30;
        Misc head_trigger(input);sent.clear();clock_ms=4000;
        head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.empty() && head_trigger.trigger_state()==Misc::TriggerState::WaitingHead,"body crosshair cannot fire in head-only mode");
        vm.m[1][3]=-94;clock_ms=4010;head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.empty() && head_trigger.trigger_state()==Misc::TriggerState::Delay,"head alignment starts the acquisition delay");
        vm.m[1][3]=-74;clock_ms=4084;head_trigger.update(cache,cfg,&memory,800,600);
        vm.m[1][3]=-94;clock_ms=4085;head_trigger.update(cache,cfg,&memory,800,600);
        clock_ms=4159;head_trigger.update(cache,cfg,&memory,800,600);check(sent.empty(),"leaving head region restarts the full delay");
        clock_ms=4160;head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.size()==1 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTDOWN,"validated head alignment permits a click");
        vm.m[1][3]=-74;clock_ms=4161;head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.size()==2 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTUP,"leaving head releases immediately before the click deadline");
        head_trigger.reset();sent.clear();cfg.tb_delay_ms=0;vm.m[1][3]=-94;
        put(enemy_scene,offsets::m_modelState+offsets::m_pBoneArray,uintptr_t{0});clock_ms=4200;
        head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.empty() && head_trigger.trigger_state()==Misc::TriggerState::WaitingData,"missing live bones do not fall back to body firing");
        put(enemy_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(enemy_bones));
        vm.m[0][0]=std::numeric_limits<float>::quiet_NaN();head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.empty() && head_trigger.trigger_state()==Misc::TriggerState::WaitingData,"invalid live camera blocks head-only firing");
        vm.m[0][0]=0;enemy_bones[bones::HEAD].position.z=88;
        head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.empty() && head_trigger.trigger_state()==Misc::TriggerState::WaitingHead,"head moved after snapshot cannot trigger from the stale cached pose");
        enemy_bones[bones::HEAD].position.z=94;
        vm.m[0][0]=0;vm.m[1][3]=-95.8f;
        cfg.tb_head_margin=.3f;head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.empty(),"small head margin rejects an edge alignment");
        cfg.tb_head_margin=.8f;head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.size()==1,"wider head margin accepts the same valid head alignment");
        clock_ms=4201;head_trigger.suspend(cfg);
        check(sent.size()==2 && sent.back().mi.dwFlags==MOUSEEVENTF_LEFTUP,"head-only cache gaps immediately release the injected click");
        sent.clear();vm.m[1][3]=-94;cfg.tb_delay_ms=75;clock_ms=4202;head_trigger.update(cache,cfg,&memory,800,600);
        check(sent.empty(),"cache gap restarts the head-only delay");head_trigger.reset();
        // Projection scale (FOV/aspect) must not widen the world-space head region.
        PlayerEnt live;check(cache.refresh_pose(cache.players()[1],live),"head gate live fixture");
        const auto centered=head_gate::test(live,vm,.8f);
        vm.m[0][1]*=2;vm.m[0][3]*=2;vm.m[1][2]*=3;vm.m[1][3]*=3;
        check(centered==head_gate::Result::Inside && head_gate::test(live,vm,.8f)==centered,"head gate is invariant to FOV/aspect projection scales");
        vm.m[3][0]=-1;check(head_gate::test(live,vm,.8f)==head_gate::Result::Outside,"heads behind the camera cannot fire");
        cfg.tb_head_only=false;cfg.tb_head_margin=.8f;cfg.tb_delay_ms=0;vm=saved_camera;
        }
        cache.vm_addr=0;
        held_key=VK_F6; cfg.aim=true; cfg.aim_key=VK_F6; cfg.aim_ads_only=false; cfg.aim_fov=15; cfg.aim_smooth=1; cfg.rcs=false;
        sent.clear(); aim.reset(); aim.update(cache,cfg,&memory); check(!sent.empty(),"aim needs only its configured key by default");
        // Closed-loop response should agree at 2ms and 16ms polling intervals.
        held_key=0; cfg.aim_smooth=5; Vec3 view{}; cache.view_angles_addr=reinterpret_cast<uintptr_t>(&view);
        auto response=[&](ULONGLONG step) {
            aim.reset(); view={}; simulated_view=&view; clock_ms=5000; aim.update(cache,cfg,&memory);
            for (ULONGLONG elapsed=step;elapsed<=160;elapsed+=step) { clock_ms=5000+elapsed; aim.update(cache,cfg,&memory); }
            simulated_view=nullptr; return view.y;
        };
        const float fast=response(2), slow=response(16);
        check(fast>4.f && slow>4.f && std::fabs(fast-slow)<.1f,"aim smoothing is independent of render/poll frequency");
        cache.view_angles_addr=0;
        // Corrupt or incomplete anatomy must never turn a guessed position into a target.
        cfg.aim=true; cfg.aim_key=VK_RBUTTON; cfg.aim_ads_only=false; cfg.aim_smooth=1; cfg.rcs=false; held_key=VK_RBUTTON;
        enemy_bones[bones::HEAD].position={110,30,10000}; cache.update();
        sent.clear(); aim.reset(); aim.update(cache,cfg,&memory);
        check(sent.empty(),"head far above the body cannot move the mouse");
        enemy_bones[bones::HEAD].position={110,30,94}; enemy_bones[bones::HEAD].scale=0; cache.update();
        sent.clear(); aim.reset(); aim.update(cache,cfg,&memory); check(sent.empty(),"zero-scale pose cannot move the mouse");
        enemy_bones[bones::HEAD].scale=1; cache.update(); cfg.aim_bone=1;
        sent.clear(); aim.reset(); aim.update(cache,cfg,&memory); check(sent.empty(),"missing neck is not replaced with a guessed aim point");
        cfg.aim_bone=0; cfg.rcs=true;
        sent.clear(); aim.reset(); aim.update(cache,cfg,&memory);
        check(!sent.empty() && sent.back().mi.dy==0,"right mouse alone cannot apply cached recoil and pull aim upward");
        held_key=0; put(punch,offsets::m_predictableBaseAngle,Vec3{1000,1000,0}); cache.update();
        sent.clear(); aim.reset(); aim.update(cache,cfg,&memory);
        check(!sent.empty() && sent.back().mi.dy==0,"outlier recoil cannot redirect a valid target into the sky");
        cfg.aim=false; sent.clear(); aim.reset(); aim.update(cache,cfg,&memory); check(sent.empty(),"invalid standalone recoil is ignored");
        held_key=0; cfg.rcs=false;
        // Live perspective feedback: a screen-left/up head must receive left/up input,
        // and unchanged camera frames must not accumulate more relative movement.
        Vec3 camera_angles{}; Matrix4x4 camera{};
        const Vec3 eye=cache.local()->eye;
        auto update_camera=[&] {
            const float pitch=camera_angles.x*3.14159265f/180.f,yaw=camera_angles.y*3.14159265f/180.f;
            const Vec3 forward{std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),-std::sin(pitch)};
            const Vec3 right{std::sin(yaw),-std::cos(yaw),0};
            const Vec3 up{std::sin(pitch)*std::cos(yaw),std::sin(pitch)*std::sin(yaw),std::cos(pitch)};
            camera={};
            for (const auto item:{std::pair<int,Vec3>{0,right},{1,up},{3,forward}}) {
                camera.m[item.first][0]=item.second.x; camera.m[item.first][1]=item.second.y; camera.m[item.first][2]=item.second.z;
                camera.m[item.first][3]=-(item.second.x*eye.x+item.second.y*eye.y+item.second.z*eye.z);
            }
        };
        put(punch,offsets::m_predictableBaseAngle,Vec3{});
        enemy_bones[bones::HEAD].position={110,30,104}; cache.update();
        cache.vm_addr=reinterpret_cast<uintptr_t>(&camera); camera_angles={}; update_camera();
        cfg.aim=true; cfg.aim_key=VK_RBUTTON; cfg.aim_bone=0; cfg.aim_fov=15; cfg.aim_smooth=1;
        held_key=VK_RBUTTON; clock_ms=6000; sent.clear(); aim.reset(); simulated_view=&camera_angles;
        aim.update(cache,cfg,&memory);
        check(sent.size()==1 && sent.back().mi.dx<0 && sent.back().mi.dy<0,"projected head left/up receives mouse left/up");
        for (int i=1;i<=20;++i) { clock_ms=6000+i*2; aim.update(cache,cfg,&memory); }
        check(sent.size()==1,"one unprocessed camera frame cannot queue repeated aim corrections");
        const auto original_camera=camera;
        for(int i=0;i<12;++i) {
            camera.m[0][3]+=.2f;camera.m[1][3]-=.1f;camera.m[3][3]+=.01f;
            clock_ms+=2;aim.update(cache,cfg,&memory);
        }
        check(sent.size()==1,"camera translation without rotational feedback cannot queue aim corrections");
        camera=original_camera;
        {
            // A dropped mouse command after a pistol shot must not latch the aim off.
            Aimbot recovery(input);simulated_view=nullptr;sent.clear();
            const auto started=clock_ms;
            recovery.update(cache,cfg,&memory);check(sent.size()==1,"recovery fixture sends first aim correction");
            clock_ms=started+79;recovery.update(cache,cfg,&memory);
            check(sent.size()==1,"bounded feedback wait does not spam an unprocessed frame");
            put(local_pawn,offsets::m_iShotsFired,1);cache.update();
            clock_ms=started+80;recovery.update(cache,cfg,&memory);
            check(sent.size()==2,"aim recovers after a shot without releasing its held key");
            recovery.suspend();clock_ms=started+82;recovery.update(cache,cfg,&memory);
            check(sent.size()==2,"brief data gap does not bypass feedback throttling");
            clock_ms=started+160;recovery.update(cache,cfg,&memory);
            check(sent.size()==3,"repeated dropped feedback never latches the activation key");
            held=false;clock_ms+=1;recovery.update(cache,cfg,&memory);
            check(sent.size()==3,"releasing aim key stops recovery retries");
            held=true;put(local_pawn,offsets::m_iShotsFired,3);cache.update();simulated_view=&camera_angles;
        }
        Esp projection;
        update_camera();
        for (int i=0;i<12;++i) { clock_ms+=16; aim.update(cache,cfg,&memory); update_camera(); }
        const auto centered=projection.w2s(cache.players()[1].head,camera,1000,1000);
        check(centered.vis && std::hypot(centered.x-500,centered.y-500)<1.f,"closed-loop aim converges on the head instead of fleeing it");
        float worst_tracking_error=0;
        for(int i=0;i<120;++i) {
            const float offset=std::sin(i*.08f)*5.f;
            enemy_bones[bones::HEAD].position={110,30+offset,104};
            cache.update(); clock_ms+=16;
            aim.update(cache,cfg,&memory); update_camera();
            const auto tracked=projection.w2s(cache.players()[1].head,camera,1000,1000);
            check(tracked.vis,"moving head stays visible");
            worst_tracking_error=(std::max)(worst_tracking_error,std::hypot(tracked.x-500,tracked.y-500));
        }
        check(worst_tracking_error<1.f,"direct response follows a moving head within one pixel in simulated feedback");
        simulated_view=nullptr;
        // Identical observed bones must yield identical corrections regardless of an
        // uncalibrated velocity/latency estimate. No input is sent to the real game.
        camera_angles={};update_camera();
        put(enemy_pawn,offsets::m_vecAbsVelocity,Vec3{0,1000,0});cache.update();
        sent.clear();aim.reset();clock_ms+=16;aim.update(cache,cfg,&memory);
        check(!sent.empty(),"current bone remains trackable during movement");
        const auto moving_command=sent.back().mi;
        put(enemy_pawn,offsets::m_vecAbsVelocity,Vec3{});cache.update();
        sent.clear();aim.reset();clock_ms+=16;aim.update(cache,cfg,&memory);
        check(!sent.empty() && sent.back().mi.dx==moving_command.dx && sent.back().mi.dy==moving_command.dy,"velocity estimate cannot pull aim away from the current validated head");
        // The diagonal of the drawn cone must agree with target selection.
        camera_angles={};update_camera();cfg.aim_fov=15;clock_ms+=16;
        const float diagonal=std::tan(15.f*fov_geometry::radians)*.70710678f*.999f;
        enemy_bones[bones::HEAD].position={eye.x+100,eye.y+100*diagonal,eye.z+100*diagonal};
        cache.update();sent.clear();aim.reset();aim.update(cache,cfg,&memory);
        check(!sent.empty(),"head inside the FOV diagonal is eligible for targeting");
        float rx=0,ry=0;Matrix4x4 scaled=camera;for(auto& row:scaled.m)for(auto& v:row)v*=3;
        check(fov_geometry::radii(scaled,15,1600,900,rx,ry),"FOV radii support scaled projection matrices");
        const auto edge=projection.w2s(enemy_bones[bones::HEAD].position,scaled,1600,900);
        const float ellipse=std::hypot((edge.x-800)/rx,(edge.y-450)/ry);
        check(ellipse<1 && ellipse>.99f,"projected selection point agrees with the displayed FOV boundary");
        enemy_bones[bones::HEAD].position={eye.x+100,eye.y+100*diagonal*1.01f,eye.z+100*diagonal*1.01f};
        cache.update();sent.clear();aim.reset();clock_ms+=16;aim.update(cache,cfg,&memory);
        check(sent.empty(),"head outside the FOV cone does not receive input");
        enemy_bones[bones::HEAD].position={110,30,104};cache.update();auto incomplete=cache.snapshot();
        put(enemy_scene,offsets::m_modelState+offsets::m_pBoneArray,uintptr_t{0});cache.update();incomplete=cache.snapshot();
        put(enemy_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(enemy_bones));
        sent.clear();aim.reset();clock_ms+=16;aim.update(incomplete,cfg,&memory);
        check(!sent.empty(),"a missing cached pose can be reacquired from current validated bones");
        // Keep the selected living enemy while the activation key remains held.
        enemy_bones[bones::HEAD].position={110,30,94}; cache.vm_addr=0; cache.update();
        cache.vm_addr=reinterpret_cast<uintptr_t>(&camera); camera_angles={}; update_camera();
        clock_ms=7000; sent.clear(); aim.reset(); aim.update(cache,cfg,&memory);
        std::vector<uint8_t> third_scene(0x300); std::array<BoneMat,32> third_bones{};
        put(observer_ctrl,offsets::m_bPawnIsAlive,true); put(observer_ctrl,offsets::m_iTeamNum,uint8_t{3});
        put(observer_ctrl,offsets::m_hPlayerPawn,uint32_t{71}); put(observer_pawn,offsets::m_iTeamNum,uint8_t{3});
        put(observer_pawn,offsets::m_iHealth,100); put(observer_pawn,offsets::m_pGameSceneNode,addr(third_scene));
        put(third_scene,offsets::m_vecAbsOrigin,Vec3{110,20.5f,30});
        third_bones[bones::HEAD].position={110,20.5f,94}; third_bones[bones::HEAD].scale=1;
        put(third_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(third_bones));
        camera_angles={0,1,0}; update_camera(); cache.update(); clock_ms+=16; sent.clear(); aim.update(cache,cfg,&memory);
        check(!sent.empty() && sent.back().mi.dx<0,"aim does not jump to a different closer-to-center enemy mid-hold");
        cfg.aim_target=1;
        put(enemy_scene,offsets::m_vecAbsOrigin,Vec3{510,30,30});enemy_bones[bones::HEAD].position={510,30,94};
        camera_angles.y=1.01f;update_camera();
        cache.update();clock_ms+=16;sent.clear();aim.update(cache,cfg,&memory);
        check(!sent.empty() && sent.back().mi.dx>0,"distance priority releases a far lock for a substantially nearer enemy");
        put(enemy_scene,offsets::m_vecAbsOrigin,Vec3{110,30,30});enemy_bones[bones::HEAD].position={110,30,94};cfg.aim_target=0;
        put(enemy_scene,offsets::m_modelState+offsets::m_pBoneArray,uintptr_t{0});cache.update();
        camera_angles.y=1.1f;update_camera();clock_ms+=16;sent.clear();aim.update(cache,cfg,&memory);
        check(!sent.empty() && sent.back().mi.dx>0,"unavailable old pose cannot block another validated target inside FOV");
        put(enemy_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(enemy_bones));
        put(observer_ctrl,offsets::m_bPawnIsAlive,false); put(observer_ctrl,offsets::m_iTeamNum,uint8_t{2});
        put(observer_ctrl,offsets::m_hPlayerPawn,uint32_t{0}); put(observer_pawn,offsets::m_iHealth,0);
        cache.vm_addr=0; cache.view_angles_addr=0; cache.update(); held_key=0;
        std::vector<uint8_t> bomb(0x2000), globals(0x100);
        uintptr_t bomb_ptr=addr(bomb), globals_ptr=addr(globals);
        cache.c4_list=reinterpret_cast<uintptr_t>(&bomb_ptr); cache.globals_addr=reinterpret_cast<uintptr_t>(&globals_ptr);
        put(bomb,offsets::m_bBombTicking,true); put(bomb,offsets::m_flC4Blow,130.f); put(bomb,offsets::m_flTimerLength,40.f);
        put(globals,0x30,100.f); cache.update();
        check(cache.bomb().planted && cache.bomb().clock_valid && cache.bomb().remaining==30.f,"bomb uses direct pointer and game time");
        put(globals,offsets::GLOBAL_CURTIME,110.f); put(bomb,offsets::m_bBeingDefused,true);
        put(bomb,offsets::m_flDefuseCountDown,118.f); put(bomb,offsets::m_flDefuseLength,10.f);
        BombState live;
        check(cache.current_bomb_state(live) && live.remaining==20.f && live.defuse_remaining==8.f && live.defuse_valid,
            "render reads bomb/defuse game time without waiting for the entity cache");
        check(cache.bomb().remaining==30.f,"fresh bomb read does not mutate a published snapshot");
        cache.update();
        check(cache.bomb().remaining==20.f && cache.bomb().defuse_remaining==8.f,"bomb and defuse countdowns advance together");
        put(globals,offsets::GLOBAL_CURTIME,111.f); put(bomb,offsets::m_bBeingDefused,false);
        check(cache.current_bomb_state(live) && !live.defusing && live.defuse_remaining==0,"cancelled defuse disappears immediately");
        put(bomb,offsets::m_bBeingDefused,true); put(bomb,offsets::m_flDefuseCountDown,116.f); put(bomb,offsets::m_flDefuseLength,5.f);
        check(cache.current_bomb_state(live) && live.defuse_valid && live.defuse_remaining==5.f && live.defuse_length==5.f,"kit duration comes from the game");
        put(bomb,offsets::m_bBombDefused,true);
        check(cache.current_bomb_state(live) && live.defused && !live.planted && !live.defuse_valid,"completed defuse clears active countdowns");
        put(bomb,offsets::m_bBombDefused,false);
        put(bomb,offsets::m_flDefuseCountDown,10000.f);
        check(cache.current_bomb_state(live) && !live.defuse_valid,"invalid defuse deadline is not displayed");
        put(globals,0x30,131.f); cache.update(); check(!cache.bomb().planted,"expired bomb is not kept on radar");
        bomb_ptr=0; cache.update(); check(!cache.bomb().planted && !cache.bomb().clock_valid,"bomb removal clears stale timer");
        const EntityCache snapshot=cache;
        check(snapshot.local() && snapshot.local()!=cache.local(),"published snapshot owns its local-player record");
        system_ptr=0; cache.update(); check(!cache.ready() && cache.players().empty() && !cache.local(),"clears stale data");
        check(snapshot.local() && snapshot.local()->health==100,"reader snapshot survives the next worker update");
        // A moving camera must be read at presentation time instead of using a stale cache matrix.
        Matrix4x4 current{}; current.m[0][0]=.01f; current.m[1][2]=.01f; current.m[3][3]=1;
        Matrix4x4 sampled{}; cache.vm_addr=reinterpret_cast<uintptr_t>(&current);
        check(cache.current_view_matrix(sampled) && sampled.m[0][3]==0,"fresh projection read");
        current.m[0][3]=.25f;
        check(cache.current_view_matrix(sampled) && sampled.m[0][3]==.25f,"camera movement is visible without another entity update");
        Config display;display.display_aspect=3;display.display_fit=1;
        auto image=game_viewport(display,1920,1080);
        check(std::fabs(image.x-240)<.01f && image.y==0 && image.width==1440 && image.height==1080,"4:3 bars use the correct 1440x1080 image in a 1920x1080 window");
        display.display_aspect=2;image=game_viewport(display,1920,1080);
        check(std::fabs(image.x-96)<.01f && image.width==1728,"16:10 pillarbox area is centered");
        display.display_aspect=1;image=game_viewport(display,1920,1200);
        check(std::fabs(image.y-60)<.01f && image.height==1080,"16:9 letterbox area is centered");
        display.display_aspect=3;image=game_viewport(display,1280,1024);
        check(image.y==32 && image.height==960,"4:3 in a 5:4 window uses 32-pixel horizontal bars");
        Matrix4x4 lens{};lens.m[0][1]=-.75f;lens.m[1][2]=1;lens.m[3][0]=1;
        for(int aspect=0;aspect<4;++aspect)for(int fit=0;fit<2;++fit) {
            display.display_aspect=aspect;display.display_fit=fit;
            for(const auto dimensions:{std::pair{1920,1080},std::pair{1920,1200},std::pair{1280,960},std::pair{1280,1024}}) {
                const auto view=game_viewport(display,dimensions.first,dimensions.second);
                const auto mapped=viewport_projection(lens,view,dimensions.first,dimensions.second);
                const auto center=projection.w2s({100,0,0},mapped,dimensions.first,dimensions.second);
                check(center.vis && std::fabs(center.x-dimensions.first*.5f)<.01f && std::fabs(center.y-dimensions.second*.5f)<.01f,"Aspect configuration cannot displace the image center");
                const auto left=projection.w2s({100,100/.75f,0},mapped,dimensions.first,dimensions.second);
                check(std::fabs(left.x-view.x)<.02f,"World projection must reach the configured image edge");
                float fov_x=0,fov_y=0;check(fov_geometry::radii(mapped,15,dimensions.first,dimensions.second,fov_x,fov_y),"FOV uses the viewport projection");
                const float diagonal=std::tan(15.f*fov_geometry::radians)*.70710678f*.99f;
                const auto point=projection.w2s({100,-100*diagonal,100*diagonal},mapped,dimensions.first,dimensions.second);
                const float fraction=std::hypot((point.x-center.x)/fov_x,(point.y-center.y)/fov_y);
                check(std::fabs(fraction-.99f)<.001f,"FOV membership and ESP mapping stay aligned across aspect ratios");
                if(fit==0 || aspect==0)check(std::memcmp(&mapped,&lens,sizeof(lens))==0,"Stretched and automatic modes cannot apply a second aspect correction");
            }
        }
        std::puts("PASS: 32 viewport combinations align image center, projected edge and FOV; stretched mode remains unchanged.");
        PlayerEnt hull; hull.origin={0,0,0}; hull.collision_valid=true; hull.collision_min={-16,-16,0}; hull.collision_max={16,16,72};
        Esp esp;
        const auto standing=esp.project_bounds(hull,sampled,1000,1000);
        check(standing.valid && standing.left<625 && standing.right>625,"box follows the shifted projection and true hull width");
        hull.collision_max.z=54;
        const auto crouched=esp.project_bounds(hull,sampled,1000,1000);
        check(crouched.valid && crouched.bottom-crouched.top<standing.bottom-standing.top,"crouch uses current hull height");
        current.m[3][3]=-1;
        check(!esp.project_bounds(hull,current,1000,1000).valid,"geometry behind camera cannot create an oversized box");
        Matrix4x4 near_plane{};near_plane.m[0][1]=1;near_plane.m[1][2]=1;near_plane.m[3][0]=1;
        hull.origin={10,0,0};
        check(esp.project_bounds(hull,near_plane,1000,1000).valid,"partially visible close player is not discarded when some hull corners are behind the camera");
        hull.origin={-100,0,0};
        check(!esp.project_bounds(hull,near_plane,1000,1000).valid,"fully behind-camera hull remains culled");
        cache.vm_addr=0; check(!cache.current_view_matrix(sampled),"failed matrix read is explicit");
        {
            ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
            ImGui::GetIO().DisplaySize={1920,1080};ImGui::GetIO().DeltaTime=1.f/60;
            unsigned char* atlas_pixels;int atlas_width,atlas_height;
            ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas_pixels,&atlas_width,&atlas_height);
            cache.vm_addr=reinterpret_cast<uintptr_t>(&lens);
            Config hud;hud.aim=true;hud.aim_fov_circle=true;hud.aim_fov=15;hud.display_aspect=3;hud.display_fit=1;
            Esp raster;ImGui::NewFrame();raster.render(cache,hud,ImGui::GetBackgroundDrawList(),1920,1080);ImGui::Render();
            const auto* draw=ImGui::GetDrawData();check(draw->TotalVtxCount>0,"viewport integration renders a FOV outline");
            float minimum=1920,maximum=0;bool clipped=false;
            for(int i=0;i<draw->CmdListsCount;++i){
                for(const auto& command:draw->CmdLists[i]->CmdBuffer)if(command.ElemCount){check(command.ClipRect.x>=239.9f && command.ClipRect.z<=1680.1f,"world overlay cannot draw inside side bars");clipped=true;}
                for(const auto& vertex:draw->CmdLists[i]->VtxBuffer){minimum=(std::min)(minimum,vertex.pos.x);maximum=(std::max)(maximum,vertex.pos.x);}
            }
            float expected_x=0,expected_y=0;
            const auto view=game_viewport(hud,1920,1080);const auto mapped=viewport_projection(lens,view,1920,1080);
            check(fov_geometry::radii(mapped,15,1920,1080,expected_x,expected_y) && std::fabs((maximum-minimum)*.5f-expected_x)<2,"rendered FOV uses the configured game image size");
            check(clipped,"viewport produces a clipped drawing command");
            ImGui::DestroyContext();
        }
        if (argc==3 && std::strcmp(argv[1],"--capture-objective")==0) {
            bomb_ptr=addr(bomb); put(bomb,offsets::m_bBombTicking,true); put(bomb,offsets::m_bBombDefused,false);
            put(bomb,offsets::m_bBeingDefused,true); put(bomb,offsets::m_flC4Blow,130.f);
            put(globals,offsets::GLOBAL_CURTIME,116.2f); put(bomb,offsets::m_flDefuseCountDown,119.6f); put(bomb,offsets::m_flDefuseLength,5.f);
            ImGui::CreateContext(); ImGui::GetIO().IniFilename=nullptr;
            ImGui::GetIO().DisplaySize={640,360}; ImGui::GetIO().DeltaTime=1.f/60;
            ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",18);
            RenderCapture renderer; check(renderer.init(640,360),"objective test renderer");
            Config hud; hud.esp=hud.radar=hud.aim=hud.crosshair=hud.spectator_list=hud.item_esp=false; hud.bomb_timer=true;
            renderer.begin(); ImGui::NewFrame(); esp.render(cache,hud,ImGui::GetBackgroundDrawList(),640,360); ImGui::Render();
            check(renderer.save(std::filesystem::path(argv[2])),"objective panel PNG");
            renderer.shutdown(); ImGui::DestroyContext();
        }
        memory.detach(); std::puts("PASS: handles, chunks, bones, weapons, observers, aim direction/FOV/delay, standalone RCS, trigger deadlines/release, no-flash restoration, stale-data reset.");
    } catch (const std::exception& error) { memory.detach(); std::fprintf(stderr,"FAIL: %s\n",error.what()); return 1; }
}
