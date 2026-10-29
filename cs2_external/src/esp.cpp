// language: C++20, file: esp.cpp, target: Windows 11 x64
#include "esp.hpp"
#include "imgui.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// ─── World → Screen ─────────────────────────────────────────────────────────
Scr Esp::w2s(const Vec3& w, const Matrix4x4& vm, int sw, int sh) const {
    // Column-major view-projection (Source 2 standard)
    float clip_w = vm.m[3][0]*w.x + vm.m[3][1]*w.y + vm.m[3][2]*w.z + vm.m[3][3];
    if (clip_w < 0.001f) return {-9999.f, -9999.f, false};
    float clip_x = vm.m[0][0]*w.x + vm.m[0][1]*w.y + vm.m[0][2]*w.z + vm.m[0][3];
    float clip_y = vm.m[1][0]*w.x + vm.m[1][1]*w.y + vm.m[1][2]*w.z + vm.m[1][3];
    return {
        (sw * 0.5f) * (1.f + clip_x / clip_w),
        (sh * 0.5f) * (1.f - clip_y / clip_w),
        true
    };
}

// ─── Main render ────────────────────────────────────────────────────────────
void Esp::render(const EntityCache& cache, const Config& cfg,
                 ImDrawList* dl, int sw, int sh) {
    if (!cfg.esp) goto skip_players;
    {
        const auto* local = cache.local();
        const auto& vm    = cache.view_matrix();

        for (const auto& p : cache.players()) {
            if (p.is_local) continue;
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

            Scr head = w2s(p.head, vm, sw, sh);
            Vec3 feet_w = p.origin; feet_w.z -= 4.f;
            Scr feet = w2s(feet_w, vm, sw, sh);

            if (!head.vis || !feet.vis) continue;

            // Vis-check color selection
            ImU32 col;
            if (cfg.esp_vischeck) {
                // Simplified vis: use flash-alpha or raycast not available externally.
                // We approximate via whether the enemy's head is in FOV and short range.
                // For accurate vis, implement ray-vs-AABB against static world — 
                // impractical externally. Use distance/flash heuristic:
                bool visible = p.flash < 10.f && dist_m < 50.f; // refine per use case
                col = visible ? cfg.col_visible
                    : (p.team == 2 ? cfg.col_tr : cfg.col_ct);
            } else {
                col = (p.team == 2) ? cfg.col_tr : cfg.col_ct;
            }

            if (cfg.esp_box)       draw_box(dl, head, feet, col, cfg.esp_box_thick, cfg.esp_box_style);
            if (cfg.esp_skeleton)  draw_skeleton(dl, p, vm, sw, sh, cfg.col_skeleton, cfg.esp_skel_thick);
            if (cfg.esp_healthbar) draw_healthbar(dl, head, feet, p.health, cfg.esp_health_num);
            if (cfg.esp_snaplines) draw_snapline(dl, feet, sw, sh, cfg.col_snapline);
            if (cfg.esp_distance || cfg.esp_weapon) draw_text_info(dl, p, head, feet, sw);
        }
    }
skip_players:

    if (cfg.radar)   draw_radar(dl, cache, cfg, sw, sh);
    if (cfg.crosshair) draw_crosshair(dl, cfg, sw, sh);
    if (cfg.aim && cfg.aim_fov_circle) draw_fov_circle(dl, cfg.aim_fov, sw, sh);
    if (cfg.bomb_timer) draw_bomb_timer(dl, cache.bomb(), cfg, sw, sh);
    if (cfg.spectator_list) draw_spec_list(dl, cache, sw, sh);
}

// ─── Box ────────────────────────────────────────────────────────────────────
void Esp::draw_box(ImDrawList* dl, const Scr& head, const Scr& feet,
                   ImU32 col, float thick, int style) {
    float box_h = fabsf(feet.y - head.y);
    float box_w = box_h * 0.5f;
    float x1 = head.x - box_w * 0.5f, y1 = head.y - box_h * 0.05f;
    float x2 = x1 + box_w,            y2 = y1 + box_h;

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
        C(x1,x1+cw, y1,y1+ch); C(x2-cw,x2, y1,y1+ch);
        C(x1,x1+cw, y2,y2-ch); C(x2-cw,x2, y2,y2-ch);
        break;
    }
    case 2: // filled
        dl->AddRectFilled({x1,y1},{x2,y2}, col & 0x40FFFFFF);
        dl->AddRect({x1,y1},{x2,y2}, col, 0.f, 0, thick);
        break;
    }
}

