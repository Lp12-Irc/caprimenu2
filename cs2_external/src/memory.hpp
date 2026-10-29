// language: C++20, file: memory.hpp, target: Windows 11 x64, MSVC
// read-only handle: PROCESS_VM_READ | PROCESS_QUERY_INFORMATION
// write handle opened separately only when needed (silent aim, no-flash)
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <cstdint>

class Memory {
public:
    bool attach(const std::string& process_name);
    bool attach_write(const std::string& process_name); // opens second handle with VM_WRITE
    void detach();

    bool is_valid()       const { return handle_r_ != nullptr; }
    bool has_write()      const { return handle_w_ != nullptr; }
    DWORD pid()           const { return pid_; }

    template<typename T>
    [[nodiscard]] T read(uintptr_t addr) const noexcept {
        T v{};
        ReadProcessMemory(handle_r_, reinterpret_cast<LPCVOID>(addr), &v, sizeof(T), nullptr);
        return v;
    }

    template<typename T>
    bool write(uintptr_t addr, const T& v) const noexcept {
        if (!handle_w_) return false;
        SIZE_T w = 0;
        return WriteProcessMemory(handle_w_, reinterpret_cast<LPVOID>(addr),
                                   &v, sizeof(T), &w) && w == sizeof(T);
    }

    std::vector<uint8_t> read_bytes(uintptr_t addr, size_t n) const noexcept;

    uintptr_t get_module_base(const std::string& mod) const;
    size_t    get_module_size(const std::string& mod) const;

    // Pattern scan: "48 8B 0D ? ? ? ? 48" — ?? or ? = wildcard
    // Returns VA of first match or 0
    uintptr_t pattern_scan(const std::string& mod, const std::string& pat) const;

    // RIP-resolve: read int32 at (addr + rel_offset), add (addr + total_len)
    uintptr_t resolve_rip(uintptr_t addr, int rel_off, int total_len) const;

    // Pointer chain: read ptr at base+offsets[0], then +offsets[1], ..., return final addr
    uintptr_t read_chain(uintptr_t base, std::initializer_list<uint32_t> offs) const;

private:
    static std::vector<uint8_t> parse_pattern(const std::string& pat);

    HANDLE handle_r_ = nullptr;
    HANDLE handle_w_ = nullptr;
    DWORD  pid_      = 0;
};
