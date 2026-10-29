// language: C++20, file: auth.cpp, target: Windows 11 x64, MSVC
#include "auth.hpp"
#include "string_enc.hpp"
#include <Windows.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <intrin.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")

// Quick JSON field extractor — no full library needed in auth module
static std::string jfield(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos) return {};
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '"')) ++pos;
    auto end = pos;
    while (end < json.size() && json[end] != '"' && json[end] != ',' &&
           json[end] != '}') ++end;
    return json.substr(pos, end - pos);
}

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

std::string Auth::http_post(const std::wstring& host, const std::wstring& path,
                             const std::string& body) {
    HINTERNET session = WinHttpOpen(L"WinHTTP/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return {};

    HINTERNET conn = WinHttpConnect(session, host.c_str(),
                                    INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET req  = conn ? WinHttpOpenRequest(
        conn, L"POST", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE) : nullptr;

    std::string result;
    if (req) {
        // Ignore cert errors on self-signed VPS — remove in production if using valid cert
        DWORD opt = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                    SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &opt, sizeof(opt));

        BOOL ok = WinHttpSendRequest(req,
            L"Content-Type: application/json\r\n", (DWORD)-1,
            const_cast<char*>(body.c_str()),
            static_cast<DWORD>(body.size()),
            static_cast<DWORD>(body.size()), 0);

        if (ok && WinHttpReceiveResponse(req, nullptr)) {
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(req, &avail) && avail) {
                std::string chunk(avail, '\0');
                DWORD read = 0;
                WinHttpReadData(req, chunk.data(), avail, &read);
                chunk.resize(read);
                result += chunk;
            }
        }
        WinHttpCloseHandle(req);
    }
    if (conn) WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return result;
}

AuthInfo Auth::validate_online(const std::string& key) {
    AuthInfo info;
    info.hwid = get_hwid();

    // API host/path — XOR-encrypted; decrypted at runtime
    // Replace with your VPS address
    auto host = ENC("api.hillblack.cc").decrypt();  // change to your domain
    auto path = ENC("/validate").decrypt();
    std::wstring whost(host.begin(), host.end());
    std::wstring wpath(path.begin(), path.end());

    std::string body = "{\"key\":\"" + key + "\",\"hwid\":\"" + info.hwid + "\"}";
    std::string resp = http_post(whost, wpath, body);

    if (resp.empty()) { info.result = AuthResult::NET_ERROR; return info; }

    info.expires_at = jfield(resp, "expires_at");
    info.message    = jfield(resp, "message");

    std::string valid_str = jfield(resp, "valid");
    std::string reason    = jfield(resp, "reason");

    if (valid_str == "true") {
        info.result = AuthResult::OK;
    } else if (reason == "expired") {
        info.result = AuthResult::EXPIRED;
    } else if (reason == "hwid_mismatch") {
        info.result = AuthResult::HWID_MISMATCH;
    } else {
        info.result = AuthResult::BAD_KEY;
    }
    return info;
}

bool Auth::revalidate(const std::string& key, const std::string& hwid) {
    auto host = ENC("api.hillblack.cc").decrypt();
    std::wstring whost(host.begin(), host.end());
    std::string body = "{\"key\":\"" + key + "\",\"hwid\":\"" + hwid + "\"}";
    std::string resp = http_post(whost, L"/validate", body);
    return resp.find("\"valid\":true") != std::string::npos;
}
