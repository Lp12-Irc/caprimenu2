// language: C++20, file: memory.cpp, target: Windows 11 x64, MSVC
#include "memory.hpp"
#include <sstream>
#include <Psapi.h>

void Memory::init_syscall_stub() {
    uint16_t ssn = indirect_syscall::get_ssn("NtReadVirtualMemory");
    if (ssn == 0xFFFF) return;
    nt_stub_ = indirect_syscall::make_stub(ssn);
    if (nt_stub_) nt_read_ = reinterpret_cast<NtReadVM_t>(nt_stub_);
}

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
    if (!handle_r_) return false;
    init_syscall_stub(); // substitui ReadProcessMemory por syscall indireta
    cache_modules();     // snapshot unico — nunca mais durante a partida
    return true;
}

bool Memory::attach_write(const std::string& process_name) {
    if (handle_w_) return true;
    if (!pid_) return false;
    (void)process_name;
    handle_w_ = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid_);
    return handle_w_ != nullptr;
}

void Memory::detach() {
    if (handle_r_) { CloseHandle(handle_r_); handle_r_ = nullptr; }
    if (handle_w_) { CloseHandle(handle_w_); handle_w_ = nullptr; }
    if (nt_stub_)  { VirtualFree(nt_stub_, 0, MEM_RELEASE); nt_stub_ = nullptr; }
    nt_read_ = nullptr;
    mod_cache_.clear();
    pid_ = 0;
}

// Snapshot unico de modulos — zero Toolhelp durante a partida.
// VAC Live detecta CreateToolhelp32Snapshot repetido como assinatura de cheat externo.
void Memory::cache_modules() {
    mod_cache_.clear();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_);
    if (snap == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32W e{ .dwSize = sizeof(e) };
    if (Module32FirstW(snap, &e)) {
        do {
            int len = WideCharToMultiByte(CP_UTF8, 0, e.szModule, -1, nullptr, 0, nullptr, nullptr);
            if (len > 1) {
                std::string name(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, e.szModule, -1, name.data(), len, nullptr, nullptr);
                mod_cache_[name] = { reinterpret_cast<uintptr_t>(e.modBaseAddr), e.modBaseSize };
            }
        } while (Module32NextW(snap, &e));
    }
    CloseHandle(snap);
}

uintptr_t Memory::get_module_base(const std::string& mod) const {
    auto it = mod_cache_.find(mod);
    return it != mod_cache_.end() ? it->second.base : 0;
}

size_t Memory::get_module_size(const std::string& mod) const {
    auto it = mod_cache_.find(mod);
    return it != mod_cache_.end() ? it->second.size : 0;
}

std::vector<uint8_t> Memory::read_bytes(uintptr_t addr, size_t n) const noexcept {
    std::vector<uint8_t> buf(n); SIZE_T r = 0; bool ok = false;
    if (nt_read_) {
        ok = (nt_read_(handle_r_, reinterpret_cast<PVOID>(addr), buf.data(), n, &r) == 0) && r == n;
    } else {
        ok = ReadProcessMemory(handle_r_, reinterpret_cast<LPCVOID>(addr), buf.data(), n, &r) != 0;
    }
    record_read(r, ok && r == n);
    buf.resize(r);
    return buf;
}

std::vector<uint8_t> Memory::parse_pattern(const std::string& pat) {
    std::vector<uint8_t> bytes;
    std::istringstream ss(pat); std::string tok;
    while (ss >> tok)
        bytes.push_back((tok == "?" || tok == "??")
            ? 0xCC
            : static_cast<uint8_t>(std::stoul(tok, nullptr, 16)));
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
        for (size_t j = 0; j < pat_len; ++j)
            if (pat_bytes[j] != 0xCC && module_bytes[i+j] != pat_bytes[j]) { found=false; break; }
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
