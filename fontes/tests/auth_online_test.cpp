#include "auth.hpp"
#include <Windows.h>
#include <filesystem>
#include <string>
#include <iostream>
int main(int argc,char** argv) {
 if(argc!=3)return 2;
 const auto original=std::filesystem::current_path();
 const auto folder=std::filesystem::temp_directory_path()/("capri-online-test-"+std::to_string(GetCurrentProcessId()));
 std::filesystem::create_directories(folder);std::filesystem::current_path(folder);
 const auto ini=std::filesystem::absolute("HillBlack.ini");
 const std::string endpoint=argv[1];std::wstring url(endpoint.begin(),endpoint.end());
 WritePrivateProfileStringW(L"auth",L"mode",L"online",ini.c_str());WritePrivateProfileStringW(L"auth",L"endpoint",url.c_str(),ini.c_str());
 std::string key;std::getline(std::cin,key);Auth auth;const auto info=auth.validate_online(key);
 const std::string expected=argv[2];
 const bool passed=(expected=="UPDATE_REQUIRED" && info.result==AuthResult::UPDATE_REQUIRED) || (expected=="OK" && info.result==AuthResult::OK) || (expected=="BAD_KEY" && info.result==AuthResult::BAD_KEY) || (expected=="NET_ERROR" && info.result==AuthResult::NET_ERROR) || (expected=="EXPIRED" && info.result==AuthResult::EXPIRED);
 std::cout<<(passed?"PASS":"FAIL")<<" online authentication expected="<<expected<<" result="<<static_cast<int>(info.result)<<'\n';
 std::filesystem::remove(ini);std::filesystem::current_path(original);std::filesystem::remove(folder);return passed?0:1;
}
