// language: C++20, file: auth.hpp, target: Windows 11 x64, MSVC
// HWID: VolumeSerial + CPUID[0] XOR'd + first MAC segment
// Online key validation via a configured HTTPS endpoint.
#pragma once
#include <string>
#include <cstdint>

enum class AuthResult { OK, BAD_KEY, EXPIRED, HWID_MISMATCH, NET_ERROR, UPDATE_REQUIRED };
inline constexpr int AUTH_POLL_MS=2000;
// A successful online validation permits a bounded window for network retries.
// Only transport failures can retain that grant; explicit denials revoke it.
inline constexpr int AUTH_LEASE_MS=60000;

struct AuthInfo {
    int latest_build=0,min_build=0;
    std::string package_sha256;
    std::uint64_t package_size=0;
    std::string version,download_url,release_notes;
    AuthResult result      = AuthResult::NET_ERROR;
    std::string expires_at = {};
    std::string message    = {};
    std::string hwid       = {};
};
inline bool auth_grant_active(const AuthInfo& access,std::uint64_t now,std::uint64_t deadline) {
    return access.result==AuthResult::OK && now<deadline;
}
inline bool auth_keep_previous_grant(const AuthInfo& previous,const AuthInfo& response,
                                     std::uint64_t now,std::uint64_t deadline) {
    return response.result==AuthResult::NET_ERROR && auth_grant_active(previous,now,deadline);
}

class Auth {
public:
    // Build HWID string from this machine
    static std::string get_hwid();

    // Online: POST /validate { key } — returns parsed result
    // URL comes from HillBlack.ini [auth] endpoint.
    AuthInfo validate_online(const std::string& key);

    // Periodic key and version check (every 2 seconds)
    bool revalidate(const std::string& key, const std::string& hwid);

private:
    static std::string http_post(const std::wstring& url,const std::string& body);
};
