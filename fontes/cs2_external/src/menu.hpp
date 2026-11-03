// language: C++20, file: menu.hpp, target: Windows 11 x64, ImGui
#pragma once
#include "config.hpp"
#include "auth.hpp"
#include "profile_catalog.hpp"
#include "ui_motion.hpp"
#include <string>
#include <cstring>

class Menu {
public:
    explicit Menu(const std::string& profile="default",int page=0) : active_tab_(page>=0 && page<7?page:0) { strncpy_s(profile_name_,profile.c_str(),_TRUNCATE); }
    static void apply_theme();
    static void apply_theme(const Config&);
    void motion_tick(const Config&,bool focused);
    bool animating() const {return motion_enabled_;}
    void render(Config& cfg, const AuthInfo& auth_info, bool& running);
    void render_access_block(const AuthInfo&,int sw,int sh,bool& running,std::string* requested_key=nullptr);
    void radar_controls(Config& cfg, int sw, int sh);
    bool want_capture_keyboard() const;
    bool want_capture_mouse()    const;

private:
    void tab_display(Config& cfg);
    void tab_esp    (Config& cfg);
    void tab_trigger(Config& cfg);
    void tab_aimbot (Config& cfg);
    void tab_radar(Config& cfg);
    void tab_misc   (Config& cfg);
    void tab_config (Config& cfg);
    void tab_info   (Config& cfg, const AuthInfo& ai);
    void color_edit (const char* label, ImU32& col);
    void save_profile(Config& cfg,const std::string& name);

    ui_motion::Plexus particles_;
    bool motion_enabled_=false;
    float tab_alpha_=1.f;
    int active_tab_ = 0;
    std::string status_;
    ProfileCatalog profiles_;
    char profile_name_[64] = "default";
    char access_key_[256] = {};
};
