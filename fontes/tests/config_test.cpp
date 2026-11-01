// language: C++20 / file: config_test.cpp / runtime: MSVC / target: Windows x64
#include "config.hpp"
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
        Config source;
        source.col_visible = COL(1, 2, 3, 4);
        source.col_skeleton = COL(5, 6, 7, 8);
        source.col_snapline = COL(9, 10, 11, 12);
        source.ch_col = COL(13, 14, 15, 16);
        source.tb_release_ms = 77.f;
        source.aim_sens = 0.1f;
        source.rcs_intensity = 0.1f;
        source.esp_box_style = 2;
        source.menu_key = VK_F7;
        source.radar_sz=440; source.radar_zoom=2.25f; source.radar_names=false;
        strcpy_s(source.saved_key, "TEST-PROFILE-NOT-A-LICENSE");
        check_result(source.save(profile.string(), &error), error);
        Config loaded;
        loaded.menu_open = true;
        check_result(loaded.load(profile.string(), &error), error);
        check(loaded.col_visible == source.col_visible && loaded.col_skeleton == source.col_skeleton &&
              loaded.col_snapline == source.col_snapline && loaded.ch_col == source.ch_col,
              "Color round-trip regression");
        check(loaded.tb_release_ms == 77.f && loaded.aim_sens == 0.1f && loaded.rcs_intensity == 0.1f,
              "Timing or float-bound round-trip regression");
        check(loaded.menu_open && loaded.menu_key == VK_F7, "Runtime menu state changed");
        check(loaded.radar_sz==440 && loaded.radar_zoom==2.25f && !loaded.radar_names,"map radar preferences survive profile round-trip");
        check(std::string(loaded.saved_key) == source.saved_key, "Saved key changed");

        // An invalid later field must not partially overwrite earlier fields.
        write_profile(profile, R"({"esp":false,"aim_sens":"broken"})");
        check(!loaded.load(profile.string(), &error) && !error.empty(), "Bad type accepted");
        check(loaded.esp && loaded.col_snapline == source.col_snapline && loaded.menu_key == VK_F7,
              "Invalid profile partially changed live state");
        for (const auto& invalid : {
            R"({"radar_scale":0})", R"({"esp_box_style":4})", R"({"menu_key":0})",
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
        write_profile(profile, R"({"esp":false,"aim_sens":0.1})");
        check(loaded.load(profile.string(), &error), "Legacy partial profile rejected");
        check(!loaded.esp && loaded.col_snapline == Config{}.col_snapline && loaded.aim_sens == 0.1f,
              "Legacy defaults were not restored");
        check(loaded.menu_open, "Loading a legacy profile hid the menu");

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
