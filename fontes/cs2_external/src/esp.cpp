// language: C++20 / file: esp.cpp / runtime: MSVC + ImGui / target: Windows x64
#include "esp.hpp"
#include "fov_geometry.hpp"
#include "game_viewport.hpp"
#include "input.hpp"
#include "imgui.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <limits>

// ─── World → Screen ─────────────────────────────────────────────────────────
Scr Esp::w2s(const Vec3& w, const Matrix4x4& vm, int sw, int sh) const {
    // Column-major view-projection (Source 2 standard)
    float clip_w = vm.m[3][0]*w.x + vm.m[3][1]*w.y + vm.m[3][2]*w.z + vm.m[3][3];
    if (!w.finite() || !std::isfinite(clip_w) || clip_w < 0.01f) return {-9999.f, -9999.f, false};
    float clip_x = vm.m[0][0]*w.x + vm.m[0][1]*w.y + vm.m[0][2]*w.z + vm.m[0][3];
    float clip_y = vm.m[1][0]*w.x + vm.m[1][1]*w.y + vm.m[1][2]*w.z + vm.m[1][3];
    if (!std::isfinite(clip_x) || !std::isfinite(clip_y)) return {-9999.f, -9999.f, false};
    return {
        (sw * 0.5f) * (1.f + clip_x / clip_w),
        (sh * 0.5f) * (1.f - clip_y / clip_w),
        true
    };
}

ScreenBounds Esp::project_bounds(const PlayerEnt& p,const Matrix4x4& vm,int sw,int sh) const {
    ScreenBounds result;
    if (sw<=0 || sh<=0) return result;
    result.left=result.top=(std::numeric_limits<float>::max)();
    result.right=result.bottom=-(std::numeric_limits<float>::max)();
    int count=0;
    auto include=[&](const Vec3& point) {
        const auto projected=w2s(point,vm,sw,sh);
        if (!projected.vis) return false;
        const float x=std::clamp(projected.x,-8.f,float(sw)+8.f),y=std::clamp(projected.y,-8.f,float(sh)+8.f);
        result.left=(std::min)(result.left,x);result.right=(std::max)(result.right,x);
        result.top=(std::min)(result.top,y);result.bottom=(std::max)(result.bottom,y);
        ++count; return true;
    };
    if (p.bones_valid) for (const int index:p.rig.joint) {
        if (index<0 || index>=static_cast<int>(p.bone.size())) continue;
        Vec3 point{};if(p.bone_point(index,point))include(point);
    }
    // Prefer the rendered pose. Collision hull is a fallback for missing rigs.
    if(count<6 && p.collision_valid) {
        for(int i=0;i<8;++i) {
            const Vec3 corner{(i&1)?p.collision_max.x:p.collision_min.x,
                (i&2)?p.collision_max.y:p.collision_min.y,(i&4)?p.collision_max.z:p.collision_min.z};
            include(p.origin+corner);
        }
    }
    if (count<2 || result.right-result.left<1 || result.bottom-result.top<1 ||
        result.right-result.left>sw*2.f || result.bottom-result.top>sh*2.f) return {};
    if(result.right<0 || result.left>sw || result.bottom<0 || result.top>sh)return {};
    const float margin=std::clamp((result.bottom-result.top)*.035f,1.f,6.f);
    result.left-=margin;result.right+=margin;result.top-=margin;result.bottom+=margin;result.valid=true;
    return result;
}

