// language: C++20, file: memory.hpp, target: Windows 11 x64, MSVC
// Leitura via NtReadVirtualMemory com stub de syscall indireta — sem
// ReadProcessMemory na IAT do executavel. Cache de base/tamanho de modulo
// evita CreateToolhelp32Snapshot repetido durante a partida.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>
#include <winternl.h>
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <unordered_map>

// ── Syscall indireta NtReadVirtualMemory ─────────────────────────────────────
// Busca o SSN no ntdll em runtime e executa o syscall a partir de um stub
// alocado em memoria anonima — fora de qualquer modulo mapeado.
// VAC Live rastreia chamadas por modulo de origem; stub inline nao pertence
// a nenhum modulo e nao aparece na tabela de chamadas do jogo.
namespace indirect_syscall {

inline uint16_t get_ssn(const char* func_name) {
    auto* ntdll = reinterpret_cast<uint8_t*>(GetModuleHandleA("ntdll.dll"));
    if (!ntdll) return 0xFFFF;
    auto* dos   = reinterpret_cast<IMAGE_DOS_HEADER*>(ntdll);
    auto* nt    = reinterpret_cast<IMAGE_NT_HEADERS*>(ntdll + dos->e_lfanew);
    auto& dir   = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    auto* exp   = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(ntdll + dir.VirtualAddress);
    auto* names = reinterpret_cast<uint32_t*>(ntdll + exp->AddressOfNames);
    auto* ords  = reinterpret_cast<uint16_t*>(ntdll + exp->AddressOfNameOrdinals);
    auto* fns   = reinterpret_cast<uint32_t*>(ntdll + exp->AddressOfFunctions);
    for (uint32_t i = 0; i < exp->NumberOfNames; ++i) {
        if (strcmp(reinterpret_cast<const char*>(ntdll + names[i]), func_name) != 0) continue;
        auto* stub = ntdll + fns[ords[i]];
        // prologue sem hook: 4C 8B D1  B8 <ssn16> ...
        if (stub[0]==0x4C && stub[1]==0x8B && stub[2]==0xD1)
            return *reinterpret_cast<uint16_t*>(stub + 4);
        // hook presente (JMP no inicio) — halo scan: vizinho abaixo tem SSN base+delta
        if (stub[0] == 0xE9) {
            for (int d = 1; d <= 32; ++d) {
                auto* n = stub - d * 32;
                if (n < ntdll) break;
                if (n[0]==0x4C && n[1]==0x8B && n[2]==0xD1)
                    return static_cast<uint16_t>(*reinterpret_cast<uint16_t*>(n+4) + d);
            }
        }
        return 0xFFFF;
    }
    return 0xFFFF;
}

// Aloca stub executavel em memoria anonima (PAGE_EXECUTE_READ apos escrita).
// Layout: mov r10,rcx | mov eax,ssn | syscall | ret
inline void* make_stub(uint16_t ssn) {
    auto* mem = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!mem) return nullptr;
    mem[0]=0x4C; mem[1]=0x8B; mem[2]=0xD1;
    mem[3]=0xB8;
    *reinterpret_cast<uint16_t*>(mem+4) = ssn;
    mem[6]=0x00; mem[7]=0x00;
    mem[8]=0x0F; mem[9]=0x05;
    mem[10]=0xC3;
    DWORD old; VirtualProtect(mem, 32, PAGE_EXECUTE_READ, &old);
    return mem;
}

} // namespace indirect_syscall

using NtReadVM_t = NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

class Memory {
public:
    struct ReadStats { uint64_t calls=0,bytes=0,failures=0; };
    void profile_reads(bool enabled) { profile_reads_=enabled; }
    void reset_read_stats() const { read_calls_=0; read_bytes_=0; read_failures_=0; }
    ReadStats read_stats() const { return {read_calls_.load(),read_bytes_.load(),read_failures_.load()}; }

    bool read_buffer(uintptr_t address, void* destination, size_t size) const noexcept {
        if (!address || !handle_r_ || !destination || !size) return false;
        SIZE_T count = 0; bool ok = false;
        if (nt_read_) {
            ok = (nt_read_(handle_r_, reinterpret_cast<PVOID>(address),
                            destination, size, &count) == 0) && count == size;
        } else {
            ok = ReadProcessMemory(handle_r_, reinterpret_cast<LPCVOID>(address),
                                   destination, size, &count) && count == size;
        }
        record_read(count, ok);
        return ok;
    }

    bool attach(const std::string& process_name);
    bool attach_write(const std::string& process_name);
    void detach();
    void cache_modules(); // chamar UMA vez apos attach(), nunca durante a partida

    bool is_valid() const {
        DWORD code = 0;
        return handle_r_ && GetExitCodeProcess(handle_r_, &code) && code == STILL_ACTIVE;
    }
    bool has_write() const { return handle_w_ != nullptr; }
    DWORD pid()      const { return pid_; }

    template<typename T>
    [[nodiscard]] T read(uintptr_t addr) const noexcept {
        T v{}; read_into(addr, v); return v;
    }
    template<typename T>
    bool read_into(uintptr_t addr, T& value) const noexcept {
        T result{};
        if (!read_buffer(addr, &result, sizeof(T))) return false;
        value = result; return true;
    }
    template<typename T>
    bool write(uintptr_t addr, const T& v) const noexcept {
        if (!handle_w_) return false;
        SIZE_T w = 0;
        return WriteProcessMemory(handle_w_, reinterpret_cast<LPVOID>(addr),
                                   &v, sizeof(T), &w) && w == sizeof(T);
    }

    std::vector<uint8_t> read_bytes(uintptr_t addr, size_t n) const noexcept;
    uintptr_t get_module_base(const std::string& mod) const; // usa cache
    size_t    get_module_size(const std::string& mod) const; // usa cache
    uintptr_t pattern_scan(const std::string& mod, const std::string& pat) const;
    uintptr_t resolve_rip(uintptr_t addr, int rel_off, int total_len) const;
    uintptr_t read_chain(uintptr_t base, std::initializer_list<uint32_t> offs) const;

private:
    void record_read(size_t bytes, bool success) const {
        if (!profile_reads_) return;
        read_calls_.fetch_add(1, std::memory_order_relaxed);
        read_bytes_.fetch_add(bytes, std::memory_order_relaxed);
        if (!success) read_failures_.fetch_add(1, std::memory_order_relaxed);
    }
    static std::vector<uint8_t> parse_pattern(const std::string& pat);
    void init_syscall_stub();

    bool   profile_reads_ = false;
    mutable std::atomic<uint64_t> read_calls_{0}, read_bytes_{0}, read_failures_{0};

    HANDLE handle_r_ = nullptr;
    HANDLE handle_w_ = nullptr;
    DWORD  pid_      = 0;

    struct ModInfo { uintptr_t base = 0; size_t size = 0; };
    mutable std::unordered_map<std::string, ModInfo> mod_cache_;

    NtReadVM_t nt_read_ = nullptr;
    void*      nt_stub_ = nullptr;
};