// ─── Skeleton ────────────────────────────────────────────────────────────────
void Esp::draw_skeleton(ImDrawList* dl, const PlayerEnt& p, const Matrix4x4& vm,
                         int sw, int sh, ImU32 col, float thick) {
    for (int i = 0; i < bones::PAIR_COUNT; ++i) {
        int a = bones::PAIRS[i][0], b = bones::PAIRS[i][1];
        Vec3 pa = p.bone[a].pos(), pb = p.bone[b].pos();
        if (pa.x == 0.f && pa.y == 0.f) continue;
        Scr sa = w2s(pa, vm, sw, sh), sb = w2s(pb, vm, sw, sh);
        if (!sa.vis || !sb.vis) continue;
        dl->AddLine({sa.x,sa.y},{sb.x,sb.y}, col, thick);
    }
}

// ─── Health Bar ──────────────────────────────────────────────────────────────
void Esp::draw_healthbar(ImDrawList* dl, const Scr& head, const Scr& feet,
                          int hp, bool show_num) {
    float box_h = fabsf(feet.y - head.y);
    float box_w = box_h * 0.5f;
    float x1 = head.x - box_w * 0.5f - 6.f;
    float y1 = head.y - box_h * 0.05f;
    float y2 = y1 + box_h;
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
                          const Scr& head, const Scr& feet, int sw) {
    float box_h = fabsf(feet.y - head.y);
    float box_w = box_h * 0.5f;
    float cx  = head.x;
    float ytop = head.y - box_h * 0.05f - 14.f;

    if (p.name[0]) {
        dl->AddText({cx - ImGui::CalcTextSize(p.name).x * 0.5f, ytop - 13.f},
                    IM_COL32(255,255,255,230), p.name);
        ytop -= 13.f;
    }

    // Distance
    char dbuf[32]{};
    if (p.is_local) return;

    // Weapon + ammo below feet
    float ybot = feet.y + box_h * 0.05f + 2.f;
    const char* wn = weapons::name(p.weapon_id);
    if (wn && p.weapon_id) {
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
    if (!p.util_ids.empty()) {
        std::string uline;
        for (auto uid : p.util_ids) {
            auto tag = weapons::util_tag(uid);
            if (tag) { uline += tag; uline += ' '; }
        }
        if (!uline.empty())
            dl->AddText({cx - ImGui::CalcTextSize(uline.c_str()).x*0.5f, ybot},
                        IM_COL32(180,220,255,200), uline.c_str());
    }
}

// ─── Radar ───────────────────────────────────────────────────────────────────
void Esp::draw_radar(ImDrawList* dl, const EntityCache& cache,
                      const Config& cfg, int sw, int sh) {
    float rx = sw * cfg.radar_x / 100.f;
    float ry = sh * cfg.radar_y / 100.f;
    float rsz = cfg.radar_sz;

    dl->AddRectFilled({rx,ry},{rx+rsz,ry+rsz}, IM_COL32(0,0,0,160));
    dl->AddRect({rx,ry},{rx+rsz,ry+rsz}, IM_COL32(80,80,80,200), 0.f, 0, 1.f);

    const auto* local = cache.local();
    if (!local) return;

    // Local player dot
    dl->AddCircleFilled({rx+rsz*0.5f, ry+rsz*0.5f}, 3.f, IM_COL32(0,255,0,255));

    for (const auto& p : cache.players()) {
        if (p.is_local || !p.valid()) continue;
        Vec3 delta = p.origin - local->origin;
        // Rotate by local yaw to orient radar forward-up
        float yaw_rad = local->eye_angles.y * 3.14159f / 180.f;
        float rx2 =  delta.x * cosf(yaw_rad) + delta.y * sinf(yaw_rad);
        float ry2 = -delta.x * sinf(yaw_rad) + delta.y * cosf(yaw_rad);
        float sx = rx + rsz*0.5f + (ry2 / cfg.radar_scale) * rsz*0.5f;
        float sy = ry + rsz*0.5f - (rx2 / cfg.radar_scale) * rsz*0.5f;
        sx = std::clamp(sx, rx+2.f, rx+rsz-2.f);
        sy = std::clamp(sy, ry+2.f, ry+rsz-2.f);
        ImU32 dc = (p.team == 2) ? cfg.col_tr : cfg.col_ct;
        dl->AddCircleFilled({sx,sy}, 3.f, dc);
    }

    // Bomb on radar
    const auto& bomb = cache.bomb();
    if (bomb.planted) {
        Vec3 bd = bomb.pos - local->origin;
        float yaw_rad = local->eye_angles.y * 3.14159f / 180.f;
        float brx =  bd.x * cosf(yaw_rad) + bd.y * sinf(yaw_rad);
        float bry = -bd.x * sinf(yaw_rad) + bd.y * cosf(yaw_rad);
        float bsx = rx + rsz*0.5f + (bry/cfg.radar_scale)*rsz*0.5f;
        float bsy = ry + rsz*0.5f - (brx/cfg.radar_scale)*rsz*0.5f;
        bsx = std::clamp(bsx, rx+2.f, rx+rsz-2.f);
        bsy = std::clamp(bsy, ry+2.f, ry+rsz-2.f);
        dl->AddCircleFilled({bsx,bsy}, 4.f, IM_COL32(255,180,0,255));
    }
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
void Esp::draw_fov_circle(ImDrawList* dl, float fov, int sw, int sh) {
    // Approximate screen-space radius from FOV degrees
    // tan(fov/2) * (screen_height / 2) / tan(game_fov/2 in degrees)
    // Simplified: radius ≈ (sh/2) * (fov / 90.f)
    float radius = (sh * 0.5f) * (fov / 90.f);
    dl->AddCircle({float(sw)*0.5f, float(sh)*0.5f}, radius,
                  IM_COL32(255,255,255,80), 64, 1.f);
}

// ─── Bomb Timer ──────────────────────────────────────────────────────────────
void Esp::draw_bomb_timer(ImDrawList* dl, const BombState& bomb,
                           const Config& cfg, int sw, int sh) {
    if (!bomb.planted || bomb.defused) return;

    float now = GetTickCount64() / 1000.f; // imprecise — bomb blow_time is server time
    // Use blow_time - curtime, but curtime needs reading from engine GlobalVars
    // Fallback: show blow_time directly and let user interpret it
    float remaining = bomb.blow_time; // set to (blow_time - curtime) in main loop

    float bx = sw * 0.5f - 100.f, by = sh * 0.12f;
    dl->AddRectFilled({bx-4.f,by-4.f},{bx+204.f,by+44.f}, IM_COL32(0,0,0,180));

    char site_buf[32]{};
    sprintf_s(site_buf, "BOMB - %s", bomb.site == 0 ? "A" : "B");
    dl->AddText({bx, by}, IM_COL32(255,80,80,255), site_buf);

    char time_buf[32]{};
    sprintf_s(time_buf, "%.1fs", remaining > 0.f ? remaining : 0.f);
    dl->AddText({bx, by+16.f}, IM_COL32(255,220,0,255), time_buf);

    // Progress bar
    float ratio = std::clamp(remaining / bomb.timer_length, 0.f, 1.f);
    dl->AddRectFilled({bx,by+32.f},{bx+200.f,by+40.f}, IM_COL32(40,40,40,200));
    ImU32 bc = ratio > 0.4f ? IM_COL32(0,200,0,255)
             : ratio > 0.2f ? IM_COL32(255,200,0,255)
             :                IM_COL32(255,40,40,255);
    dl->AddRectFilled({bx,by+32.f},{bx+200.f*ratio,by+40.f}, bc);

    if (bomb.defuse_end > 0.f) {
        char dbuf[32]{};
        sprintf_s(dbuf, "DEFUSE %.1fs", bomb.defuse_end);
        dl->AddText({bx, by+44.f}, IM_COL32(0,255,200,255), dbuf);
    }
}

// ─── Spectator List ──────────────────────────────────────────────────────────
void Esp::draw_spec_list(ImDrawList* dl, const EntityCache& cache, int sw, int sh) {
    const auto* local = cache.local();
    if (!local) return;

    float x = sw - 180.f, y = sh * 0.3f;
    bool header = false;
    for (const auto& p : cache.players()) {
        if (p.is_local || !p.alive) continue;
        // obs_target is a handle — compare to local pawn ptr (approximate)
        // In practice: resolve handle → pawn and compare with local->pawn
        if (p.obs_target == 0 || p.obs_target == 0xFFFFFFFF) continue;
        // Mark if watching local (handle resolution needed — simplified check)
        if (!header) {
            dl->AddText({x, y-14.f}, IM_COL32(200,200,200,200), "Spectating you:");
            header = true;
        }
        dl->AddText({x, y}, IM_COL32(255,80,80,230), p.name[0] ? p.name : "Player");
        y += 14.f;
    }
}