// ─── Main render ────────────────────────────────────────────────────────────
void Esp::render(const EntityCache& cache, const Config& cfg,
                 ImDrawList* dl, int sw, int sh) {
    const auto now=input_clock_ms(),age=now-cache.sampled_at();
    const bool changed_map=!cache.map_name().empty() && !visual_map_.empty() && visual_map_!=cache.map_name();
    if(changed_map || camera_width_!=sw || camera_height_!=sh) {
        pose_history_.clear();camera_at_=0;visual_team_=0;camera_width_=sw;camera_height_=sh;
    }
    if(!cache.map_name().empty())visual_map_=cache.map_name();
    const auto* current_local=cache.local();
    uintptr_t observed_pawn=0;
    const bool camera_subject_known=!cfg.esp || cache.current_observer_target(observed_pawn);
    if(current_local && (current_local->team==2 || current_local->team==3))visual_team_=current_local->team;
    frame_players_.clear();
    if(cfg.esp && age<=VISUAL_GRACE_MS) {
        if(frame_players_.capacity()<64)frame_players_.reserve(64);
        if(pose_history_.empty())pose_history_.resize(65);
        std::array<bool,65> seen{};
        auto append=[&](const PlayerEnt& source,bool validate_live) {
            if(!source.pose_at || now<source.pose_at || now-source.pose_at>VISUAL_GRACE_MS || source.is_local)return;
            if(cfg.esp_filter!=2 && (!visual_team_ || (cfg.esp_filter==0 && source.team==visual_team_) || (cfg.esp_filter==1 && source.team!=visual_team_)))return;
            if(current_local && cfg.esp_max_dist>0 && (source.origin-current_local->origin).len()*.01905f>cfg.esp_max_dist)return;
            PlayerEnt pose;
            if(cache.visual_pose(source,pose,now-source.pose_at,validate_live))frame_players_.push_back(std::move(pose));
        };
        for(const auto& source:cache.players()) {
            if(source.controller_index<1 || source.controller_index>64)continue;
            seen[source.controller_index]=true;
            auto& history=pose_history_[source.controller_index];
            if(!source.valid()) {
                if(source.state_valid)history={};else append(history,true);
                continue;
            }
            if(source.is_local)continue;
            if(history.pawn!=source.pawn || history.pawn_handle!=source.pawn_handle || history.ctrl!=source.ctrl ||
               (source.scene && history.scene!=source.scene) || (source.model_binding && history.model_binding!=source.model_binding) || (source.model_info && history.model_info!=source.model_info) || history.team!=source.team)history={};
            if(source.bones_valid && source.pose_at && source.pose_at>=history.pose_at &&
               (!cfg.esp_body || source.model_info || !history.model_info))history=source;
            if(history.pose_at)append(history,!source.bones_valid);
            else if(source.state_valid && source.collision_valid) {
                PlayerEnt hull=source;hull.pose_at=cache.sampled_at();append(hull,true);
            }
        }
        // A controller omitted by a transient read can retain a visual only after
        // its current identity and life state are checked again. Never used by aim.
        for(size_t i=1;i<seen.size();++i)if(!seen[i])append(pose_history_[i],true);
    } else pose_history_.clear();
    // One camera sample after the poses. Reuse only a bounded, previously validated
    // camera if a read overlaps the game's own matrix update.
    Matrix4x4 vm{};
    const bool needs_camera=cfg.esp || cfg.item_esp || (cfg.aim && cfg.aim_fov_circle);
    bool matrix_valid=needs_camera && cache.current_view_matrix(vm);
    if(matrix_valid){last_camera_=vm;camera_at_=now;}
    else if(camera_at_ && now-camera_at_<=80){vm=last_camera_;matrix_valid=true;}
    const auto area=game_viewport(cfg,sw,sh);
    if(matrix_valid)vm=viewport_projection(vm,area,sw,sh);
    if(cfg.esp && matrix_valid) {
        dl->PushClipRect({area.x,area.y},{area.right(),area.bottom()},true);
        const auto* local=cache.local();
        for(const auto& p:frame_players_) {
            if (p.is_local || (observed_pawn && p.pawn==observed_pawn)) continue;
            if (!p.valid())  continue;

            // Filter: enemies / allies / all
            if (local) {
                bool is_enemy = p.team != local->team;
                if (cfg.esp_filter == 0 && !is_enemy) continue;
                if (cfg.esp_filter == 1 &&  is_enemy) continue;
            }

            // Distance in meters
            float dist_m = 0.f;
            if (local) {
                Vec3 d = p.origin - local->origin;
                dist_m = d.len() * 0.01905f;
            }
            if (cfg.esp_max_dist > 0.f && dist_m > cfg.esp_max_dist) continue;

            const auto bounds=project_bounds(p,vm,sw,sh);
            if(!bounds.valid || bounds.right<area.x || bounds.left>area.right() || bounds.bottom<area.y || bounds.top>area.bottom())continue;
            const Scr feet{(bounds.left+bounds.right)*.5f,bounds.bottom,true};

            // Vis-check color selection
            ImU32 col;
            if (cfg.esp_vischeck) {
                // The network spotted mask is distinct from an immediate line-of-sight trace.
                bool visible = local && local->controller_index >= 1 && local->controller_index <= 64 &&
                    (p.spotted_by & (1ULL << (local->controller_index - 1)));
                col = visible ? cfg.col_visible
                    : (p.team == 2 ? cfg.col_tr : cfg.col_ct);
            } else {
                col = (p.team == 2) ? cfg.col_tr : cfg.col_ct;
            }

            if (cfg.esp_body && camera_subject_known) {
                if(cfg.esp_player_style==1)draw_skeleton(dl,p,vm,sw,sh,cfg.col_skeleton,cfg.esp_skel_thick);
                else neon_.draw(dl,p,vm,sw,sh,cfg.col_body);
            }
            if (cfg.esp_box)       draw_box(dl,bounds,col,cfg.esp_box_thick,cfg.esp_box_style);
            if (cfg.esp_healthbar) draw_healthbar(dl,bounds,p.health,cfg.esp_health_num);
            if (cfg.esp_snaplines) draw_snapline(dl, feet, sw, sh, cfg.col_snapline);
            if (cfg.esp_distance || cfg.esp_weapon)
                draw_text_info(dl,p,bounds,cfg,dist_m);
        }
        dl->PopClipRect();
    }

    if (cfg.radar)   draw_radar(dl, cache, cfg, sw, sh);
    if (cfg.crosshair) draw_crosshair(dl, cfg, sw, sh);
    if(matrix_valid && cfg.aim && cfg.aim_fov_circle){dl->PushClipRect({area.x,area.y},{area.right(),area.bottom()},true);draw_fov_circle(dl,cfg.aim_fov,vm,sw,sh);dl->PopClipRect();}
    if (cfg.bomb_timer) { BombState bomb; if (cache.current_bomb_state(bomb)) draw_bomb_timer(dl,bomb,cfg,sw,sh); }
    if (cfg.spectator_list) draw_spec_list(dl, cache, sw, sh);
    if (cfg.triggerbot && !cfg.menu_open)
        dl->AddText({16.f,sh-34.f},IM_COL32(220,225,235,220),cfg.trigger_status.c_str());
    if(matrix_valid && cfg.item_esp) {
    dl->PushClipRect({area.x,area.y},{area.right(),area.bottom()},true);
    for (const auto& item : cache.items()) {
        if (cache.local() && (item.origin - cache.local()->origin).len() * .01905f > cfg.esp_max_dist) continue;
        const auto screen = w2s(item.origin,vm,sw,sh);
        if (screen.vis) dl->AddText({screen.x, screen.y}, IM_COL32(230,210,120,230), weapons::name(item.weapon_id));
    }
    dl->PopClipRect();
    }
}

