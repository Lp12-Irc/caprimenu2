#include "profile_store.hpp"
#include <iostream>
#include <limits>

namespace fs=std::filesystem;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
int main() {
    const auto parent=fs::current_path();
    const auto folder=parent/("profile-store-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    auto cleanup=[&]{if(fs::absolute(folder).parent_path()==parent){std::error_code ignored;fs::remove_all(folder,ignored);}};
    try {
        fs::create_directories(folder);std::string error,name;
        Config saved;saved.esp=saved.aim=saved.radar=true;saved.esp_player_style=1;
        saved.col_skeleton=COL(10,240,180);saved.aim_fov=13.5f;saved.aim_smooth=7.2f;
        saved.radar_x=85;saved.radar_y=20;saved.radar_sz=360;saved.menu_key=VK_F8;
        check(profile_store::save(saved,"Minha config",&error,folder),"named profile save");
        const auto profile=folder/"Minha config.hbc";const auto bytes=read(profile);
        Config restarted;
        check(profile_store::startup(restarted,name,&error,folder),"startup load");
        check(name=="Minha config" && restarted==saved,"restart must restore all settings and enabled flags from the selected profile");
        check(read(profile)==bytes,"startup cannot rewrite saved preferences");

        Config other;other.esp_player_style=0;other.col_body=COL(255,0,180);
        check(profile_store::save(other,"Outra",&error,folder),"second profile save");
        Config session;strcpy_s(session.saved_key,"CURRENT-AUTHENTICATED-KEY");session.menu_open=true;session.game_status="IN GAME";
        check(profile_store::load(session,"Minha config",&error,folder),"profile selection");
        check(profile_store::selected(folder)=="Minha config" && session.esp && session.esp_player_style==1,"loading must remember the active profile");
        check(std::string(session.saved_key)=="CURRENT-AUTHENTICATED-KEY" && session.menu_open && session.game_status=="IN GAME","profile load must preserve authenticated/runtime state");

        const Config before=session;
        {std::ofstream broken(folder/"Outra.hbc",std::ios::trunc);broken<<"BROKEN";}
        check(!profile_store::load(session,"Outra",&error,folder) && session==before && profile_store::selected(folder)=="Minha config","corrupt profile must not change live settings or the next startup profile");
        Config invalid=saved;invalid.aim_sens=std::numeric_limits<float>::quiet_NaN();
        check(!profile_store::save(invalid,"Minha config",&error,folder) && read(profile)==bytes,"failed save cannot damage the last good profile");
        check(!profile_store::save(saved,"../escape",&error,folder) && profile_store::selected(folder)=="Minha config","profile traversal must be rejected");

        check(other.save(profile_store::file(folder,"Outra"),&error),"restore second profile");
        const auto marker=folder/"active-profile.txt";
        check(SetFileAttributesW(marker.c_str(),FILE_ATTRIBUTE_READONLY)!=0,"readonly marker fixture");
        const bool selected=profile_store::load(session,"Outra",&error,folder);
        SetFileAttributesW(marker.c_str(),FILE_ATTRIBUTE_NORMAL);
        check(!selected && session==before && profile_store::selected(folder)=="Minha config","remember failure must not partially load another profile");

        Config blocked=restarted;blocked.deactivate();
        Config after_block;
        check(profile_store::startup(after_block,name,&error,folder) && after_block==saved,"temporary access deactivation must not replace the saved feature states");
        check(other.save(profile_store::file(folder,"default"),&error),"default fallback fixture");
        check(profile_store::remember("Deleted profile",&error,folder),"missing selection fixture");
        check(profile_store::startup(after_block,name,&error,folder) && name=="default" && after_block==other,"deleted profile falls back to the existing default");
        {std::ofstream marker_file(marker,std::ios::trunc);marker_file<<"../escape";}
        check(profile_store::startup(after_block,name,&error,folder) && name=="default","invalid selection cannot escape the profiles folder");
        cleanup();
        std::cout<<"PASS: named profiles survive restart, all settings restored, active selection remembered, session key preserved, failed writes/loads remain atomic\n";
        return 0;
    } catch(const std::exception& error){cleanup();std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
