from pathlib import Path
p=Path(__file__).parent.parent/'fontes/cs2_external/src/aimbot.cpp'
s=p.read_text(encoding='utf-8')
s=s.replace('#include <limits>','#include <limits>\n#include <cstring>')
start=s.index('void Aimbot::send_mouse(')
s=s[:start]+r'''bool Aimbot::send_mouse(float dpitch,float dyaw,float sens) {
    if (!std::isfinite(dpitch) || !std::isfinite(dyaw) || !std::isfinite(sens) || sens<=0) return false;
    mouse_x_+=-dyaw/(.022f*sens); mouse_y_+=dpitch/(.022f*sens);
    const LONG mx=static_cast<LONG>(std::clamp(mouse_x_,-32767.f,32767.f));
    const LONG my=static_cast<LONG>(std::clamp(mouse_y_,-32767.f,32767.f));
    if (!mx && !my) return false;
    INPUT input{}; input.type=INPUT_MOUSE; input.mi.dx=mx; input.mi.dy=my; input.mi.dwFlags=MOUSEEVENTF_MOVE;
    if (input_.send(1,&input,sizeof(input))==1) { mouse_x_-=mx; mouse_y_-=my; return true; }
    mouse_x_=mouse_y_=0; return false;
}
void Aimbot::update(const EntityCache& cache,const Config& cfg,Memory*) {
    const auto* local=cache.local();
    if (!local || !local->valid() || !local->eye.finite()) { reset(); return; }
    const bool held=cfg.aim && (input_.key(cfg.aim_key)&0x8000) && (!cfg.aim_ads_only || (input_.key(VK_RBUTTON)&0x8000));
    const bool recoil_active=cfg.rcs && local->shots_fired>1 && (input_.key(VK_LBUTTON)&0x8000) &&
        local->punch_angle.finite() && std::fabs(local->punch_angle.x)<=25 && std::fabs(local->punch_angle.y)<=25 && std::fabs(local->punch_angle.z)<=5;
    if (!held && !recoil_active) { reset(); return; }
    const float sensitivity=cache.sensitivity()*cfg.aim_sens;
    Matrix4x4 camera{}; const bool projected=cache.vm_addr!=0;
    Vec3 view=local->eye_angles;
    float focal_x=0,focal_y=0;
    if (projected) {
        if (!cache.current_view_matrix(camera)) { reset(); return; }
        const auto length=[](const float row[4]) { return std::sqrt(row[0]*row[0]+row[1]*row[1]+row[2]*row[2]); };
        const float forward=length(camera.m[3]);
        if (forward<.001f) { reset(); return; }
        focal_x=length(camera.m[0])/forward; focal_y=length(camera.m[1])/forward;
        if (!std::isfinite(focal_x) || !std::isfinite(focal_y) || focal_x<.001f || focal_y<.001f) { reset(); return; }
    } else {
        if (cache.view_angles_addr && !cache.current_view_angles(view)) { reset(); return; }
        if (!view.finite() || std::fabs(view.x)>89.5f || std::fabs(view.y)>36000 || std::fabs(view.z)>10) { reset(); return; }
    }
    if (awaiting_feedback_ && projected==last_projected_) {
        const bool unchanged=projected ? std::memcmp(&camera,&last_command_camera_,sizeof(camera))==0 :
            std::fabs(normalize(view.x-last_command_view_.x))<.0001f && std::fabs(normalize(view.y-last_command_view_.y))<.0001f;
        if (unchanged) return; // Do not queue repeated corrections against the same unprocessed frame.
    }
    awaiting_feedback_=false;
    const auto now=input_.now();
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
    float score=(std::numeric_limits<float>::max)();
    if (held) for (const auto& player:cache.players()) {
        if (player.is_local || !player.valid() || player.team==local->team) continue;
        Vec3 point{},error{};
        if (!get_bone_pos(player,cfg.aim_bone,point) || !error_to(point,error)) continue;
        const float fov=std::hypot(error.x,error.y);
        if (fov>cfg.aim_fov) continue;
        if (player.pawn==target_ && player.ctrl==target_ctrl_) locked=&player;
        const float candidate=cfg.aim_target==0 ? fov : (point-local->eye).len();
        if (candidate<score) { score=candidate; best=&player; }
    }
    if (locked) best=locked;
    bool applied=false,moved=false;
    if (best) {
        if (target_!=best->pawn || target_ctrl_!=best->ctrl) {
            target_=best->pawn; target_ctrl_=best->ctrl; target_since_=now; mouse_x_=mouse_y_=0;
        }
        if (now-target_since_>=static_cast<ULONGLONG>(cfg.aim_delay_ms)) {
            Vec3 point{},error{};
            if (!get_bone_pos(*best,cfg.aim_bone,point) || !error_to(point,error)) { reset(); return; }
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
            if (std::hypot(error.x,error.y)>.011f*sensitivity) moved=send_mouse(pitch,yaw,sensitivity);
            else mouse_x_=mouse_y_=0;
            applied=true;
        }
    } else { target_=target_ctrl_=0; target_since_=0; }
    if (recoil_active) {
        const auto punch=local->punch_angle*(2.f*cfg.rcs_intensity);
        const auto delta=punch-prev_punch_;
        if (!applied && delta.finite() && std::hypot(delta.x,delta.y)<=12.f) moved=send_mouse(-delta.x,-delta.y,sensitivity)||moved;
        prev_punch_=punch;
    } else prev_punch_={};
    if (moved) {
        awaiting_feedback_=true; last_projected_=projected; last_command_camera_=camera; last_command_view_=view;
    }
}
'''
p.write_text(s,encoding='utf-8')
