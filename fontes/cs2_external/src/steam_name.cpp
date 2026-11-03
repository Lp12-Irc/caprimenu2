#include "steam_name.hpp"
#include "json.hpp"
#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>
#include <fstream>
namespace steam_name {
namespace {
    bool started=false;std::string launch_error;
    std::filesystem::path status_file(){PWSTR raw=nullptr;std::filesystem::path path;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&raw))){path=std::filesystem::path(raw)/"Capri"/"SteamConnect"/"nickname-status.json";CoTaskMemFree(raw);}return path;}
}
bool begin(bool retry){
    if(started && !retry)return true;
    wchar_t executable[32768]{};if(!GetModuleFileNameW(nullptr,executable,32768))return false;
    const auto folder=std::filesystem::path(executable).parent_path()/"assets"/"steam";
    const auto helper=folder/"Capri.Steam.exe";
    if(GetFileAttributesW(helper.c_str())==INVALID_FILE_ATTRIBUTES){launch_error="Conexao Steam indisponivel. Reinstale o pacote completo.";return false;}
    std::wstring command=L"\""+helper.wstring()+L"\" --activate";
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
    if(!CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&process)){launch_error="Nao foi possivel abrir a conexao Steam.";return false;}
    CloseHandle(process.hThread);CloseHandle(process.hProcess);started=true;launch_error.clear();return true;
}
std::string status(){
    if(!launch_error.empty())return launch_error;
    static ULONGLONG next=0;static std::string cached="Conectando Steam...";if(GetTickCount64()<next)return cached;next=GetTickCount64()+700;
    try{
        const auto path=status_file();std::error_code error;const auto size=std::filesystem::file_size(path,error);if(error || size>4096)return cached;
        std::ifstream file(path);nlohmann::json data;file>>data;const auto state=data.value("state",std::string{});
        if(state=="done")cached="Nome __proton__ confirmado na Steam.";
        else if(state=="login_required")cached="Entre na sua conta na janela Conectar Steam.";
        else if(state=="renaming" || state=="verifying")cached="Aplicando e confirmando __proton__ na Steam...";
        else if(state=="cancelled")cached="Conexao cancelada. Use Conectar Steam para tentar novamente.";
        else if(state=="runtime_missing")cached="Instale o Microsoft Edge WebView2 Runtime para conectar a Steam.";
        else if(state=="steam_not_running")cached="Entre no aplicativo Steam antes de conectar.";
        else if(state=="wrong_account")cached="Use a mesma conta que esta ativa no aplicativo Steam.";
        else if(state=="form_unavailable" || state=="not_saved" || state=="error" || state=="network_error")cached="A Steam nao confirmou a troca. Abra Conectar Steam para conferir.";
        else cached="Conectando ao perfil Steam...";
    }catch(...){/* A simultaneous status write is retried on the next poll. */}
    return cached;
}
}
