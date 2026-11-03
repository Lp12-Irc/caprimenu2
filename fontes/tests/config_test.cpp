// language: C++20 / file: config_test.cpp / runtime: MSVC / target: Windows x64
#include "config.hpp"
#include "credentials.hpp"
#include "input_frame.hpp"
#include "auth.hpp"
#include "render_policy.hpp"
#include "ui_motion.hpp"
#include "aim_math.hpp"
#include "profile_catalog.hpp"
#include "string_enc.hpp"
#include "json.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace fs = std::filesystem;
using json = nlohmann::json;

static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static void check_result(bool value, const std::string& error) {
    if (!value) throw std::runtime_error(error);
}

static void write_profile(const fs::path& path, const std::string& json_text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    for (const auto c : json_text) stream.put(static_cast<char>(static_cast<uint8_t>(c) ^ enc::KEY));
    check(bool(stream), "Could not write test fixture");
}

int main() {
    const fs::path folder = fs::current_path() / ("config-test-" + std::to_string(GetCurrentProcessId()) +
                                               "-" + std::to_string(GetTickCount64()));
    try {
        fs::create_directories(folder);
        const auto profile = folder / "settings.hbc";
        std::string error;
        ui_motion::Plexus field;
        field.advance(1000,true);field.advance(1040,true);const auto before=field.points;
        field.advance(1050,false);const float phase=field.phase();field.advance(100000,false);
        check(field.phase()==phase && field.points[0].x==before[0].x,"Hidden particle field continues moving");
        field.advance(100040,true);check(field.phase()==phase,"Resume integrates hidden elapsed time");
        field.advance(100080,true);check(field.phase()>phase,"Particle motion does not resume");
        for(const auto link:field.links)check(link.a<field.count && link.b<field.count,"Particle link outside fixed storage");
        float remainder=.99f;
        check(aim_math::bounded_count(.2f,.6f,remainder)==0 && remainder==0,"Sub-count target receives an overshooting count");
        remainder=.9f;check(aim_math::bounded_count(.9f,1.1f,remainder)==1,"Quantized correction overshoots remaining error");
        check(aim_math::bounded_count(-.5f,-2.f,remainder)==0 && remainder<0,"Direction reversal retains opposite fractional input");
        Config source;
        AuthInfo grant;grant.result=AuthResult::OK;
        check(AUTH_POLL_MS==2000 && auth_grant_active(grant,3999,4000) && !auth_grant_active(grant,4000,4000),"expired server grant keeps input enabled");
        grant.result=AuthResult::BAD_KEY;check(!auth_grant_active(grant,1,4000),"revoked grant remains active");
        grant.result=AuthResult::OK;check(auth_grant_active(grant,4001,8000),"reactivated key cannot regain access");
        AuthInfo response;
        const std::uint64_t network_deadline=1000+AUTH_LEASE_MS;
        check(AUTH_LEASE_MS==60000,"network retry window changed unexpectedly");
        for(std::uint64_t now=3000;now<network_deadline;now+=AUTH_POLL_MS)
            check(auth_keep_previous_grant(grant,response,now,network_deadline),"brief network failure interrupts valid session");
        check(!auth_keep_previous_grant(grant,response,network_deadline,network_deadline),"network failures extend grant indefinitely");
        for(auto denial:{AuthResult::BAD_KEY,AuthResult::EXPIRED,AuthResult::HWID_MISMATCH,AuthResult::UPDATE_REQUIRED}) {
            response.result=denial;
            check(!auth_keep_previous_grant(grant,response,3000,network_deadline),"explicit denial incorrectly receives network grace");
        }
        response.result=AuthResult::NET_ERROR;grant.result=AuthResult::BAD_KEY;
        check(!auth_keep_previous_grant(grant,response,3000,network_deadline),"network failure revives denied session");
        grant.result=AuthResult::OK;response.result=AuthResult::OK;
        check(!auth_keep_previous_grant(grant,response,3000,network_deadline),"successful response does not renew grant");
        check(source.aim_target==1 && source.aim_visibility && source.menu_size==0,"safe compact defaults missing");
        source.menu_accent=COL(35,180,165);source.menu_background=COL(8,19,20);
        InputFrames channel;
        check(channel.publish(source,1920,1080,false,false),"Initial settings were not published");
        const auto immutable=channel.load();int unchanged=0;
        for(int i=0;i<10000;++i)unchanged+=channel.publish(source,1920,1080,false,false);
        check(unchanged==0 && channel.load()==immutable,"Unchanged settings allocate new input frames");
        Config changed=source;changed.aim_fov=12;
        check(channel.publish(changed,1920,1080,false,false) && immutable->cfg.aim_fov==source.aim_fov,"Settings publication mutates an existing frame");
        check(channel.publish(changed,1280,720,true,false) && channel.load()->enabled,"Resize/activation not published");
        check(channel.publish(changed,1280,720,false,true) && channel.load()->blocked,"Access block not published");
        MenuFrameGate gate;int quiet=0;
        for(std::uint64_t t=0;t<8000;t+=16)if(gate.due(t,1,false,false)){++quiet;gate.commit(t,1);}
        check(quiet==32,"Quiet menu does not limit redundant redraws");
        check(gate.due(8000,2,false,false),"Input event did not wake redraw");
        gate.commit(8000,2);
        check(gate.wait_ms(8001)==249 && gate.wait_ms(8250)==0,"Idle wait deadline incorrect");
        check(gate.due(8001,2,true,false) && gate.due(8001,2,false,true) && gate.due(8001,2,false,false,true),"Live content, interaction or access change was throttled");
        gate.commit(9000,2);
        check(!gate.due(9039,2,false,false,false,true) && gate.due(9040,2,false,false,false,true),"Ambient cadence exceeds its 25 Hz budget");
        check(gate.wait_ms(9001,true)==39,"Ambient wait deadline incorrect");
        std::cout<<"PASS: quiet-menu schedule 32/500 redraws; 10000 unchanged settings publish zero copies\n";
        check(!source.needs_entities() && !source.needs_input() && !source.needs_drawing(),"Fresh launch enables a feature");
        source.esp=source.radar=source.aim=source.rcs=source.triggerbot=source.no_flash=true;
        source.spectator_list=source.bomb_timer=source.crosshair=source.item_esp=true;
        source.deactivate();
        check(!source.needs_entities() && !source.needs_input() && !source.needs_drawing(),"Startup does not deactivate saved features");
        source.menu_motion=false;
        source.esp=true; // The round-trip below intentionally includes an enabled feature.
        source.esp_body=false;source.esp_player_style=1;source.col_body=COL(255,0,200);
        source.col_visible = COL(1, 2, 3, 4);
        source.col_skeleton = COL(5, 6, 7, 8);
        source.col_snapline = COL(9, 10, 11, 12);
        source.ch_col = COL(13, 14, 15, 16);
        check(source.tb_delay_ms==0 && source.tb_interval_ms==0 && source.tb_release_ms==16,"immediate trigger defaults missing");
        check(source.tb_head_only && source.tb_head_margin==.8f,"head-only defaults missing");
        source.tb_head_only=false;source.tb_head_margin=.5f;source.tb_interval_ms=125;
        source.tb_release_ms = 77.f;
        source.aim_sens = 0.1f;
        source.rcs_intensity = 0.1f;
        source.esp_box_style = 2;
        source.menu_key = VK_F7;
        source.display_aspect=3;source.display_fit=1;
        source.radar_sz=440; source.radar_zoom=2.25f; source.radar_names=false;
        strcpy_s(source.saved_key, "TEST-PROFILE-NOT-A-LICENSE");
        check_result(source.save(profile.string(), &error), error);
        ProfileCatalog catalog;
        check(catalog.refresh(10,folder) && catalog.names().size()==1,"Profile catalog first scan failed");
        for(int i=11;i<1000;++i)catalog.refresh(i,folder);
        check(catalog.scans()==1,"Profile directory scanned each frame");
        const auto extra=folder/"new.hbc";write_profile(extra,"{}");
        catalog.invalidate();check(catalog.refresh(1000,folder) && catalog.names().size()==2,"New profile invalidation failed");
        fs::remove(extra);
        check(catalog.refresh(3000,folder) && catalog.names().size()==1,"External profile deletion did not refresh");
        Config loaded;
        loaded.menu_open = true;
        check_result(loaded.load(profile.string(), &error), error);
        check(loaded.esp_body==source.esp_body && loaded.esp_player_style==source.esp_player_style && loaded.col_body==source.col_body,"neon body/color not persisted");
        check(loaded.menu_accent==source.menu_accent && loaded.menu_background==source.menu_background && loaded.aim_visibility==source.aim_visibility,"menu theme or visibility not persisted");
        check(loaded.col_visible == source.col_visible && loaded.col_skeleton == source.col_skeleton &&
              loaded.col_snapline == source.col_snapline && loaded.ch_col == source.ch_col,
              "Color round-trip regression");
        check(!loaded.tb_head_only && loaded.tb_head_margin==.5f && loaded.tb_interval_ms==125,"trigger region/timing preferences not persisted");
        check(loaded.tb_release_ms == 77.f && loaded.aim_sens == 0.1f && loaded.rcs_intensity == 0.1f,
              "Timing or float-bound round-trip regression");
        check(loaded.display_aspect==3 && loaded.display_fit==1,"Display aspect and format were not persisted");
        check(!loaded.menu_motion,"Economy preference was not persisted");
        check(loaded.menu_open && loaded.menu_key == VK_F7, "Runtime menu state changed");
        check(loaded.radar_sz==440 && loaded.radar_zoom==2.25f && !loaded.radar_names,"map radar preferences survive profile round-trip");
        check(!loaded.saved_key[0], "A saved profile contains a license key");
        const auto vault=folder/"credentials.dat";
        std::string secret="unchanged";
        check(credentials::load(vault,secret,error)==credentials::Result::Missing && secret=="unchanged","Missing vault modified the current key");
        check(credentials::save(vault,source.saved_key,error),"DPAPI protection failed");
        check(credentials::load(vault,secret,error)==credentials::Result::Loaded && secret==source.saved_key,"Protected key round-trip failed");
        std::ifstream encrypted(vault,std::ios::binary);
        std::string raw((std::istreambuf_iterator<char>(encrypted)),{});encrypted.close();
        check(raw.find(source.saved_key)==std::string::npos,"Protected file contains the plaintext key");
        check(!credentials::save(vault,"",error),"Empty key overwrote the saved key");
        check(credentials::load(vault,secret,error)==credentials::Result::Loaded && secret==source.saved_key,"Failed save damaged the protected key");
        raw[raw.size()/2]^=1;
        {std::ofstream corrupt(vault,std::ios::binary|std::ios::trunc);corrupt.write(raw.data(),raw.size());}
        secret="unchanged";
        check(credentials::load(vault,secret,error)==credentials::Result::Error && secret=="unchanged","Tampered vault accepted or changed the current key");
        fs::remove(vault);

        // An invalid later field must not partially overwrite earlier fields.
        write_profile(profile, R"({"esp":false,"aim_sens":"broken"})");
        check(!loaded.load(profile.string(), &error) && !error.empty(), "Bad type accepted");
        check(loaded.esp && loaded.col_snapline == source.col_snapline && loaded.menu_key == VK_F7,
              "Invalid profile partially changed live state");
        for (const auto& invalid : {
            R"({"tb_head_margin":0.2})", R"({"tb_head_margin":1.1})", R"({"tb_interval_ms":-1})", R"({"tb_interval_ms":501})", R"({"tb_head_only":1})",
            R"({"display_aspect":4})", R"({"display_fit":2})", R"({"radar_sz":0})", R"({"esp_box_style":4})", R"({"menu_key":0})",
            R"({"esp_box_style":1.5})", R"({"col_ct":-1})", R"({"col_ct":4294967296})",
            R"({"aim_sens":1e400})", R"({"esp":1})", R"([])", "{broken"}) {
            write_profile(profile, invalid);
            check(!loaded.load(profile.string(), &error), "Invalid range or shape accepted");
            check(loaded.menu_key == VK_F7 && loaded.menu_open, "Failed load changed runtime state");
        }
        write_profile(profile, json{{"saved_key", std::string(256, 'x')}}.dump());
        check(!loaded.load(profile.string(), &error), "Oversized key accepted");
        write_profile(profile, json{{"saved_key", std::string("x\0y", 3)}}.dump());
        check(!loaded.load(profile.string(), &error), "Embedded NUL accepted");
        write_profile(profile, R"({"esp":false,"aim_sens":0.1,"saved_key":"LEGACY-TEST-KEY"})");
        check(loaded.load(profile.string(), &error), "Legacy partial profile rejected");
        check(std::string(loaded.saved_key)=="LEGACY-TEST-KEY","Legacy migration cannot read the old key");
        check(!loaded.esp && loaded.col_snapline == Config{}.col_snapline && loaded.aim_sens == 0.1f,
              "Legacy defaults were not restored");
        check(loaded.menu_open, "Loading a legacy profile hid the menu");
        check(loaded.tb_head_only && loaded.tb_head_margin==.8f && loaded.tb_interval_ms==0,"legacy profiles must inherit head-only defaults");
        check(loaded.display_aspect==0 && loaded.display_fit==0,"Legacy profiles must default to full-window automatic projection");

        check(source.save(profile.string(), &error), "Valid replacement failed");
        source.aim_sens = std::numeric_limits<float>::quiet_NaN();
        check(!source.save(profile.string(), &error), "Invalid save accepted");
        check(loaded.load(profile.string(), &error) && loaded.tb_release_ms == 77.f,
              "Failed save damaged the last good file");
        check(!loaded.load((folder / "missing.hbc").string(), &error) && !error.empty(),
              "Missing profile did not report failure");
        check(!loaded.save(folder.string(), &error) && !error.empty(), "Directory accepted as profile");
        fs::remove(profile);
        fs::remove(folder);
        std::cout << "PASS: config round-trip, legacy defaults, validation, transactional load/save\n";
        return 0;
    } catch (const std::exception& ex) {
        std::error_code ignored;
        fs::remove(folder / "settings.hbc", ignored);
        fs::remove(folder, ignored);
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
}
