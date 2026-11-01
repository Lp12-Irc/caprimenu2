// language: C++20, file: memory.cpp, target: Windows 11 x64, MSVC
#include "memory.hpp"
#include <sstream>
#include <Psapi.h>

bool Memory::attach(const std::string& process_name) {
    detach();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W e{ .dwSize = sizeof(e) };
    std::wstring target(process_name.begin(), process_name.end());

    if (Process32FirstW(snap, &e)) {
        do {
            if (target == e.szExeFile) { pid_ = e.th32ProcessID; break; }
        } while (Process32NextW(snap, &e));
    }
    CloseHandle(snap);
    if (!pid_) return false;

    handle_r_ = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid_);
    return handle_r_ != nullptr;
}

bool Memory::attach_write(const std::string& process_name) {
    if (handle_w_) return true;
    if (!pid_) return false;
    handle_w_ = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid_);
    return handle_w_ != nullptr;
}

void Memory::detach() {
    if (handle_r_) { CloseHandle(handle_r_); handle_r_ = nullptr; }
    if (handle_w_) { CloseHandle(handle_w_); handle_w_ = nullptr; }
    pid_ = 0;
}

std::vector<uint8_t> Memory::read_bytes(uintptr_t addr, size_t n) const noexcept {
    std::vector<uint8_t> buf(n);
    SIZE_T r = 0;
    const bool ok=ReadProcessMemory(handle_r_,reinterpret_cast<LPCVOID>(addr),buf.data(),n,&r)!=0;
    record_read(r,ok && r==n);
    buf.resize(r);
    return buf;
}

uintptr_t Memory::get_module_base(const std::string& mod) const {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W e{ .dwSize = sizeof(e) };
    std::wstring target(mod.begin(), mod.end());
    uintptr_t result = 0;

    if (Module32FirstW(snap, &e)) {
        do {
            if (target == e.szModule) {
                result = reinterpret_cast<uintptr_t>(e.modBaseAddr);
                break;
            }
        } while (Module32NextW(snap, &e));
    }
    CloseHandle(snap);
    return result;
}

size_t Memory::get_module_size(const std::string& mod) const {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W e{ .dwSize = sizeof(e) };
    std::wstring target(mod.begin(), mod.end());
    size_t result = 0;

    if (Module32FirstW(snap, &e)) {
        do {
            if (target == e.szModule) { result = e.modBaseSize; break; }
        } while (Module32NextW(snap, &e));
    }
    CloseHandle(snap);
    return result;
}

std::vector<uint8_t> Memory::parse_pattern(const std::string& pat) {
    // 0xCC = wildcard sentinel (never appears in valid x64 code paths we scan)
    std::vector<uint8_t> bytes;
    std::istringstream ss(pat);
    std::string tok;
    while (ss >> tok) {
        bytes.push_back((tok == "?" || tok == "??")
            ? 0xCC
            : static_cast<uint8_t>(std::stoul(tok, nullptr, 16)));
    }
    return bytes;
}

uintptr_t Memory::pattern_scan(const std::string& mod, const std::string& pat) const {
    uintptr_t base = get_module_base(mod);
    size_t    size = get_module_size(mod);
    if (!base || !size) return 0;

    auto module_bytes = read_bytes(base, size);
    if (module_bytes.empty()) return 0;

    auto pat_bytes = parse_pattern(pat);
    size_t pat_len = pat_bytes.size();

    for (size_t i = 0; i + pat_len <= module_bytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < pat_len; ++j) {
            if (pat_bytes[j] != 0xCC && module_bytes[i + j] != pat_bytes[j]) {
                found = false;
                break;
            }
        }
        if (found) return base + i;
    }
    return 0;
}

uintptr_t Memory::resolve_rip(uintptr_t addr, int rel_off, int total_len) const {
    int32_t rel = read<int32_t>(addr + rel_off);
    return addr + total_len + rel;
}

uintptr_t Memory::read_chain(uintptr_t base, std::initializer_list<uint32_t> offs) const {
    if (!base || offs.size() == 0) return base;
    uintptr_t cur = base;
    auto it = offs.begin();
    for (size_t i = 0; i < offs.size() - 1; ++i, ++it) {
        cur = read<uintptr_t>(cur + *it);
        if (!cur) return 0;
    }
    return cur + *it;
}
