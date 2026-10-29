// language: C++20, file: aimbot.cpp, target: Windows 11 x64, MSVC
#include "aimbot.hpp"
#include "offsets.hpp"
#include <cmath>
#include <Windows.h>

static constexpr float PI = 3.14159265f;

float Aimbot::normalize(float a) const {
    while (a >  180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

Vec3 Aimbot::calc_angle(const Vec3& src, const Vec3& dst) const {
    Vec3 d = dst - src;
    float pitch = -atan2f(d.z, sqrtf(d.x*d.x + d.y*d.y)) * (180.f / PI);
    float yaw   =  atan2f(d.y, d.x) * (180.f / PI);
    return {pitch, yaw, 0.f};
}

Vec3 Aimbot::get_bone_pos(const PlayerEnt& p, int bone) const {
    switch (bone) {
        case 0: return p.bone[bones::HEAD].pos();
        case 1: return p.bone[bones::NECK].pos();
        case 2: return p.bone[bones::CHEST_UP].pos();
        default: return p.bone[bones::HEAD].pos();
    }
}

void Aimbot::send_mouse(float dpitch, float dyaw, float sens) {
    // Source 2 formula: mouse_counts = angle_delta / (yaw_factor * sensitivity)
    // yaw_factor = 0.022 (Valve constant), pitch_factor also 0.022
    constexpr float YAW_FACTOR = 0.022f;
    int mx = static_cast<int>(dyaw   / (YAW_FACTOR * sens));
    int my = static_cast<int>(dpitch / (YAW_FACTOR * sens));
    if (mx == 0 && my == 0) return;

    INPUT in{ .type = INPUT_MOUSE, .mi = {
        .dx = mx, .dy = my, .dwFlags = MOUSEEVENTF_MOVE
    }};
    SendInput(1, &in, sizeof(INPUT));
}

void Aimbot::update(const EntityCache& cache, const Config& cfg, Memory* mem) {
    const auto* local = cache.local();
    if (!local || !local->valid()) return;

    // ── Standard aimbot ──────────────────────────────────────────────────────
    if (cfg.aim) {
        bool key_held = (GetAsyncKeyState(cfg.aim_key) & 0x8000) != 0;
        if (cfg.aim_ads_only && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000))
            key_held = false;

        if (key_held) {
            const PlayerEnt* best  = nullptr;
            float             best_fov = cfg.aim_fov;

            for (const auto& p : cache.players()) {
                if (p.is_local || !p.valid()) continue;
                if (p.team == local->team)    continue;

                Vec3  target  = get_bone_pos(p, cfg.aim_bone);
                Vec3  ang_to  = calc_angle(local->head, target);
                float dpitch  = normalize(ang_to.x - local->eye_angles.x);
                float dyaw    = normalize(ang_to.y - local->eye_angles.y);
                float fov     = sqrtf(dpitch*dpitch + dyaw*dyaw);

                bool closer = (cfg.aim_target == 0)
                    ? (fov < best_fov)
                    : (best == nullptr || (p.origin - local->origin).len() <
                                         (best->origin - local->origin).len());
                if (closer) { best_fov = fov; best = &p; }
            }

            if (best) {
                if (cfg.aim_delay_ms > 0.f) Sleep(static_cast<DWORD>(cfg.aim_delay_ms));

                Vec3 target = get_bone_pos(*best, cfg.aim_bone);
                Vec3 ang_to = calc_angle(local->head, target);

                // RCS integration: read punch from local pawn, apply before smooth
                Vec3 eye = local->eye_angles;
                if (cfg.rcs) {
                    eye.x -= local->punch_angle.x * 2.f * cfg.rcs_intensity;
                    eye.y -= local->punch_angle.y * 2.f * cfg.rcs_intensity;
                }

                float dpitch = normalize(ang_to.x - eye.x) / cfg.aim_smooth;
                float dyaw   = normalize(ang_to.y - eye.y) / cfg.aim_smooth;

                send_mouse(dpitch, dyaw, cfg.aim_sens);
            }
        }
    }

    // ── RCS standalone (when aimbot key not held) ────────────────────────────
    if (cfg.rcs && cfg.aim) {
        // Handled above — standalone RCS when key not held:
        int cur_shots = local->shots_fired;
        if (cur_shots > 0 && cur_shots != static_cast<int>(prev_shots_)) {
            // apply punch compensation
            float dpitch = -local->punch_angle.x * 2.f * cfg.rcs_intensity / cfg.aim_smooth;
            float dyaw   = -local->punch_angle.y * 2.f * cfg.rcs_intensity / cfg.aim_smooth;
            if (!(GetAsyncKeyState(cfg.aim_key) & 0x8000)) // don't double-apply
                send_mouse(dpitch, dyaw, cfg.aim_sens);
        }
        prev_shots_ = static_cast<float>(local->shots_fired);
    }

    // ── Silent aim ───────────────────────────────────────────────────────────
    // Writes adjusted viewangles to CUserCmd — requires mem->write() (VM_WRITE handle)
    // Detection: GC kernel driver can detect WriteProcessMemory calls to cs2.exe
    // Mitigation: use a signed kernel driver that maps the write below IRQL monitoring
    if (cfg.silent_aim && mem && mem->has_write()) {
        bool sa_key = true; // always on when enabled — add key if preferred
        if (sa_key) {
            const PlayerEnt* best = nullptr;
            float best_fov = cfg.sa_fov;

            for (const auto& p : cache.players()) {
                if (p.is_local || !p.valid() || p.team == local->team) continue;
                Vec3 t = get_bone_pos(p, cfg.sa_bone);
                Vec3 a = calc_angle(local->head, t);
                float dp = normalize(a.x - local->eye_angles.x);
                float dy = normalize(a.y - local->eye_angles.y);
                float f  = sqrtf(dp*dp + dy*dy);
                if (f < best_fov) { best_fov = f; best = &p; }
            }

            if (best) {
                Vec3 t = get_bone_pos(*best, cfg.sa_bone);
                Vec3 a = calc_angle(local->head, t);
                // Write to m_angEyeAngles on local pawn
                // This modifies the angles used by the next UserCmd send
                mem->write<Vec3>(local->pawn + offsets::m_angEyeAngles, a);
                // The visible camera doesn't move (no SendInput) — only the cmd angle changes
            }
        }
    }
}
