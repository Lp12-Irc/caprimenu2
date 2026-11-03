// language: C++20, file: auth.cpp, target: Windows 11 x64, MSVC
#include "auth.hpp"
#include "version.hpp"
#include "json.hpp"
#include "auth_local.hpp"
#include "string_enc.hpp"
#include <Windows.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <intrin.h>
#include <filesystem>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")

std::string Auth::get_hwid() {
    // Volume serial of the C: drive.
    DWORD serial = 0;
    GetVolumeInformationW(L"C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);

    // CPUID brand
    int cpu[4]{};
    __cpuid(cpu, 1);
    uint32_t cpu_sig = static_cast<uint32_t>(cpu[0]) ^ static_cast<uint32_t>(cpu[3]);

    // First adapter MAC
    uint32_t mac_word = 0;
    {
        IP_ADAPTER_INFO buf[4]{};
        DWORD sz = sizeof(buf);
        if (GetAdaptersInfo(buf, &sz) == ERROR_SUCCESS) {
            mac_word = (buf[0].Address[0] << 24) | (buf[0].Address[1] << 16) |
                       (buf[0].Address[2] <<  8) |  buf[0].Address[3];
        }
    }

    char hwid[32]{};
    sprintf_s(hwid, "%08X%08X%08X", serial, cpu_sig, mac_word);
    return hwid;
}


static std::wstring auth_endpoint() {
    wchar_t value[2048]{};
    GetPrivateProfileStringW(L"auth",L"endpoint",L"",value,2048,std::filesystem::absolute("HillBlack.ini").c_str());
    return value;
}
std::string Auth::http_post(const std::wstring& url,const std::string& body) {
    URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
    parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(url.c_str(),0,ICU_REJECT_USERPWD,&parts))return {};
    const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    const bool secure=parts.nScheme==INTERNET_SCHEME_HTTPS;
    if(!secure && !(parts.nScheme==INTERNET_SCHEME_HTTP && (host==L"127.0.0.1" || host==L"localhost")))return {};
    std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);
    if(path.empty())path=L"/validate";
    if(parts.dwExtraInfoLength)return {};
    HINTERNET session=WinHttpOpen(L"Capri/1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session)return {};
    WinHttpSetTimeouts(session,5000,5000,5000,5000);
    HINTERNET connection=WinHttpConnect(session,host.c_str(),parts.nPort,0);
    HINTERNET request=connection ? WinHttpOpenRequest(connection,L"POST",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,secure?WINHTTP_FLAG_SECURE:0) : nullptr;
    std::string response;
    if(request) {
        DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
        BOOL sent=WinHttpSendRequest(request,L"Content-Type: application/json\r\n",DWORD(-1),const_cast<char*>(body.data()),DWORD(body.size()),DWORD(body.size()),0);
        if(sent && WinHttpReceiveResponse(request,nullptr)) {
            DWORD status=0,length=sizeof(status);
            WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX);
            if(status==200) {
                DWORD available=0;
                while(WinHttpQueryDataAvailable(request,&available) && available) {
                    if(response.size()+available>16384){response.clear();break;}
                    std::string chunk(available,'\0');DWORD count=0;
                    if(!WinHttpReadData(request,chunk.data(),available,&count) || !count){response.clear();break;}
                    chunk.resize(count);response+=chunk;
                }
            }
        }
        WinHttpCloseHandle(request);
    }
    if(connection)WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);return response;
}
AuthInfo Auth::validate_online(const std::string& key) {
    AuthInfo info;info.hwid=get_hwid();
    auto endpoint=auth_endpoint();
    if(endpoint.empty()){info.message="Servico de chaves nao configurado. Use o pacote atualizado enviado pelo administrador.";return info;}
    const auto response=http_post(endpoint,nlohmann::json{{"key",key},{"client_build",CAPRI_CLIENT_BUILD}}.dump());
    if(response.empty()){info.message="Nao foi possivel conectar ao servidor de chaves. O PC do administrador e o servico precisam estar ligados.";return info;}
    try {
        const auto data=nlohmann::json::parse(response);
        if(!data.is_object() || !data.contains("valid") || !data["valid"].is_boolean())return info;
        info.package_sha256=data.value("package_sha256",std::string{});info.package_size=data.value("package_size",std::uint64_t(0));
        info.latest_build=data.value("latest_build",0);info.min_build=data.value("min_build",0);
        info.version=data.value("version",std::string{});info.download_url=data.value("download_url",std::string{});info.release_notes=data.value("release_notes",std::string{});
        info.message=data.value("message",std::string{});
        info.expires_at=data.value("expires_at",std::string{});
        if(data["valid"].get<bool>())info.result=AuthResult::OK;
        else info.result=data.value("reason",std::string{})=="expired"?AuthResult::EXPIRED:AuthResult::BAD_KEY;
        if(info.min_build>CAPRI_CLIENT_BUILD || data.value("reason",std::string{})=="update_required")info.result=AuthResult::UPDATE_REQUIRED;
    }catch(...){info.result=AuthResult::NET_ERROR;}
    return info;
}
bool Auth::revalidate(const std::string& key,const std::string& hwid) {
    return validate_online(key).result==AuthResult::OK;
}
