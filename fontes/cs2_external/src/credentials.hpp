#pragma once
#include <Windows.h>
#include <wincrypt.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#pragma comment(lib,"crypt32.lib")

namespace credentials {
enum class Result { Missing, Loaded, Error };
inline DATA_BLOB entropy() {
    static char value[]="Capri credentials v1";
    return {static_cast<DWORD>(sizeof(value)-1),reinterpret_cast<BYTE*>(value)};
}
inline bool save(const std::filesystem::path& path,const std::string& key,std::string& error) {
    if(key.empty() || key.size()>255 || key.find('\0')!=std::string::npos) {error="Chave invalida para armazenamento.";return false;}
    DATA_BLOB input{static_cast<DWORD>(key.size()),reinterpret_cast<BYTE*>(const_cast<char*>(key.data()))},output{};
    auto extra=entropy();
    if(!CryptProtectData(&input,L"Capri",&extra,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)) {error="Nao foi possivel proteger a chave neste usuario do Windows.";return false;}
    std::filesystem::path temporary;
    bool ok=false;
    try {
        const auto destination=std::filesystem::absolute(path);
        std::filesystem::create_directories(destination.parent_path());
        wchar_t name[MAX_PATH]{};
        if(!GetTempFileNameW(destination.parent_path().c_str(),L"cpr",0,name))throw 0;
        temporary=name;
        std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
        file.write(reinterpret_cast<char*>(output.pbData),output.cbData);file.flush();
        if(!file)throw 0;
        file.close();if(!file)throw 0;
        if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw 0;
        ok=true;
    } catch(...) {error="Nao foi possivel gravar a chave protegida.";}
    SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);
    if(!ok && !temporary.empty()){std::error_code ignored;std::filesystem::remove(temporary,ignored);}
    return ok;
}
inline Result load(const std::filesystem::path& path,std::string& key,std::string& error) {
    std::error_code ec;
    const bool exists=std::filesystem::exists(path,ec);
    if(!exists && !ec)return Result::Missing;
    if(ec){error="Nao foi possivel acessar a chave protegida.";return Result::Error;}
    try {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        const auto size=file.tellg();
        if(!file || size<=0 || size>16384)throw 0;
        std::vector<BYTE> bytes(static_cast<size_t>(size));file.seekg(0);
        file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!file)throw 0;
        DATA_BLOB input{static_cast<DWORD>(bytes.size()),bytes.data()},output{};auto extra=entropy();
        if(!CryptUnprotectData(&input,nullptr,&extra,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))throw 0;
        const bool valid=output.cbData>0 && output.cbData<=255 && !memchr(output.pbData,0,output.cbData);
        if(valid)key.assign(reinterpret_cast<char*>(output.pbData),output.cbData);
        SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);
        if(!valid)throw 0;
        return Result::Loaded;
    }catch(...){error="Nao foi possivel abrir a chave protegida. Informe sua chave novamente neste Windows.";return Result::Error;}
}
}
