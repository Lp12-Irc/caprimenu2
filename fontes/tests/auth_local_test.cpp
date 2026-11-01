#include "auth_local.hpp"
#include <winsqlite/winsqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <Windows.h>

int main() {
    auto initial = std::filesystem::current_path();
    auto temp = std::filesystem::temp_directory_path() / ("hillblack-auth-test-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directory(temp);
    std::filesystem::current_path(temp);
    int failures = 0;
    auto check = [&](bool condition, const char* label) { std::cout << (condition ? "PASS " : "FAIL ") << label << '\n'; if (!condition) ++failures; };
    sqlite3* db = nullptr;
    sqlite3_open("hillblack.db", &db);
    sqlite3_exec(db, "CREATE TABLE keys(key_value TEXT PRIMARY KEY,status TEXT,expires_at TEXT,hwid_bound TEXT);"
        "INSERT INTO keys VALUES('valid','active','never',''),('revoked','revoked','never',''),"
        "('expired','active','2000-01-01T00:00:00Z',''),('malformed','active','bad-date',''),"
        "('bound','active','never','other-machine'),('future','active','2099-01-01T00:00:00+00:00','');", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    check(local_auth_mode(), "local mode");
    check(validate_local_key("missing", "machine").result == AuthResult::BAD_KEY, "unknown rejected");
    check(validate_local_key("revoked", "machine").result == AuthResult::BAD_KEY, "revoked rejected");
    check(validate_local_key("expired", "machine").result == AuthResult::EXPIRED, "expired rejected");
    check(validate_local_key("malformed", "machine").result == AuthResult::EXPIRED, "bad expiry rejected");
    check(validate_local_key("bound", "machine").result == AuthResult::HWID_MISMATCH, "different HWID rejected");
    check(validate_local_key("valid", "machine").result == AuthResult::OK, "active accepted");
    check(validate_local_key("valid", "machine").result == AuthResult::OK, "same HWID accepted again");
    check(validate_local_key("valid", "other").result == AuthResult::HWID_MISMATCH, "initial binding persisted");
    check(validate_local_key("future", "machine").result == AuthResult::OK, "future expiry accepted");
    std::filesystem::remove("hillblack.db");
    check(validate_local_key("valid", "machine").result == AuthResult::NET_ERROR, "missing database rejected");
    std::filesystem::current_path(initial);
    std::filesystem::remove(temp);
    return failures ? 1 : 0;
}
