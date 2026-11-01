#pragma once
#include "auth.hpp"
bool local_auth_mode();
AuthInfo validate_local_key(const std::string& key, const std::string& hwid);
