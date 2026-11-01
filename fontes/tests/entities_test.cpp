#include "entities.hpp"
#include "aimbot.hpp"
#include "misc.hpp"
#include "esp.hpp"
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
        check(cache.local()->weapon_id==7 && cache.local()->clip==30 && cache.local()->reserve==90,"active weapon service and nested item");
        check(cache.entity_handle(UINT32_MAX)==0 && cache.entity_handle(0)==0,"invalid handles");
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
        put(binding,0,addr(model)); put(local_scene,offsets::m_modelState+offsets::m_hModel,addr(binding));
        remapped_bones[7+32].position={10,20,94};
        remapped_bones[7+32].scale=1;
        put(local_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(remapped_bones)); cache.update();
        check(cache.local()->rig.named && cache.local()->rig.target(0)==39 && cache.local()->head.z==94,"live bone array follows model-specific names");
        put(local_scene,offsets::m_modelState+offsets::m_hModel,uintptr_t{0});
        put(local_scene,offsets::m_modelState+offsets::m_pBoneArray,addr(local_bones)); cache.update();
        // Exercise input behavior with a recording sink: nothing reaches the real mouse.
        const InputApi input{record_input,key_state,test_clock};
        enemy_bones[bones::HEAD].position={110,30,94};
        put(enemy_scene,offsets::m_vecAbsOrigin,Vec3{110,30,30}); cache.update();
        Config cfg; cfg.aim=true; cfg.aim_ads_only=false; cfg.aim_fov=15; cfg.aim_smooth=1; cfg.aim_delay_ms=100;
        Aimbot aim(input); aim.update(cache,cfg,&memory);
        check(sent.empty(),"aim waits without sleeping");
        clock_ms=1099; aim.update(cache,cfg,&memory); check(sent.empty(),"aim acquisition delay respected");
        clock_ms=1100; aim.update(cache,cfg,&memory);
        check(sent.size()==1 && sent.back().mi.dx<0 && sent.back().mi.dwFlags==MOUSEEVENTF_MOVE,"Source yaw maps to negative mouse X");
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
        Misc misc(input); cfg.rcs=false; cfg.triggerbot=true; cfg.tb_delay_ms=75; cfg.tb_release_ms=20;
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
        PlayerEnt hull; hull.origin={0,0,0}; hull.collision_valid=true; hull.collision_min={-16,-16,0}; hull.collision_max={16,16,72};
        Esp esp;
        const auto standing=esp.project_bounds(hull,sampled,1000,1000);
        check(standing.valid && standing.left<625 && standing.right>625,"box follows the shifted projection and true hull width");
        hull.collision_max.z=54;
        const auto crouched=esp.project_bounds(hull,sampled,1000,1000);
        check(crouched.valid && crouched.bottom-crouched.top<standing.bottom-standing.top,"crouch uses current hull height");
        current.m[3][3]=-1;
        check(!esp.project_bounds(hull,current,1000,1000).valid,"geometry behind camera cannot create an oversized box");
        cache.vm_addr=0; check(!cache.current_view_matrix(sampled),"failed matrix read is explicit");
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
