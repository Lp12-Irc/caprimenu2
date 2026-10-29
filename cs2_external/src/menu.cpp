// language: C++20, file: menu.cpp, target: Windows 11 x64, ImGui
#include "menu.hpp"
#include "imgui.h"
#include <filesystem>
#include <vector>
#include <string>
namespace fs = std::filesystem;

static const char* BONE_NAMES[] = {"Head","Neck","Chest"};
static const char* FILTER_NAMES[] = {"Enemies","Allies","All"};
static const char* BOX_STYLES[]   = {"Outline","Corner","Filled"};
static const char* TARGET_MODES[] = {"Min FOV","Min Distance"};

// Compact color picker: inline button showing the color, opens picker on click
void Menu::color_edit(const char* label, ImU32& col) {
    float fc[4] = {
        float(col & 0xFF) / 255.f,
        float((col >> 8)  & 0xFF) / 255.f,
        float((col >> 16) & 0xFF) / 255.f,
        float((col >> 24) & 0xFF) / 255.f
    };
    if (ImGui::ColorEdit4(label, fc, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
        col = IM_COL32(uint8_t(fc[0]*255), uint8_t(fc[1]*255),
                       uint8_t(fc[2]*255), uint8_t(fc[3]*255));
    }
}

void Menu::render(Config& cfg, const AuthInfo& auth_info, bool& running) {
    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowSize({520, 420}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("HillBlack", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("main_tabs")) {
        if (ImGui::BeginTabItem("ESP"))    { tab_esp(cfg);                ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Aimbot")) { tab_aimbot(cfg);             ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Misc"))   { tab_misc(cfg);               ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Config")) { tab_config(cfg);             ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Info"))   { tab_info(cfg, auth_info);    ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void Menu::tab_esp(Config& cfg) {
    ImGui::Checkbox("Enable ESP", &cfg.esp);
    ImGui::Separator();
    if (!cfg.esp) { ImGui::TextDisabled("(disabled)"); return; }

    ImGui::Columns(2, nullptr, false);

    ImGui::Checkbox("Bounding Box", &cfg.esp_box);
    if (cfg.esp_box) {
        ImGui::Combo("Style##box", &cfg.esp_box_style, BOX_STYLES, 3);
        ImGui::SliderFloat("Thickness##box", &cfg.esp_box_thick, 0.5f, 4.f);
    }
    ImGui::Checkbox("Skeleton",     &cfg.esp_skeleton);
    if (cfg.esp_skeleton)
        ImGui::SliderFloat("Thickness##sk", &cfg.esp_skel_thick, 0.5f, 4.f);
    ImGui::Checkbox("Snap Lines",   &cfg.esp_snaplines);
    ImGui::Checkbox("Distance",     &cfg.esp_distance);
    ImGui::Checkbox("Health Bar",   &cfg.esp_healthbar);
    if (cfg.esp_healthbar)
        ImGui::Checkbox("Show Number", &cfg.esp_health_num);
    ImGui::Checkbox("Weapon Info",  &cfg.esp_weapon);
    ImGui::Checkbox("Utilities",    &cfg.esp_utilities);
    ImGui::Checkbox("Vis Check",    &cfg.esp_vischeck);

    ImGui::NextColumn();

    ImGui::Combo("Filter##esp", &cfg.esp_filter, FILTER_NAMES, 3);
    ImGui::SliderFloat("Max Dist (m)", &cfg.esp_max_dist, 10.f, 1000.f);
    ImGui::Separator();
    color_edit("CT Color",      cfg.col_ct);
    color_edit("TR Color",      cfg.col_tr);
    color_edit("Visible Color", cfg.col_visible);
    color_edit("Skeleton Color",cfg.col_skeleton);
    color_edit("Snapline Color",cfg.col_snapline);

    ImGui::Columns(1);
}

void Menu::tab_aimbot(Config& cfg) {
    ImGui::Checkbox("Aimbot", &cfg.aim);
    if (cfg.aim) {
        ImGui::Combo("Bone##aim",   &cfg.aim_bone,   BONE_NAMES,   3);
        ImGui::Combo("Target##aim", &cfg.aim_target, TARGET_MODES, 2);
        ImGui::SliderFloat("FOV##aim",    &cfg.aim_fov,    1.f, 45.f);
        ImGui::SliderFloat("Smooth##aim", &cfg.aim_smooth, 1.f, 15.f);
        ImGui::SliderFloat("Sens##aim",   &cfg.aim_sens,   0.1f, 5.f);
        ImGui::SliderFloat("Delay ms",    &cfg.aim_delay_ms, 0.f, 200.f);
        ImGui::Checkbox("ADS Only",        &cfg.aim_ads_only);
        ImGui::Checkbox("FOV Circle",      &cfg.aim_fov_circle);
    }
    ImGui::Separator();
    ImGui::Checkbox("Silent Aim", &cfg.silent_aim);
    if (cfg.silent_aim) {
        ImGui::TextColored({1.f,0.8f,0.2f,1.f},
            "Requires write handle — higher detection risk on GC/ESEA");
        ImGui::Combo("Bone##sa",  &cfg.sa_bone, BONE_NAMES, 3);
        ImGui::SliderFloat("FOV##sa", &cfg.sa_fov, 1.f, 30.f);
    }
    ImGui::Separator();
    ImGui::Checkbox("Recoil Control (RCS)", &cfg.rcs);
    if (cfg.rcs)
        ImGui::SliderFloat("RCS Intensity", &cfg.rcs_intensity, 0.1f, 2.f);
}

void Menu::tab_misc(Config& cfg) {
    ImGui::Columns(2, nullptr, false);

    ImGui::Checkbox("Radar", &cfg.radar);
    if (cfg.radar) {
        ImGui::SliderFloat("Pos X%%", &cfg.radar_x, 0.f, 90.f);
        ImGui::SliderFloat("Pos Y%%", &cfg.radar_y, 0.f, 90.f);
        ImGui::SliderFloat("Size px", &cfg.radar_sz, 80.f, 300.f);
        ImGui::SliderFloat("Scale",   &cfg.radar_scale, 50.f, 1000.f);
    }
    ImGui::Separator();
    ImGui::Checkbox("No Flash", &cfg.no_flash);
    if (cfg.no_flash) {
        ImGui::TextColored({1.f,0.8f,0.2f,1.f}, "Needs write handle");
        ImGui::SliderFloat("Max Alpha", &cfg.no_flash_max, 0.f, 50.f);
    }
    ImGui::Separator();
    ImGui::Checkbox("Custom Crosshair", &cfg.crosshair);
    if (cfg.crosshair) {
        ImGui::SliderFloat("Size##ch",  &cfg.ch_size,  1.f, 20.f);
        ImGui::SliderFloat("Gap##ch",   &cfg.ch_gap,   0.f, 10.f);
        ImGui::SliderFloat("Thick##ch", &cfg.ch_thick, 0.5f, 4.f);
        ImGui::Checkbox("Dot", &cfg.ch_dot);
        color_edit("CH Color", cfg.ch_col);
    }

    ImGui::NextColumn();

    ImGui::Checkbox("Triggerbot", &cfg.triggerbot);
    if (cfg.triggerbot) {
        ImGui::SliderFloat("Delay ms##tb",   &cfg.tb_delay_ms,   0.f, 300.f);
        ImGui::SliderFloat("Release ms##tb", &cfg.tb_release_ms, 10.f, 100.f);
    }
    ImGui::Separator();
    ImGui::Checkbox("Spectator List", &cfg.spectator_list);
    ImGui::Checkbox("Bomb Timer",     &cfg.bomb_timer);
    ImGui::Checkbox("Item ESP",       &cfg.item_esp);
    ImGui::Columns(1);
}

void Menu::tab_config(Config& cfg) {
    ImGui::InputText("Profile", profile_name_, sizeof(profile_name_));

    if (ImGui::Button("Save")) {
        std::string path = std::string("configs/") + profile_name_ + ".hbc";
        fs::create_directories("configs");
        cfg.save(path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        std::string path = std::string("configs/") + profile_name_ + ".hbc";
        cfg.load(path);
    }
    ImGui::Separator();

    // List existing profiles
    if (fs::exists("configs")) {
        ImGui::Text("Profiles:");
        for (auto& f : fs::directory_iterator("configs")) {
            if (f.path().extension() == ".hbc") {
                std::string stem = f.path().stem().string();
                if (ImGui::Selectable(stem.c_str()))
                    strncpy_s(profile_name_, stem.c_str(), sizeof(profile_name_)-1);
            }
        }
    }
}

void Menu::tab_info(Config& cfg, const AuthInfo& ai) {
    ImGui::Text("Version:   1.0.0");
    ImGui::Text("HWID:      %s", ai.hwid.c_str());
    ImGui::Text("Expires:   %s", ai.expires_at.empty() ? "—" : ai.expires_at.c_str());

    const char* status_str = "Unknown";
    ImVec4      status_col = {1,1,0,1};
    switch (ai.result) {
        case AuthResult::OK:           status_str = "Active";        status_col = {0,1,0,1}; break;
        case AuthResult::EXPIRED:      status_str = "Expired";       status_col = {1,0.5,0,1}; break;
        case AuthResult::BAD_KEY:      status_str = "Invalid Key";   status_col = {1,0,0,1}; break;
        case AuthResult::HWID_MISMATCH:status_str = "HWID Mismatch"; status_col = {1,0,0,1}; break;
        case AuthResult::NET_ERROR:    status_str = "Net Error";     status_col = {0.7,0.7,0,1}; break;
    }
    ImGui::TextColored(status_col, "Status:    %s", status_str);
    ImGui::Separator();
    ImGui::Text("Menu Key:  INSERT");
    ImGui::Text("Detection: External read-only handle");
    ImGui::TextDisabled("Write features (no-flash, silent aim) use a second handle.");
}

bool Menu::want_capture_keyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }
bool Menu::want_capture_mouse()    const { return ImGui::GetIO().WantCaptureMouse; }
