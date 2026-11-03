#pragma once
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>

class ProfileCatalog {
    std::uint64_t next_=0,scans_=0;
    std::vector<std::string> names_;
    std::string error_;
public:
    void invalidate(){next_=0;}
    bool refresh(std::uint64_t now,const std::filesystem::path& folder="configs") {
        if(now<next_)return false;
        next_=now+2000;++scans_;names_.clear();error_.clear();
        try {
            std::error_code ec;
            std::filesystem::directory_iterator it(folder,ec),end;
            if(ec){error_="Nao foi possivel listar os perfis.";return true;}
            for(;it!=end;it.increment(ec)) {
                if(ec){error_="Falha ao listar perfis.";names_.clear();break;}
                if(it->is_regular_file(ec) && it->path().extension()==".hbc")names_.push_back(it->path().stem().string());
            }
            std::sort(names_.begin(),names_.end());
        }catch(...){names_.clear();error_="Nao foi possivel ler os nomes dos perfis.";}
        return true;
    }
    const auto& names()const{return names_;}
    const auto& error()const{return error_;}
    std::uint64_t scans()const{return scans_;}
};
