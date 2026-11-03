#pragma once
#include "config.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <stdexcept>

namespace profile_store {
namespace fs=std::filesystem;
inline bool valid_name(const std::string& name) {
    return !name.empty() && name.size()<64 && name!="." && name!=".." && name.back()!=' ' &&
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ")==std::string::npos;
}
inline std::string file(const fs::path& folder,const std::string& name) {
    const auto utf8=(folder/(name+".hbc")).u8string();return {utf8.begin(),utf8.end()};
}
inline std::string selected(const fs::path& folder="configs") {
    std::ifstream in(folder/"active-profile.txt",std::ios::binary);
    char bytes[64]{};in.read(bytes,sizeof(bytes));
    std::string name(bytes,static_cast<size_t>(in.gcount()));
    return valid_name(name)?name:"default";
}
inline bool remember(const std::string& name,std::string* error=nullptr,const fs::path& folder="configs") {
    if(error)error->clear();fs::path temporary;
    try {
        if(!valid_name(name))throw std::runtime_error("Nome de perfil invalido.");
        const auto directory=fs::absolute(folder);fs::create_directories(directory);
        wchar_t temp_name[MAX_PATH]{};
        if(!GetTempFileNameW(directory.c_str(),L"hbp",0,temp_name))throw std::runtime_error("Nao foi possivel registrar o perfil selecionado.");
        temporary=temp_name;
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out.write(name.data(),name.size());out.flush();
        if(!out)throw std::runtime_error("Falha ao registrar o perfil selecionado.");
        out.close();if(!out)throw std::runtime_error("Falha ao finalizar o perfil selecionado.");
        if(!MoveFileExW(temporary.c_str(),(directory/"active-profile.txt").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Nao foi possivel lembrar o perfil para a proxima abertura.");
        return true;
    } catch(const std::exception& ex) {
        if(!temporary.empty()){std::error_code ignored;fs::remove(temporary,ignored);}
        if(error)*error=ex.what();return false;
    }
}
inline bool save(const Config& cfg,const std::string& name,std::string* error=nullptr,const fs::path& folder="configs") {
    if(!valid_name(name)){if(error)*error="Nome de perfil invalido.";return false;}
    if(!cfg.save(file(folder,name),error))return false;
    if(!remember(name,error,folder)) {
        if(error)*error="Os ajustes foram gravados, mas nao foi possivel lembrar o perfil: "+*error;
        return false;
    }
    return true;
}
inline bool load(Config& cfg,const std::string& name,std::string* error=nullptr,const fs::path& folder="configs") {
    if(!valid_name(name)){if(error)*error="Nome de perfil invalido.";return false;}
    Config next=cfg;
    if(!next.load(file(folder,name),error))return false;
    // Loading preferences never replaces the authenticated session's key.
    std::memcpy(next.saved_key,cfg.saved_key,sizeof(next.saved_key));
    if(!remember(name,error,folder))return false;
    cfg=std::move(next);return true;
}
inline bool startup(Config& cfg,std::string& name,std::string* error=nullptr,const fs::path& folder="configs") {
    if(error)error->clear();name=selected(folder);std::error_code ec;
    if(!fs::is_regular_file(folder/(name+".hbc"),ec))name="default";
    ec.clear();if(!fs::exists(folder/(name+".hbc"),ec))return true;
    // Startup still accepts a legacy saved key so credentials can migrate to DPAPI.
    return cfg.load(file(folder,name),error);
}
}
