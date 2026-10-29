// language: C++20, file: main.cpp, target: Windows 11 x64, MSVC
// Entry: wait for cs2.exe → resolve patterns → spawn cache thread → render loop
#include "string_enc.hpp"
#include "anti_debug.hpp"
#include "memory.hpp"
#include "offsets.hpp"
#include "entities.hpp"
#include "overlay.hpp"
#include "esp.hpp"
#include "aimbot.hpp"
#include "misc.hpp"
#include "menu.hpp"
#include "config.hpp"
#include "auth.hpp"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <thread>
#include <atomic>
#include <random>
#include <Windows.h>

static std::atomic<bool> g_running{true};

static void resolve_offsets(Memory& mem, EntityCache& cache) {
    auto client = ENC("client.dll").decrypt();

    // EntitySystem
    uintptr_t es_sig = mem.pattern_scan(client, patterns::ENTITY_SYSTEM);
    cache.ent_system  = es_sig ? mem.resolve_rip(es_sig, 3, 7) : 0;

    // LocalPlayerController
    uintptr_t lp_sig  = mem.pattern_scan(client, patterns::LOCAL_PLAYER_CTRL);
    cache.local_ctrl   = lp_sig ? mem.resolve_rip(lp_sig, 3, 7) : 0;

    // ViewMatrix
    uintptr_t vm_sig  = mem.pattern_scan(client, patterns::VIEW_MATRIX);
    cache.vm_addr      = vm_sig ? mem.resolve_rip(vm_sig, 3, 7) : 0;

    // PlantedC4
    uintptr_t c4_sig  = mem.pattern_scan(client, patterns::PLANTED_C4);
    cache.c4_list      = c4_sig ? mem.resolve_rip(c4_sig, 3, 7) : 0;
}

static void entity_thread_fn(EntityCache& cache,
                              const std::atomic<bool>& running) {
    std::mt19937 rng(GetTickCount());
    std::uniform_int_distribution<int> jitter(10, 17); // 60-100Hz jitter

    while (running.load()) {
        cache.update();
        Sleep(jitter(rng)); // randomized interval — avoid 1000Hz constant pattern
        anti_debug::watchdog_tick(); // silent exit if debugger attaches mid-session
    }
}

static bool show_login(Config& cfg, Auth& auth, AuthInfo& info_out) {
    // Simple console or ImGui login before overlay init
    // For overlay-integrated login: show ImGui window before ESP is active
    char key_buf[256]{};
    strncpy_s(key_buf, cfg.saved_key, sizeof(key_buf)-1);

    // If saved key present, validate immediately
    if (key_buf[0]) {
        auto info = auth.validate_online(key_buf);
        if (info.result == AuthResult::OK) {
            info_out = info;
            return true;
        }
    }
    // Otherwise prompt (simplified — in real build, show ImGui login window)
    // This blocks until valid key or exit
    // TODO: integrate into overlay as pre-ESP modal window
    return false;
}

int WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // ── Anti-debug ──────────────────────────────────────────────────────────
    anti_debug::run();

    // ── Config ──────────────────────────────────────────────────────────────
    Config cfg;
    cfg.load("configs/default.hbc");

    // ── Auth ─────────────────────────────────────────────────────────────────
    Auth    auth;
    AuthInfo auth_info;
    auth_info = auth.validate_online(cfg.saved_key);
    if (auth_info.result != AuthResult::OK) {
        // Show error + prompt — simplified: exit
        MessageBoxW(nullptr, L"Invalid key or expired.", L"HillBlack", MB_ICONERROR);
        return 1;
    }

    // ── Wait for CS2 ─────────────────────────────────────────────────────────
    Memory mem;
    while (!mem.attach(ENC("cs2.exe").decrypt())) Sleep(1000);

    // ── Resolve offsets ───────────────────────────────────────────────────────
    EntityCache cache;
    cache.init(&mem);
    resolve_offsets(mem, cache);

    // Optional write handle (silent aim / no-flash)
    if (cfg.silent_aim || cfg.no_flash)
        mem.attach_write(ENC("cs2.exe").decrypt());

    // ── Overlay ───────────────────────────────────────────────────────────────
    Overlay overlay;
    if (!overlay.create(ENC("Counter-Strike 2").decrypt())) return 1;

    // ── ImGui ─────────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't write imgui.ini

    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4.f;
    ImGui::GetStyle().FrameRounding  = 2.f;

    ImGui_ImplWin32_Init(overlay.hwnd());
    ImGui_ImplDX11_Init(overlay.device(), overlay.context());
    io.Fonts->AddFontDefault();

    // ── Subsystems ────────────────────────────────────────────────────────────
    Esp    esp;
    Aimbot aimbot;
    Misc   misc;
    Menu   menu;

    // ── Entity cache thread ───────────────────────────────────────────────────
    std::thread ent_thread(entity_thread_fn, std::ref(cache), std::ref(g_running));

    // ── Periodic revalidation thread (every 5 minutes) ────────────────────────
    std::thread auth_thread([&]() {
        while (g_running.load()) {
            Sleep(300'000); // 5 min
            if (!auth.revalidate(cfg.saved_key, auth_info.hwid))
                g_running.store(false); // revoked key → exit
        }
    });

    // ── Render loop ───────────────────────────────────────────────────────────
    while (g_running.load()) {
        if (!overlay.begin_frame()) break;

        // Toggle menu
        static bool menu_key_last = false;
        bool menu_key_now = (GetAsyncKeyState(cfg.menu_key) & 0x8000) != 0;
        if (menu_key_now && !menu_key_last) cfg.menu_open = !cfg.menu_open;
        menu_key_last = menu_key_now;

        // Check if CS2 still running
        if (!mem.is_valid()) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ESP on background draw list
        int sw = overlay.width(), sh = overlay.height();
        esp.render(cache, cfg, ImGui::GetBackgroundDrawList(), sw, sh);

        // Aimbot
        aimbot.update(cache, cfg, &mem);

        // Misc (no-flash, triggerbot)
        misc.update(cache, cfg, &mem);

        // Menu
        if (cfg.menu_open && !menu.want_capture_mouse()) {
            bool running = g_running.load();
            menu.render(cfg, auth_info, running);
            if (!running) g_running.store(false);
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        overlay.end_frame();
    }

    g_running.store(false);
    ent_thread.join();
    auth_thread.join();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    overlay.destroy();
    mem.detach();
    return 0;
}