// ─── Box ────────────────────────────────────────────────────────────────────
void Esp::draw_box(ImDrawList* dl, const ScreenBounds& bounds,
                   ImU32 col, float thick, int style) {
    const float box_h=bounds.bottom-bounds.top,box_w=bounds.right-bounds.left;
    const float x1=bounds.left,y1=bounds.top,x2=bounds.right,y2=bounds.bottom;

    switch (style) {
    case 0: // outline
        dl->AddRect({x1,y1},{x2,y2}, IM_COL32(0,0,0,180), 0.f, 0, thick+1.f);
        dl->AddRect({x1,y1},{x2,y2}, col, 0.f, 0, thick);
        break;
    case 1: { // corner
        float cw = box_w * 0.25f, ch = box_h * 0.2f;
        auto C = [&](float ax, float bx, float ay, float by) {
            dl->AddLine({ax,ay},{bx,ay}, col, thick);
            dl->AddLine({ax,ay},{ax,by}, col, thick);
        };
        C(x1,x1+cw, y1,y1+ch); C(x2,x2-cw, y1,y1+ch);
        C(x1,x1+cw, y2,y2-ch); C(x2,x2-cw, y2,y2-ch);
        break;
    }
    case 2: // filled
        dl->AddRectFilled({x1,y1},{x2,y2}, (col & 0x00FFFFFF) | IM_COL32(0,0,0,64));
        dl->AddRect({x1,y1},{x2,y2}, col, 0.f, 0, thick);
        break;
    }
}

// ─── Skeleton ────────────────────────────────────────────────────────────────
void Esp::draw_skeleton(ImDrawList* dl, const PlayerEnt& p, const Matrix4x4& vm,
                         int sw, int sh, ImU32 col, float thick) {
    if (!p.bones_valid) return;
    for (const auto& edge : bones::CONNECTIONS) {
        const int a=p.rig.joint[edge[0]], b=p.rig.joint[edge[1]];
        if (a<0 || b<0 || a>=static_cast<int>(p.bone.size()) || b>=static_cast<int>(p.bone.size())) continue;
        Vec3 pa{},pb{};
        if (!p.bone_point(a,pa) || !p.bone_point(b,pb) || (pa-pb).len()>80.f || (pa-pb).len()<.01f) continue;
        Scr sa = w2s(pa, vm, sw, sh), sb = w2s(pb, vm, sw, sh);
        if (!sa.vis || !sb.vis) continue;
        dl->AddLine({sa.x,sa.y},{sb.x,sb.y}, col, thick);
    }
}

