#include "auth_local.hpp"
#include <Windows.h>
#include <winsqlite/winsqlite3.h>
#include <filesystem>

bool local_auth_mode() {
    wchar_t mode[32]{};
    GetPrivateProfileStringW(L"auth", L"mode", L"local", mode, 32,
        std::filesystem::absolute("HillBlack.ini").c_str());
    return _wcsicmp(mode, L"local") == 0;
}

AuthInfo validate_local_key(const std::string& key, const std::string& hwid) {
    AuthInfo result;
    result.hwid = hwid;
    result.message = "Nao foi possivel abrir o banco hillblack.db desta pasta.";
    sqlite3* db = nullptr;
    const auto path = std::filesystem::absolute("hillblack.db").u8string();
    if (sqlite3_open_v2(reinterpret_cast<const char*>(path.c_str()), &db,
        SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return result;
    }
    struct Close { sqlite3* p; ~Close() { sqlite3_exec(p, "ROLLBACK", nullptr, nullptr, nullptr); sqlite3_close(p); } } close{db};
    sqlite3_busy_timeout(db, 3000);
    if (sqlite3_exec(db, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK) return result;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db,
        "SELECT status,expires_at,COALESCE(hwid_bound,''),"
        "CASE WHEN expires_at='never' THEN 0 WHEN julianday(expires_at) IS NULL THEN 1 "
        "WHEN julianday(expires_at)<=julianday('now') THEN 1 ELSE 0 END "
        "FROM keys WHERE key_value=?", -1, &stmt, nullptr) != SQLITE_OK) return result;
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE) { result.result = AuthResult::BAD_KEY; result.message = "Key nao encontrada no painel local."; }
        return result;
    }
    auto text = [stmt](int col) { const auto* p = sqlite3_column_text(stmt, col); return p ? std::string(reinterpret_cast<const char*>(p)) : std::string{}; };
    const auto status = text(0);
    result.expires_at = text(1);
    const auto bound = text(2);
    const bool expired = sqlite3_column_int(stmt, 3) != 0;
    sqlite3_finalize(stmt);
    result.message.clear();
    if (status == "expired" || expired) { result.result = AuthResult::EXPIRED; return result; }
    if (status != "active") { result.result = AuthResult::BAD_KEY; result.message = "Key revogada ou inativa no painel."; return result; }
    if (!bound.empty() && bound != hwid) { result.result = AuthResult::HWID_MISMATCH; return result; }
    if (bound.empty()) {
        if (sqlite3_prepare_v2(db, "UPDATE keys SET hwid_bound=? WHERE key_value=?", -1, &stmt, nullptr) != SQLITE_OK) return result;
        sqlite3_bind_text(stmt, 1, hwid.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (!ok) return result;
    }
    if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK) return result;
    result.result = AuthResult::OK;
    result.message = "Key validada no banco do painel local.";
    return result;
}
