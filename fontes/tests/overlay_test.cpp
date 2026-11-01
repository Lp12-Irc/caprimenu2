// language: C++20 / file: overlay_test.cpp / runtime: MSVC + DX11 / target: Windows x64
#include "overlay.hpp"
#include "render_capture.hpp"
#include "menu.hpp"
#include "brand.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <cstdio>
#include <stdexcept>
#include <cstring>

static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    if(argc>2 && (std::strcmp(argv[1],"--capture")==0 || std::strcmp(argv[1],"--test-fov")==0 || std::strcmp(argv[1],"--test-radar")==0)) {
        ImGui::CreateContext(); ImGui::GetIO().IniFilename=nullptr;
        ImGui::GetIO().DisplaySize={1360,800}; ImGui::GetIO().DeltaTime=1.f/60;
        ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",18);
        Menu::apply_theme(); RenderCapture capture;
        check(capture.init(1360,800),"capture init"); check(Brand::init(capture.device()),"avatar");
        Menu menu; Config cfg; AuthInfo info; bool running=true;
        const bool stability=std::strcmp(argv[1],"--test-fov")==0;
        cfg.aim_fov=7.f;
        const bool radar_test=std::strcmp(argv[1],"--test-radar")==0;
        if(radar_test){cfg.menu_open=true;cfg.radar_x=0;cfg.radar_y=0;cfg.radar_sz=160;}
        for(int i=0;i<(stability?600:radar_test?20:3);i++){
            if(stability) { ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow,(i%30)<15); ImGui::GetIO().AddKeyEvent(ImGuiKey_UpArrow,(i%40)<20); }
            if(radar_test) {
                ImGui::GetIO().AddMousePosEvent(i<5?50.f:90.f,i<5?16.f:56.f);
                if(i==3) ImGui::GetIO().AddMouseButtonEvent(0,true);
                if(i==8) ImGui::GetIO().AddMouseButtonEvent(0,false);
            }
            capture.begin();ImGui::NewFrame();menu.render(cfg,info,running);
            if(radar_test)menu.radar_controls(cfg,1360,800);
            ImGui::Render();
            check(cfg.aim_fov==7.f,"FOV changes without explicit confirmation");
        }
        if(stability) std::puts("PASS: FOV remains 7.0 across 600 frames with arrow-key input, without confirmation.");
        if(radar_test){check(cfg.radar_x>1.f && cfg.radar_y>4.f,"radar drag updates position");std::puts("PASS: radar title drag updates position.");}
        bool saved=capture.save(argv[2]); Brand::shutdown();capture.shutdown();ImGui::DestroyContext();
        return saved?0:1;
    }
    bool interactive = argc > 1 && std::strcmp(argv[1], "--interactive") == 0;
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"HillBlackTestHost";
    wc.hbrBackground = CreateSolidBrush(RGB(28,33,41));
    RegisterClassW(&wc);
    HWND host = CreateWindowW(wc.lpszClassName, L"HillBlack UI test host", WS_OVERLAPPEDWINDOW,
        120, 80, 1160, 800, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(host, SW_SHOW); SetForegroundWindow(host);
    try {
        Overlay overlay;
        check(overlay.create("HillBlack UI test host"), "create overlay");
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        check(Brand::init(overlay.device()), "embedded Capri avatar decodes");
    Menu::apply_theme();
        ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 18);
        check(ImGui_ImplWin32_Init(overlay.hwnd()), "win32 init");
        check(ImGui_ImplDX11_Init(overlay.device(), overlay.context()), "dx11 init");
        auto styles = [&] { return GetWindowLongPtrW(overlay.hwnd(), GWL_EXSTYLE); };
        check((styles() & WS_EX_TRANSPARENT) != 0, "closed menu passes input");
        overlay.set_interactive(true);
        check((styles() & (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE)) == 0, "open menu captures input");
        POINT hit{20,20}; ClientToScreen(overlay.hwnd(), &hit);
        const auto hit_position = MAKELPARAM(hit.x, hit.y);
        check(SendMessageW(overlay.hwnd(), WM_NCHITTEST, 0, hit_position) == HTCLIENT, "interactive hit-test");
        SendMessageW(overlay.hwnd(), WM_CHAR, 'a', 0);
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        check(ImGui::GetIO().InputQueueCharacters.Size == 1, "WndProc forwards text to ImGui");
        ImGui::EndFrame();
        check(overlay.begin_frame(true), "initial frame");
        RECT client{}; GetClientRect(host, &client);
        check(overlay.width() == client.right && overlay.height() == client.bottom, "client size");
        SetWindowPos(host, nullptr, 150, 90, 1100, 760, SWP_NOZORDER | SWP_NOACTIVATE);
        check(overlay.begin_frame(true), "resize frame");
        GetClientRect(host, &client);
        check(overlay.width() == client.right && overlay.height() == client.bottom, "resized client size");
        ID3D11RenderTargetView* rtv = nullptr;
        overlay.context()->OMGetRenderTargets(1, &rtv, nullptr);
        check(rtv != nullptr, "render target survives resize"); rtv->Release();
        overlay.set_interactive(false);
        check((styles() & WS_EX_TRANSPARENT) != 0, "close restores passthrough");
        check(SendMessageW(overlay.hwnd(), WM_NCHITTEST, 0, hit_position) == HTTRANSPARENT, "closed hit-test");
        overlay.set_interactive(true);
        Config cfg; cfg.menu_open = true;
        AuthInfo info; info.result = AuthResult::OK; info.hwid = "TESTE LOCAL DE INTERFACE";
        info.expires_at = "Ambiente isolado";
        Menu menu; bool running = true, last = false;
        int frames = 0;
        while (running && overlay.begin_frame(!interactive)) {
            bool key = (GetAsyncKeyState(cfg.menu_key) & 0x8000) != 0;
            if (key && !last) cfg.menu_open = !cfg.menu_open;
            last = key;
            overlay.set_interactive(cfg.menu_open);
            if (interactive && !overlay.visible()) { Sleep(20); continue; }
            ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
            if (cfg.menu_open) menu.render(cfg, info, running);
            overlay.set_interactive(cfg.menu_open);
            ImGui::Render(); ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); overlay.end_frame();
            if (!interactive && ++frames >= 4) break;
        }
        Brand::shutdown();
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
        overlay.destroy();
        std::puts("PASS: input styles, hit tests, ImGui text events, client alignment, DX11 resize, render.");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what()); DestroyWindow(host); return 1;
    }
    DestroyWindow(host);
    DeleteObject(wc.hbrBackground);
    return 0;
}
