#pragma once
#include <Windows.h>
#include <shellapi.h>
#include <string>
#include "auth.hpp"
inline constexpr int CAPRI_CLIENT_BUILD=29;
inline constexpr const char* CAPRI_CLIENT_VERSION="1.8.3";
inline bool open_release_url(const std::string& url) {
    if(!url.starts_with("https://") || url.size()<=8 || url.find_first_of(" \r\n\t")!=std::string::npos)return false;
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,url.c_str(),-1,nullptr,0);if(!size)return false;
    std::wstring wide(size,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,url.c_str(),-1,wide.data(),size);
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",wide.c_str(),nullptr,nullptr,SW_SHOWNORMAL))>32;
}

inline bool launch_client_update(const AuthInfo& info) {
    if(info.package_sha256.size()!=64 || info.package_size==0 || info.package_size>104857600 || info.latest_build<CAPRI_CLIENT_BUILD || !info.download_url.starts_with("https://") || info.download_url.find_first_of("\" \r\n\t")!=std::string::npos)return false;
    for(char c:info.package_sha256)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
    wchar_t file[32768]{},system[MAX_PATH]{};
    if(!GetModuleFileNameW(nullptr,file,32768) || !GetSystemDirectoryW(system,MAX_PATH))return false;
    std::wstring base=file;auto slash=base.find_last_of(L"\\/");if(slash==std::wstring::npos)return false;base.resize(slash);
    const std::wstring helper=base+L"\\assets\\updater\\Capri.Updater.exe";
    const std::wstring work=base+L"\\atualizacao";
    const std::wstring runner=work+L"\\Capri.Updater.exe";
    if(GetFileAttributesW(helper.c_str())==INVALID_FILE_ATTRIBUTES)return false;
    if(!CreateDirectoryW(work.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    const DWORD work_attributes=GetFileAttributesW(work.c_str());
    const DWORD runner_attributes=GetFileAttributesW(runner.c_str());
    if(work_attributes==INVALID_FILE_ATTRIBUTES || (work_attributes&FILE_ATTRIBUTE_REPARSE_POINT) ||
       (runner_attributes!=INVALID_FILE_ATTRIBUTES && (runner_attributes&FILE_ATTRIBUTE_REPARSE_POINT)))return false;
    // Run from the update cache so the installed helper can itself be upgraded.
    if(!CopyFileW(helper.c_str(),runner.c_str(),FALSE))return false;
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,info.download_url.c_str(),-1,nullptr,0);if(!size)return false;
    std::wstring url(size,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,info.download_url.c_str(),-1,url.data(),size);url.resize(size-1);
    auto quote=[](const std::wstring& value){return L"\""+value+L"\"";};
    std::wstring command=quote(runner)+L" --root "+quote(base)+L" --url "+quote(url)+L" --sha "+std::wstring(info.package_sha256.begin(),info.package_sha256.end())+L" --build "+std::to_wstring(info.latest_build)+L" --size "+std::to_wstring(info.package_size)+L" --wait-pid "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW start{};start.cb=sizeof(start);PROCESS_INFORMATION process{};
    if(!CreateProcessW(runner.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,base.c_str(),&start,&process))return false;
    CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
}