// ─── Health Bar ──────────────────────────────────────────────────────────────
void Esp::draw_healthbar(ImDrawList* dl, const ScreenBounds& bounds,
                          int hp, bool show_num) {
    hp = std::clamp(hp, 0, 100);
    const float box_h=bounds.bottom-bounds.top;
    const float x1=bounds.left-6.f,y1=bounds.top,y2=bounds.bottom;
    float bar_h = box_h * (hp / 100.f);

    dl->AddRectFilled({x1-1.f, y1-1.f},{x1+4.f, y2+1.f}, IM_COL32(0,0,0,160));
    // Gradient: green→yellow→red based on hp
    uint8_t r = hp > 50 ? uint8_t(255 * (1.f - (hp-50)/50.f)) : 255;
    uint8_t g = hp < 50 ? uint8_t(255 * (hp/50.f)) : 255;
    dl->AddRectFilled({x1, y2-bar_h},{x1+4.f, y2}, IM_COL32(r,g,0,220));

    if (show_num && hp < 100) {
        char buf[8]; sprintf_s(buf, "%d", hp);
        dl->AddText({x1-2.f, y2-bar_h-10.f}, IM_COL32(255,255,255,200), buf);
    }
}

// ─── Snap Line ───────────────────────────────────────────────────────────────
void Esp::draw_snapline(ImDrawList* dl, const Scr& feet, int sw, int sh, ImU32 col) {
    dl->AddLine({float(sw)*0.5f, float(sh)}, {feet.x, feet.y}, col, 1.f);
}

// ─── Text Info ───────────────────────────────────────────────────────────────
void Esp::draw_text_info(ImDrawList* dl, const PlayerEnt& p,
                          const ScreenBounds& bounds, const Config& cfg, float distance_m) {
    const float cx=(bounds.left+bounds.right)*.5f;
    float ytop=bounds.top-14.f;

    if (p.name[0]) {
        dl->AddText({cx - ImGui::CalcTextSize(p.name).x * 0.5f, ytop - 13.f},
                    IM_COL32(255,255,255,230), p.name);
        ytop -= 13.f;
    }

    if (p.is_local) return;

    // Weapon + ammo below feet
    float ybot=bounds.bottom+3.f;
    if (cfg.esp_distance) {
        char dbuf[32]{};
        sprintf_s(dbuf, "%.0f m", distance_m);
        dl->AddText({cx - ImGui::CalcTextSize(dbuf).x * 0.5f, ybot},
                    IM_COL32(190, 205, 220, 220), dbuf);
        ybot += 13.f;
    }
    const char* wn = weapons::name(p.weapon_id);
    if (cfg.esp_weapon && wn && p.weapon_id) {
        char wbuf[64]{};
        if (p.clip >= 0)
            sprintf_s(wbuf, "%s [%d/%d]", wn, p.clip, p.reserve);
        else
            sprintf_s(wbuf, "%s", wn);
        dl->AddText({cx - ImGui::CalcTextSize(wbuf).x*0.5f, ybot},
                    IM_COL32(220,220,180,220), wbuf);
        ybot += 13.f;
    }

    // Utilities

}

// ─── Radar ───────────────────────────────────────────────────────────────────
void Esp::draw_radar(ImDrawList* dl,const EntityCache& cache,const Config& cfg,int sw,int sh) {
    radar_.draw(dl,cache,cfg,sw,sh);
}

// ─── Crosshair ───────────────────────────────────────────────────────────────
void Esp::draw_crosshair(ImDrawList* dl, const Config& cfg, int sw, int sh) {
    float cx = sw * 0.5f, cy = sh * 0.5f;
    float sz = cfg.ch_size, gap = cfg.ch_gap, th = cfg.ch_thick;
    ImU32 col = cfg.ch_col;
    dl->AddLine({cx-sz-gap,cy},{cx-gap,cy}, col, th);
    dl->AddLine({cx+gap,cy},{cx+sz+gap,cy}, col, th);
    dl->AddLine({cx,cy-sz-gap},{cx,cy-gap}, col, th);
    dl->AddLine({cx,cy+gap},{cx,cy+sz+gap}, col, th);
    if (cfg.ch_dot) dl->AddCircleFilled({cx,cy}, 1.5f, col);
}

