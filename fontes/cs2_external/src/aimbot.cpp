#include "aimbot.hpp"
#include "aim_math.hpp"
#include "fov_geometry.hpp"
#include <algorithm>
#include <limits>
#include <cstring>

static constexpr float PI = 3.14159265f;
float Aimbot::normalize(float a) const { return std::isfinite(a) ? std::remainder(a, 360.f) : 0.f; }
Vec3 Aimbot::calc_angle(const Vec3& src, const Vec3& dst) const {
    const Vec3 d = dst - src;
    return {-std::atan2(d.z, std::hypot(d.x, d.y)) * 180.f / PI, std::atan2(d.y, d.x) * 180.f / PI, 0};
}
bool Aimbot::get_bone_pos(const PlayerEnt& p,int bone,Vec3& point) const {
    return p.aim_point(std::clamp(bone,0,2),point);
}
bool Aimbot::send_mouse(float dpitch,float dyaw,float sens,const Vec3* remaining) {
    if (!std::isfinite(dpitch) || !std::isfinite(dyaw) || !std::isfinite(sens) || sens<=0) return false;
    LONG mx=0,my=0;
    if(remaining) {
        mx=aim_math::bounded_count(-dyaw/(.022f*sens),-remaining->y/(.022f*sens),mouse_x_);
        my=aim_math::bounded_count(dpitch/(.022f*sens),remaining->x/(.022f*sens),mouse_y_);
    } else {
        mouse_x_+=-dyaw/(.022f*sens);mouse_y_+=dpitch/(.022f*sens);
        mx=static_cast<LONG>(std::clamp(mouse_x_,-32767.f,32767.f));
        my=static_cast<LONG>(std::clamp(mouse_y_,-32767.f,32767.f));
        mouse_x_-=mx;mouse_y_-=my;
    }
    if (!mx && !my) return false;
    INPUT input{}; input.type=INPUT_MOUSE; input.mi.dx=mx; input.mi.dy=my; input.mi.dwFlags=MOUSEEVENTF_MOVE;
    if (input_.send(1,&input,sizeof(input))==1) { return true; }
    mouse_x_=mouse_y_=0; return false;
}
void Aimbot::update(const EntityCache& cache,const Config& cfg,Memory*) {
    const auto* local=cache.local();
    if (!local || !local->valid() || !local->eye.finite()) { reset(); return; }
    const bool held=cfg.aim && (input_.key(cfg.aim_key)&0x8000) && (!cfg.aim_ads_only || (input_.key(VK_RBUTTON)&0x8000));
    const bool recoil_active=cfg.rcs && local->shots_fired>1 && (input_.key(VK_LBUTTON)&0x8000) &&
        local->punch_angle.finite() && std::fabs(local->punch_angle.x)<=25 && std::fabs(local->punch_angle.y)<=25 && std::fabs(local->punch_angle.z)<=5;
    if (!held && !recoil_active) { reset(); return; }
    int live_team=0;
    if(!cache.current_local_team(live_team)){reset();return;}
    if(held && cfg.aim_visibility && !cache.visibility_ready()){reset();return;}
    const float sensitivity=cache.sensitivity()*(cfg.aim_auto_sens?1.f:cfg.aim_sens);
    Matrix4x4 camera{}; const bool projected=cache.vm_addr!=0;
    Vec3 view=local->eye_angles;
    float focal_x=0,focal_y=0;
    if (projected) {
        if (!cache.current_view_matrix(camera)) { suspend(); return; }
        if(!fov_geometry::focal(camera,focal_x,focal_y)){suspend();return;}
    } else {
        if (cache.view_angles_addr && !cache.current_view_angles(view)) { reset(); return; }
        if (!view.finite() || std::fabs(view.x)>89.5f || std::fabs(view.y)>36000 || std::fabs(view.z)>10) { reset(); return; }
    }
    const auto now=input_.now();
    if (awaiting_feedback_ && projected==last_projected_) {
        // Translation and FOV changes are not acknowledgement of mouse rotation.
        // Compare the normalized camera orientation, excluding its position column.
        auto same_orientation=[&] {
            for(const int row : {0,1,3}) {
                float a=0,b=0;
                for(int col=0;col<3;++col) {a+=camera.m[row][col]*camera.m[row][col];b+=last_command_camera_.m[row][col]*last_command_camera_.m[row][col];}
                a=std::sqrt(a);b=std::sqrt(b);
                if(a<.0001f || b<.0001f)return false;
                for(int col=0;col<3;++col)if(std::fabs(camera.m[row][col]/a-last_command_camera_.m[row][col]/b)>.000001f)return false;
            }
            return true;
        };
        const bool unchanged=projected ? same_orientation() :
            std::fabs(normalize(view.x-last_command_view_.x))<.0001f && std::fabs(normalize(view.y-last_command_view_.y))<.0001f;
        // Wait for a frame acknowledgement, but recover from a dropped command
        // without making the user release the activation key.
        if (unchanged && now-command_sent_at_<feedback_timeout_ms) return;
        if (unchanged) { mouse_x_=mouse_y_=0; last_update_=0; }
    }
    awaiting_feedback_=false;
    const float dt=last_update_ ? std::clamp((now-last_update_)/1000.f,.001f,.05f) : 1.f/60.f;
    last_update_=now;
    auto error_to=[&](const Vec3& point,Vec3& error) {
        if (projected) {
            const auto dot=[&](const float row[4]) { return row[0]*point.x+row[1]*point.y+row[2]*point.z+row[3]; };
            const float w=dot(camera.m[3]);
            if (!std::isfinite(w) || w<=.01f) return false;
            error={-std::atan(dot(camera.m[1])/(w*focal_y))*180.f/PI,
                   -std::atan(dot(camera.m[0])/(w*focal_x))*180.f/PI,0};
        } else {
            const auto angle=calc_angle(local->eye,point);
            error={normalize(angle.x-view.x),normalize(angle.y-view.y),0};
        }
        return error.finite();
    };
    const PlayerEnt* best=nullptr; const PlayerEnt* locked=nullptr;
    const PlayerEnt* pending_pose=nullptr;
    PlayerEnt recovered;bool attempted_recovery=false;
    float score=(std::numeric_limits<float>::max)();
    if (held) for (const auto& cached:cache.players()) {
        const PlayerEnt* candidate_player=&cached;
        if(!cached.bones_valid && cached.valid() && !cached.is_local && cached.team!=local->team && !attempted_recovery){
            attempted_recovery=true;if(cache.refresh_pose(cached,recovered))candidate_player=&recovered;
        }
        const auto& player=*candidate_player;
        if (player.is_local || !player.valid() || player.team==local->team) continue;
        Vec3 point{},error{};
        if (!get_bone_pos(player,cfg.aim_bone,point)) {
            if(player.pawn==target_ && player.ctrl==target_ctrl_ && player.pawn_handle==target_handle_)pending_pose=&player;
            continue;
        }
        if(!error_to(point,error))continue;
        const float distance=(point-local->eye).len();
        if(distance*.01905f>cfg.aim_max_dist)continue;
        const float fov=projected?fov_geometry::cone_angle(error):std::hypot(error.x,error.y);
        if (fov>cfg.aim_fov+.0001f) continue;
        if(cfg.aim_visibility && !cache.clear_path(local->eye,point))continue;
        if (player.pawn==target_ && player.ctrl==target_ctrl_ && player.pawn_handle==target_handle_) locked=&player;
        const float candidate=cfg.aim_target==0 ? fov : (point-local->eye).len();
        if (candidate<score) { score=candidate; best=&player; }
    }
    if(pending_pose && !best && target_seen_ && now-target_seen_<=100){suspend();return;}
    if(locked && cfg.aim_target==0)best=locked;
    else if (locked && best) {
        Vec3 point{},error{};
        if(get_bone_pos(*locked,cfg.aim_bone,point) && error_to(point,error)){
            const float locked_score=cfg.aim_target==0?(projected?fov_geometry::cone_angle(error):std::hypot(error.x,error.y)):(point-local->eye).len();
            if(locked_score<=score*1.15f)best=locked;
        }
    }
    bool applied=false,moved=false;
    if (best) {
        if (target_!=best->pawn || target_ctrl_!=best->ctrl || target_handle_!=best->pawn_handle) {
            target_=best->pawn; target_ctrl_=best->ctrl; target_handle_=best->pawn_handle;target_since_=now; mouse_x_=mouse_y_=0;
        }
        target_seen_=now;
        if (now-target_since_>=static_cast<ULONGLONG>(cfg.aim_delay_ms)) {
            PlayerEnt fresh;
            const PlayerEnt* selected=best;
            if(best->scene && best!=&recovered){if(!cache.refresh_pose(*best,fresh)){suspend();return;}selected=&fresh;}
            if(!cache.current_local_team(live_team) || !cache.current_enemy(*selected,live_team)){reset();return;}
            Vec3 point{},error{};
            if (!get_bone_pos(*selected,cfg.aim_bone,point)) {reset();return;}
            if((point-local->eye).len()*.01905f>cfg.aim_max_dist || (cfg.aim_visibility && !cache.clear_path(local->eye,point))){reset();return;}
            // Follow the current validated bone; do not invent an input-latency lead.
            if(!error_to(point,error)){reset();return;}
            if((projected?fov_geometry::cone_angle(error):std::hypot(error.x,error.y))>cfg.aim_fov+.0001f){reset();return;}
            // The live camera projection already includes visible recoil; do not compensate it twice.
            if (!projected && recoil_active) {
                error.x-=local->punch_angle.x*2.f*cfg.rcs_intensity;
                error.y-=local->punch_angle.y*2.f*cfg.rcs_intensity;
            }
            const float smoothing=(std::max)(1.f,cfg.aim_smooth);
            const float blend=smoothing<=1 ? 1.f : 1.f-std::pow(1.f-1.f/smoothing,dt*60.f);
            float pitch=error.x*blend,yaw=error.y*blend;
            const float magnitude=std::hypot(pitch,yaw),limit=360.f*dt;
            if (magnitude>limit) { pitch*=limit/magnitude; yaw*=limit/magnitude; }
            if (std::hypot(error.x,error.y)>.011f*sensitivity) moved=send_mouse(pitch,yaw,sensitivity,&error);
            else mouse_x_=mouse_y_=0;
            applied=true;
        }
    } else { target_=target_ctrl_=0;target_handle_=0;target_seen_=target_since_=0; }
    if (recoil_active) {
        const auto punch=local->punch_angle*(2.f*cfg.rcs_intensity);
        const auto delta=punch-prev_punch_;
        if (!applied && delta.finite() && std::hypot(delta.x,delta.y)<=12.f) moved=send_mouse(-delta.x,-delta.y,sensitivity)||moved;
        prev_punch_=punch;
    } else prev_punch_={};
    if (moved) {
        command_sent_at_=now; awaiting_feedback_=true; last_projected_=projected; last_command_camera_=camera; last_command_view_=view;
    }
}
