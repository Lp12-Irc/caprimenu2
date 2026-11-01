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
#include "brand.hpp"
#include "config.hpp"
#include "auth.hpp"
#include "login.hpp"
#include <filesystem>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <Windows.h>

static std::atomic<bool> g_running{true};
static std::mutex g_stop_mutex;
static std::condition_variable g_stop_cv;
static std::atomic<std::shared_ptr<const EntityCache>> g_snapshot;
struct InputFrame { Config cfg; int width=0, height=0; bool enabled=false; };
static std::atomic<std::shared_ptr<const InputFrame>> g_input_frame;
static std::atomic<bool> g_write_available{false};
static std::atomic<Misc::TriggerState> g_trigger_state{Misc::TriggerState::Paused};

static void entity_thread_fn(EntityCache& cache,
                              const std::atomic<bool>& running) {
    PollTimer timer;
    while (running.load()) {
        CacheOptions options;
        const auto frame=g_input_frame.load();
        if (frame) {
            options.bones=frame->cfg.esp || frame->cfg.aim;
            options.inventory=frame->cfg.esp && frame->cfg.esp_utilities;
            options.items=frame->cfg.item_esp;
        }
        cache.update(options);
        g_snapshot.store(std::make_shared<const EntityCache>(cache));
        if (frame && (!frame->width || !frame->height)) Sleep(30);
        else timer.wait();
        anti_debug::watchdog_tick(); // silent exit if debugger attaches mid-session
    }
}

static void input_thread_fn(Memory& memory) {
    Aimbot aim; Misc misc;
    PollTimer timer;
    ULONGLONG next_write_attempt=0;
    while (g_running.load()) {
        const auto frame=g_input_frame.load();
        const auto snapshot=g_snapshot.load();
        DWORD foreground_pid=0;
        GetWindowThreadProcessId(GetForegroundWindow(),&foreground_pid);
        const bool active=frame && frame->enabled && !frame->cfg.menu_open && snapshot && snapshot->ready() &&
            foreground_pid==memory.pid() && GetTickCount64()-snapshot->sampled_at()<=30;
        if (active) {
            if (frame->cfg.no_flash && !memory.has_write() && GetTickCount64()>=next_write_attempt) {
                memory.attach_write("cs2.exe"); next_write_attempt=GetTickCount64()+5000;
            }
            aim.update(*snapshot,frame->cfg,&memory);
            misc.update(*snapshot,frame->cfg,&memory,frame->width,frame->height);
        } else { aim.reset(); misc.reset(); }
        g_write_available.store(memory.has_write());
        g_trigger_state.store(misc.trigger_state());
        if (active) timer.wait();
        else Sleep(8);
    }
    aim.reset(); misc.reset();
}

int WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // ── Anti-debug ──────────────────────────────────────────────────────────
    anti_debug::run();

    ImGui_ImplWin32_EnableDpiAwareness();

    // ── Config ──────────────────────────────────────────────────────────────
    wchar_t exe_path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH))
        std::filesystem::current_path(std::filesystem::path(exe_path).parent_path());
    Config cfg;
    cfg.load("configs/default.hbc");

    // ── Auth ─────────────────────────────────────────────────────────────────
    Auth    auth;
    AuthInfo auth_info;
    if (!show_login(cfg, auth, auth_info)) return 0;
    cfg.menu_open = true;

    // ── Wait for CS2 ─────────────────────────────────────────────────────────
    Memory mem;
    while (!mem.attach(ENC("cs2.exe").decrypt())) Sleep(1000);

    // ── Resolve offsets ───────────────────────────────────────────────────────
    EntityCache cache;
    cache.init(&mem);
    cache.resolve();

    // Optional write handle for No Flash.
    if (cfg.no_flash)
        mem.attach_write(ENC("cs2.exe").decrypt());

    // ── Overlay ───────────────────────────────────────────────────────────────
    Overlay overlay;
    if (!overlay.create(ENC("Counter-Strike 2").decrypt())) {
        MessageBoxW(nullptr, L"Nao foi possivel iniciar a janela Direct3D. Abra o CS2 em janela ou sem bordas.", L"Capri", MB_OK | MB_ICONERROR);
        return 1;
    }

    // ── ImGui ─────────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't write imgui.ini

    Brand::init(overlay.device());
    Menu::apply_theme();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplWin32_Init(overlay.hwnd());
    ImGui_ImplDX11_Init(overlay.device(), overlay.context());
    if (!io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 18.f))
        io.Fonts->AddFontDefault();
    overlay.set_interactive(cfg.menu_open);

    // ── Subsystems ────────────────────────────────────────────────────────────
    Esp    esp;
    esp.init(overlay.device());
    Menu   menu;

    // ── Entity cache thread ───────────────────────────────────────────────────
    std::thread ent_thread(entity_thread_fn, std::ref(cache), std::ref(g_running));
    std::thread input_thread(input_thread_fn,std::ref(mem));

    // ── Periodic revalidation thread (every 5 minutes) ────────────────────────
    const std::string session_key = cfg.saved_key;
    std::thread auth_thread([&]() {
        std::unique_lock<std::mutex> lock(g_stop_mutex);
        while (!g_stop_cv.wait_for(lock, std::chrono::minutes(5), [] { return !g_running.load(); })) {
            lock.unlock();
            bool valid = auth.revalidate(session_key, auth_info.hwid);
            lock.lock();
            if (!valid) { g_running.store(false); break; }
        }
    });

    // ── Render loop ───────────────────────────────────────────────────────────
    PollTimer render_timer;
    while (g_running.load()) {
        if (!overlay.begin_frame()) break;

        // Toggle menu
        static bool menu_key_last = false;
        bool menu_key_now = (GetAsyncKeyState(cfg.menu_key) & 0x8000) != 0;
        if (menu_key_now && !menu_key_last && overlay.visible()) cfg.menu_open = !cfg.menu_open;
        overlay.set_interactive(cfg.menu_open);
        menu_key_last = menu_key_now;

        // Check if CS2 still running
        if (!mem.is_valid()) break;
        if (!overlay.visible()) {
            auto frame=std::make_shared<InputFrame>(); frame->cfg=cfg;
            g_input_frame.store(frame); Sleep(30); continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ESP on background draw list
        int sw = overlay.width(), sh = overlay.height();
        const auto snapshot=g_snapshot.load();
        cfg.write_available=g_write_available.load();
        cfg.trigger_status=Misc::trigger_text(g_trigger_state.load());
        if (snapshot) {
            cfg.game_status=snapshot->status(); cfg.game_ready=snapshot->ready();
            esp.render(*snapshot,cfg,ImGui::GetBackgroundDrawList(),sw,sh);
        }

        // Menu
        if (cfg.menu_open) {
            bool running = g_running.load();
            menu.render(cfg, auth_info, running);
            menu.radar_controls(cfg,sw,sh);
            if (!running) g_running.store(false);
        }

        overlay.set_interactive(cfg.menu_open);
        auto frame=std::make_shared<InputFrame>();
        frame->cfg=cfg; frame->width=sw; frame->height=sh; frame->enabled=!cfg.menu_open;
        g_input_frame.store(frame);
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        overlay.end_frame();
        render_timer.wait(4); // Bound CPU use without waiting for a VSync interval.
    }

    { std::lock_guard<std::mutex> lock(g_stop_mutex); g_running.store(false); }
    g_stop_cv.notify_all();
    input_thread.join();
    ent_thread.join();
    auth_thread.join();

    Brand::shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    overlay.destroy();
    mem.detach();
    return 0;
}
