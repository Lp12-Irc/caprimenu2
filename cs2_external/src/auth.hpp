// language: C++20, file: auth.hpp, target: Windows 11 x64, MSVC
// HWID: VolumeSerial + CPUID[0] XOR'd + first MAC segment
// Online validation via WinHTTP POST to /validate
#pragma once
#include <string>
#include <cstdint>

enum class AuthResult { OK, BAD_KEY, EXPIRED, HWID_MISMATCH, NET_ERROR };

struct AuthInfo {
    AuthResult result      = AuthResult::NET_ERROR;
    std::string expires_at = {};
    std::string message    = {};
    std::string hwid       = {};
};

class Auth {
public:
    // Build HWID string from this machine
    static std::string get_hwid();

    // Online: POST /validate { key, hwid } — returns parsed result
    // URL is compile-time XOR'd, decrypted on first call
    AuthInfo validate_online(const std::string& key);

    // Periodic re-check (call every 5 min from watchdog thread)
    bool revalidate(const std::string& key, const std::string& hwid);

private:
    static std::string http_post(const std::wstring& host, const std::wstring& path,
                                  const std::string& body);
};
