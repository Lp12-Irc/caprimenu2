// language: C++20, file: memory.hpp, target: Windows 11 x64, MSVC
// read-only handle: PROCESS_VM_READ | PROCESS_QUERY_INFORMATION
// Write handle is opened separately for the implemented No Flash feature.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>

class Memory {
public:
    struct ReadStats { uint64_t calls=0,bytes=0,failures=0; };
    void profile_reads(bool enabled) { profile_reads_=enabled; }
    void reset_read_stats() const { read_calls_=0; read_bytes_=0; read_failures_=0; }
    ReadStats read_stats() const { return {read_calls_.load(),read_bytes_.load(),read_failures_.load()}; }
    bool read_buffer(uintptr_t address,void* destination,size_t size) const noexcept {
        if (!address || !handle_r_ || !destination || !size) return false;
        SIZE_T count=0;
        const bool ok=ReadProcessMemory(handle_r_,reinterpret_cast<LPCVOID>(address),destination,size,&count) && count==size;
        record_read(count,ok); return ok;
    }
    bool attach(const std::string& process_name);
    bool attach_write(const std::string& process_name); // opens second handle with VM_WRITE
    void detach();

    bool is_valid()       const {
        DWORD code = 0;
        return handle_r_ && GetExitCodeProcess(handle_r_, &code) && code == STILL_ACTIVE;
    }
    bool has_write()      const { return handle_w_ != nullptr; }
    DWORD pid()           const { return pid_; }

    template<typename T>
    [[nodiscard]] T read(uintptr_t addr) const noexcept {
        T v{};
        if (!read_into(addr, v)) return T{};
        return v;
    }

    template<typename T>
    bool read_into(uintptr_t addr, T& value) const noexcept {
        T result{};
        SIZE_T count = 0;
        if (!addr || !handle_r_) return false;
        const bool ok=ReadProcessMemory(handle_r_,reinterpret_cast<LPCVOID>(addr),&result,sizeof(T),&count) && count==sizeof(T);
        record_read(count,ok);
        if (!ok) return false;
        value = result;
        return true;
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
    void record_read(size_t bytes,bool success) const {
        if (profile_reads_) { read_calls_.fetch_add(1,std::memory_order_relaxed); read_bytes_.fetch_add(bytes,std::memory_order_relaxed); if(!success) read_failures_.fetch_add(1,std::memory_order_relaxed); }
    }
    bool profile_reads_=false;
    mutable std::atomic<uint64_t> read_calls_{0},read_bytes_{0},read_failures_{0};
    static std::vector<uint8_t> parse_pattern(const std::string& pat);

    HANDLE handle_r_ = nullptr;
    HANDLE handle_w_ = nullptr;
    DWORD  pid_      = 0;
};