// ─── FOV Circle ──────────────────────────────────────────────────────────────
void Esp::draw_fov_circle(ImDrawList* dl, float fov, const Matrix4x4& vm, int sw, int sh) {
    float rx=0,ry=0;
    if(!fov_geometry::radii(vm,fov,sw,sh,rx,ry))return;
    dl->AddEllipse({float(sw)*.5f,float(sh)*.5f},{rx,ry},IM_COL32(255,255,255,80),0,96,1.f);
}

// ─── Bomb Timer ──────────────────────────────────────────────────────────────
void Esp::draw_bomb_timer(ImDrawList* dl,const BombState& bomb,const Config&,int sw,int sh) {
    if (!bomb.entity || !bomb.clock_valid || (!bomb.ticking && !bomb.defused)) return;
    const bool exploded=!bomb.defused && !bomb.planted && bomb.server_time>=bomb.blow_time;
    if (!bomb.planted && !bomb.defused && !exploded) return;
    const float width=320.f,x=sw*.5f-width*.5f,y=sh*.1f;
    dl->AddRectFilled({x,y},{x+width,y+112},IM_COL32(10,16,23,240),6.f);
    const char* site=bomb.site==0 ? "A" : bomb.site==1 ? "B" : "?";
    char title[32]; sprintf_s(title,"BOMBA %s",site);
    dl->AddText(nullptr,13.f,{x+12,y+9},IM_COL32(190,205,220,255),title);
    dl->AddText(nullptr,13.f,{x+174,y+9},IM_COL32(190,205,220,255),"DESARME");
    if (bomb.defused || exploded) {
        dl->AddText(nullptr,24.f,{x+12,y+34},bomb.defused ? IM_COL32(80,235,155,255) : IM_COL32(255,90,80,255),
            bomb.defused ? "BOMBA DESARMADA" : "BOMBA EXPLODIU");
        return;
    }
    const ImU32 bomb_color=bomb.remaining<=5 ? IM_COL32(255,85,75,255) : IM_COL32(255,212,95,255);
    const bool in_time=bomb.defuse_valid && bomb.defuse_end<bomb.blow_time;
    const ImU32 defuse_color=in_time ? IM_COL32(80,235,155,255) : IM_COL32(255,110,90,255);
    char text[32]; sprintf_s(text,"%.2f s",bomb.remaining);
    dl->AddText(nullptr,25.f,{x+12,y+29},bomb_color,text);
    if (bomb.defuse_valid) sprintf_s(text,"%.2f s",bomb.defuse_remaining);
    else strcpy_s(text,bomb.defusing ? "..." : "--");
    dl->AddText(nullptr,25.f,{x+174,y+29},bomb.defuse_valid ? defuse_color : IM_COL32(145,160,175,255),text);
    const float bomb_ratio=std::clamp(bomb.remaining/bomb.timer_length,0.f,1.f);
    const float defuse_ratio=bomb.defuse_valid ? std::clamp(1.f-bomb.defuse_remaining/bomb.defuse_length,0.f,1.f) : 0;
    dl->AddRectFilled({x+12,y+66},{x+146,y+72},IM_COL32(40,50,60,255),2.f);
    dl->AddRectFilled({x+12,y+66},{x+12+134*bomb_ratio,y+72},bomb_color,2.f);
    dl->AddRectFilled({x+174,y+66},{x+308,y+72},IM_COL32(40,50,60,255),2.f);
    dl->AddRectFilled({x+174,y+66},{x+174+134*defuse_ratio,y+72},defuse_color,2.f);
    const char* status=!bomb.defusing ? "SEM DESARME ATIVO" : !bomb.defuse_valid ? "ATUALIZANDO DESARME" :
        in_time ? "DESARME TERMINA ANTES DA EXPLOSAO" : "DESARME NAO TERMINA A TEMPO";
    dl->AddText(nullptr,12.f,{x+12,y+88},bomb.defuse_valid ? defuse_color : IM_COL32(165,180,195,255),status);
}

// ─── Spectator List ──────────────────────────────────────────────────────────
void Esp::draw_spec_list(ImDrawList* dl, const EntityCache& cache, int sw, int sh) {
    const auto* local = cache.local();
    if (!local) return;

    float x = sw - 180.f, y = sh * 0.3f;
    bool header = false;
    for (const auto& p : cache.players()) {
        if (p.is_local || p.alive) continue;
        const auto watched = local->alive ? local->pawn : local->obs_target;
        if (!watched || p.obs_target != watched) continue;
        if (!header) {
            dl->AddText({x, y-14.f}, IM_COL32(200,200,200,200), "Spectating you:");
            header = true;
        }
        dl->AddText({x, y}, IM_COL32(255,80,80,230), p.name[0] ? p.name : "Player");
        y += 14.f;
    }
}
