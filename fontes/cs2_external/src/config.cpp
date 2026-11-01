// language: C++20 / file: config.cpp / runtime: MSVC / target: Windows x64
#include "config.hpp"
#include "string_enc.hpp"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <type_traits>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
// One field list keeps legacy profile loading and saving symmetrical.
#define BOOL_FIELDS(X) \
    X(esp) X(esp_box) X(esp_skeleton) X(esp_snaplines) X(esp_distance) \
    X(esp_healthbar) X(esp_health_num) X(esp_weapon) X(esp_utilities) X(esp_vischeck) \
    X(aim) X(aim_ads_only) X(aim_fov_circle) X(silent_aim) X(rcs) X(radar) \
    X(no_flash) X(crosshair) X(ch_dot) X(triggerbot) X(spectator_list) X(bomb_timer) X(item_esp) X(radar_names)

#define NUMBER_FIELDS(X) \
    X(esp_box_style, 0, 2) X(esp_box_thick, 0.5, 4) X(esp_skel_thick, 0.5, 4) \
    X(esp_filter, 0, 2) X(esp_max_dist, 0, 1000) \
    X(col_ct, 0, 4294967295.0) X(col_tr, 0, 4294967295.0) \
    X(col_visible, 0, 4294967295.0) X(col_skeleton, 0, 4294967295.0) \
    X(col_snapline, 0, 4294967295.0) X(ch_col, 0, 4294967295.0) \
    X(aim_key, 1, 254) X(aim_bone, 0, 2) X(aim_fov, 1, 45) X(aim_smooth, 1, 15) \
    X(aim_target, 0, 1) X(aim_sens, 0.1, 5) X(aim_delay_ms, 0, 200) \
    X(sa_fov, 1, 30) X(sa_bone, 0, 2) X(rcs_intensity, 0.1, 2) \
    X(radar_x, 0, 100) X(radar_y, 0, 100) X(radar_sz, 80, 600) X(radar_scale, 50, 1000) X(radar_zoom, 1, 4) \
    X(no_flash_max, 0, 50) X(ch_size, 1, 20) X(ch_thick, 0.5, 4) X(ch_gap, 0, 10) \
    X(tb_key, 1, 254) X(tb_delay_ms, 0, 300) X(tb_release_ms, 10, 100) X(menu_key, 1, 254)

template<typename T>
void validate_number(const char* name, T value, double minimum, double maximum) {
    const double number = static_cast<double>(value);
    if (!std::isfinite(number) || number < static_cast<double>(static_cast<T>(minimum)) ||
        number > static_cast<double>(static_cast<T>(maximum)))
        throw std::runtime_error(std::string("Valor fora do intervalo: ") + name);
}

template<typename T>
void load_number(const json& j, const char* name, T& value, double minimum, double maximum) {
    const auto it = j.find(name);
    if (it == j.end()) return;
    if (!it->is_number())
        throw std::runtime_error(std::string("Valor numerico esperado: ") + name);
    const double number = it->get<double>();
    if (!std::isfinite(number) || number < std::min(minimum, static_cast<double>(static_cast<T>(minimum))) ||
        number > std::max(maximum, static_cast<double>(static_cast<T>(maximum))) ||
        (std::is_integral_v<T> && std::trunc(number) != number))
        throw std::runtime_error(std::string("Valor fora do intervalo: ") + name);
    value = static_cast<T>(number);
}

void load_bool(const json& j, const char* name, bool& value) {
    const auto it = j.find(name);
    if (it == j.end()) return;
    if (!it->is_boolean())
        throw std::runtime_error(std::string("Valor booleano esperado: ") + name);
    value = it->get<bool>();
}

void transform(std::string& bytes) {
    for (auto& c : bytes) c = static_cast<char>(static_cast<uint8_t>(c) ^ enc::KEY);
}

bool failure(std::string* error, const std::exception& ex) {
    if (error) *error = ex.what();
    return false;
}
}

bool Config::save(const std::string& path, std::string* error) const {
    if (error) error->clear();
    fs::path temporary;
    try {
        json j = json::object();
#define SAVE_BOOL(name) j[#name] = name;
        BOOL_FIELDS(SAVE_BOOL)
#undef SAVE_BOOL
#define SAVE_NUMBER(name, lo, hi) validate_number(#name, name, lo, hi); j[#name] = name;
        NUMBER_FIELDS(SAVE_NUMBER)
#undef SAVE_NUMBER
        const auto key_end = static_cast<const char*>(std::memchr(saved_key, '\0', sizeof(saved_key)));
        if (!key_end) throw std::runtime_error("Chave salva invalida.");
        j["saved_key"] = std::string(saved_key, key_end);
        std::string bytes = j.dump(2);
        transform(bytes);

        const fs::path destination = fs::absolute(fs::u8path(path));
        fs::create_directories(destination.parent_path());
        wchar_t temp_name[MAX_PATH]{};
        if (!GetTempFileNameW(destination.parent_path().c_str(), L"hbc", 0, temp_name))
            throw std::runtime_error("Nao foi possivel criar o arquivo temporario do perfil.");
        temporary = temp_name;
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) throw std::runtime_error("Falha ao gravar o perfil. Verifique o espaco em disco.");
        out.close();
        if (!out) throw std::runtime_error("Falha ao finalizar o perfil.");
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Nao foi possivel substituir o perfil. Verifique as permissoes.");
        return true;
    } catch (const std::exception& ex) {
        if (!temporary.empty()) {
            std::error_code ignored;
            fs::remove(temporary, ignored);
        }
        return failure(error, ex);
    }
}

bool Config::load(const std::string& path, std::string* error) {
    if (error) error->clear();
    try {
        std::ifstream in(fs::u8path(path), std::ios::binary | std::ios::ate);
        if (!in) throw std::runtime_error("Nao foi possivel abrir o perfil.");
        const auto length = in.tellg();
        if (length <= 0 || length > 1024 * 1024)
            throw std::runtime_error("O perfil esta vazio ou excede o limite de 1 MB.");
        std::string bytes(static_cast<size_t>(length), '\0');
        in.seekg(0);
        in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!in) throw std::runtime_error("Falha ao ler o perfil completo.");
        transform(bytes);
        const auto j = json::parse(bytes);
        if (!j.is_object()) throw std::runtime_error("Formato de perfil invalido.");

        Config next;
        next.menu_open = menu_open; // Runtime state is never loaded from profiles.
        next.game_status = game_status;
        next.game_ready = game_ready;
        next.write_available = write_available;
        next.trigger_status = trigger_status;
#define LOAD_BOOL(name) load_bool(j, #name, next.name);
        BOOL_FIELDS(LOAD_BOOL)
#undef LOAD_BOOL
#define LOAD_NUMBER(name, lo, hi) load_number(j, #name, next.name, lo, hi);
        NUMBER_FIELDS(LOAD_NUMBER)
#undef LOAD_NUMBER
        const auto key = j.value("saved_key", std::string{});
        if (key.size() >= sizeof(next.saved_key) || key.find('\0') != std::string::npos)
            throw std::runtime_error("Chave salva invalida.");
        std::memcpy(next.saved_key, key.c_str(), key.size() + 1);
        *this = next; // Commit only after the entire profile has been validated.
        return true;
    } catch (const std::exception& ex) {
        return failure(error, ex);
    }
}

#undef BOOL_FIELDS
#undef NUMBER_FIELDS
