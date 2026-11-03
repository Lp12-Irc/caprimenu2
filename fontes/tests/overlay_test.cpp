// language: C++20 / file: overlay_test.cpp / runtime: MSVC + DX11 / target: Windows x64
#include "overlay.hpp"
#include "render_capture.hpp"
#include "menu.hpp"
#include "profile_store.hpp"
#include "render_policy.hpp"
#include "input.hpp"
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
static void check_alpha(Overlay& overlay) {
    ID3D11RenderTargetView* view=nullptr;ID3D11Resource* resource=nullptr;ID3D11Texture2D* source=nullptr;
    overlay.context()->OMGetRenderTargets(1,&view,nullptr);check(view!=nullptr,"alpha readback target");
    view->GetResource(&resource);view->Release();
    HRESULT hr=resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&source));resource->Release();check(SUCCEEDED(hr),"alpha readback texture");
    D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);const auto format=desc.Format;
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    ID3D11Texture2D* staging=nullptr;hr=overlay.device()->CreateTexture2D(&desc,nullptr,&staging);
    if(FAILED(hr)){source->Release();check(false,"alpha staging texture");}
    overlay.context()->CopyResource(staging,source);source->Release();
    D3D11_MAPPED_SUBRESOURCE map{};hr=overlay.context()->Map(staging,0,D3D11_MAP_READ,0,&map);
    if(FAILED(hr)){staging->Release();check(false,"alpha staging map");}
    const auto* clear=static_cast<const unsigned char*>(map.pData);
    const auto* tint=clear+4*map.RowPitch+4*4;
    const int red=format==DXGI_FORMAT_B8G8R8A8_UNORM?tint[2]:tint[0];
    const bool valid=clear[3]==0 && tint[3]>=127 && tint[3]<=129 && red>=63 && red<=65 && tint[1]>=31 && tint[1]<=33;
    overlay.context()->Unmap(staging,0);staging->Release();check(valid,"GPU output must preserve transparent clear and premultiplied color");
    std::puts("PASS: actual swap-chain GPU readback has alpha 0 outside content and premultiplied alpha/color for translucent content.");
}
int main(int argc, char** argv) {
    if(argc>2 && (std::strcmp(argv[1],"--capture-trigger")==0 || std::strcmp(argv[1],"--capture-account")==0 || std::strcmp(argv[1],"--test-profile-save")==0 || std::strcmp(argv[1],"--capture")==0 || std::strcmp(argv[1],"--capture-small")==0 || std::strcmp(argv[1],"--capture-settings")==0 || std::strcmp(argv[1],"--capture-theme")==0 || std::strcmp(argv[1],"--capture-revoked")==0 || std::strcmp(argv[1],"--test-fov")==0 || std::strcmp(argv[1],"--test-radar")==0 || std::strcmp(argv[1],"--capture-radar")==0 || std::strcmp(argv[1],"--capture-update")==0 || std::strcmp(argv[1],"--capture-display")==0 || std::strcmp(argv[1],"--capture-display-small")==0)) {
        const bool profile_test=std::strcmp(argv[1],"--test-profile-save")==0;
        const auto previous_directory=std::filesystem::current_path();
        if(profile_test){const auto isolated=previous_directory/"verificacao"/"testes-ui-profile"/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));std::filesystem::create_directories(isolated);std::filesystem::current_path(isolated);}
        ImGui::CreateContext(); ImGui::GetIO().IniFilename=nullptr;
        const bool small_display=std::strcmp(argv[1],"--capture-display-small")==0;
        const bool display_page=small_display || std::strcmp(argv[1],"--capture-display")==0;
        const bool narrow_capture=small_display || std::strcmp(argv[1],"--capture-small")==0 || (argc>3 && std::strcmp(argv[3],"--small")==0);
        const bool settings=std::strcmp(argv[1],"--capture-settings")==0;
        const bool alternate=std::strcmp(argv[1],"--capture-theme")==0;
        const bool revoked=std::strcmp(argv[1],"--capture-revoked")==0;
        const int capture_width=narrow_capture?800:1360,capture_height=narrow_capture?600:800;
        ImGui::GetIO().DisplaySize={float(capture_width),float(capture_height)}; ImGui::GetIO().DeltaTime=1.f/60;
        ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",16);
        Menu::apply_theme(); RenderCapture capture;
        check(capture.init(capture_width,capture_height),"capture init"); check(Brand::init(capture.device()),"avatar");
        Menu menu("default",settings?5:display_page?2:std::strcmp(argv[1],"--capture-trigger")==0?1:std::strcmp(argv[1],"--capture-account")==0?6:0); Config cfg; AuthInfo info; bool running=true;
        cfg.aim=true;if(display_page)cfg.esp=true;
        if(std::strcmp(argv[1],"--capture-trigger")==0){cfg.triggerbot=true;cfg.trigger_status="Aguardando inimigo sob a mira";}
        if(std::strcmp(argv[1],"--capture-account")==0){info.result=AuthResult::OK;info.expires_at="never";}
        if(profile_test){cfg.esp=true;cfg.radar=true;cfg.esp_player_style=1;}
        if(alternate){cfg.menu_accent=IM_COL32(30,190,165,255);cfg.menu_background=IM_COL32(8,18,20,255);cfg.menu_surface=IM_COL32(13,26,29,255);}
        if(revoked){info.result=AuthResult::BAD_KEY;info.message="Chave desativada pelo administrador.";}
        const bool update_page=std::strcmp(argv[1],"--capture-update")==0;
        if(update_page){info.result=AuthResult::UPDATE_REQUIRED;info.version="1.4.1";info.latest_build=info.min_build=3;info.package_sha256=std::string(64,'a');info.package_size=4096;info.download_url="https://example.com/client.zip";info.message="Atualizacao obrigatoria para continuar.";info.release_notes="Melhorias de desempenho e correcoes do painel.";}
        const bool radar_page=std::strcmp(argv[1],"--capture-radar")==0;
        const bool stability=std::strcmp(argv[1],"--test-fov")==0;
        cfg.aim_fov=7.f;
        if(display_page){cfg.display_aspect=3;cfg.display_fit=1;}
        const bool radar_test=std::strcmp(argv[1],"--test-radar")==0;
        if(radar_test){cfg.radar=true;cfg.menu_open=true;cfg.radar_x=0;cfg.radar_y=0;cfg.radar_sz=160;}
        for(int i=0;i<(profile_test?8:stability?600:radar_test?20:radar_page?25:display_page||settings?8:3);i++){
            if(radar_page){
                ImGui::GetIO().AddMousePosEvent(i<7?100.f:i<12?670.f:i<16?470.f:590.f,i<7?385.f:i<12?370.f:560.f);
                if(i==3 || i==8 || i==13 || i==18)ImGui::GetIO().AddMouseButtonEvent(0,true);
                if(i==4 || i==9 || i==14 || i==19)ImGui::GetIO().AddMouseButtonEvent(0,false);
            }
            if(profile_test){ImGui::GetIO().AddMousePosEvent(1060,660);if(i==2)ImGui::GetIO().AddMouseButtonEvent(0,true);if(i==3)ImGui::GetIO().AddMouseButtonEvent(0,false);}
            if(stability) { ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow,(i%30)<15); ImGui::GetIO().AddKeyEvent(ImGuiKey_UpArrow,(i%40)<20); }
            if(radar_test) {
                ImGui::GetIO().AddMousePosEvent(i<5?50.f:90.f,i<5?16.f:56.f);
                if(i==3) ImGui::GetIO().AddMouseButtonEvent(0,true);
                if(i==8) ImGui::GetIO().AddMouseButtonEvent(0,false);
            }
            capture.begin();ImGui::NewFrame();if(update_page || revoked)menu.render_access_block(info,capture_width,capture_height,running);else menu.render(cfg,info,running);
            if(radar_test)menu.radar_controls(cfg,1360,800);
            ImGui::Render();
            check(cfg.aim_fov==7.f,"FOV changes without explicit confirmation");
        }
        if(stability) std::puts("PASS: FOV remains 7.0 across 600 frames with arrow-key input, without confirmation.");
        if(radar_test){check(cfg.radar_x>1.f && cfg.radar_y>4.f,"radar drag updates position");std::puts("PASS: radar title drag updates position.");}
        if(radar_page){check(cfg.radar_sz==120.f,"menu small preset updates radar size");check(cfg.radar_x==17.f && cfg.radar_y==73.f,"menu direction buttons update radar position");std::puts("PASS: radar menu preset and direction buttons update size and position.");}
        if(profile_test){Config restored;std::string name,error;check(profile_store::startup(restored,name,&error),error.c_str());check(restored.esp && restored.aim && restored.radar && restored.esp_player_style==1 && restored.aim_fov==7.f,"Save button failed to persist active options and selected wall style");std::filesystem::current_path(previous_directory);std::puts("PASS: footer Save persists options, style and values through profile startup in isolated folder.");}
        bool saved=capture.save(argv[2]); Brand::shutdown();capture.shutdown();ImGui::DestroyContext();
        return saved?0:1;
    }
    const bool visual_qa=argc>1 && std::strcmp(argv[1],"--visual-qa")==0;
    bool interactive = visual_qa || (argc > 1 && std::strcmp(argv[1], "--interactive") == 0);
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"HillBlackTestHost";
    wc.hbrBackground = CreateSolidBrush(RGB(28,33,41));
    RegisterClassW(&wc);
    HWND host = CreateWindowW(wc.lpszClassName, L"HillBlack UI test host", WS_OVERLAPPEDWINDOW,
        120, 80, 1160, 800, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(host, SW_SHOW); ShowWindow(host, SW_SHOW); SetForegroundWindow(host);
    try {
        Overlay overlay;
        check(overlay.create("HillBlack UI test host"), "create overlay");
        std::printf("Presentation: %s\n",overlay.uses_composition()?"DirectComposition flip / premultiplied alpha":"legacy fallback");
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
        const auto presentations=overlay.presents(),placements=overlay.placements();
        for(int i=0;i<120;++i)check(overlay.begin_frame(true,false),"idle pump");
        check(overlay.presents()==presentations,"idle polling submitted GPU presentations");
        check(overlay.placements()==placements,"idle polling repeatedly repositioned an unchanged window");
        std::puts("PASS: 120 idle polls submit no presentations and do not reposition the window.");
        overlay.set_interactive(false);
        const bool game_was_visible=overlay.visible();
        overlay.set_content_visible(false);
        check(!IsWindowVisible(overlay.hwnd()) && overlay.visible()==game_was_visible,"Empty overlay hides its surface while keeping PgUp eligibility");
        overlay.set_content_visible(true);
        check(bool(IsWindowVisible(overlay.hwnd()))==overlay.visible(),"Overlay surface resumes only when its game is foreground");
        check((styles() & WS_EX_TRANSPARENT) != 0, "close restores passthrough");
        check(SendMessageW(overlay.hwnd(), WM_NCHITTEST, 0, hit_position) == HTTRANSPARENT, "closed hit-test");
        if(GetForegroundWindow()==host) {
            POINT cursor{};GetCursorPos(&cursor);RECT pin{cursor.x,cursor.y,cursor.x+1,cursor.y+1};
            ClipCursor(&pin);overlay.set_interactive(true);
            RECT released{};GetClipCursor(&released);ClipCursor(nullptr);
            check(released.right-released.left>1 && released.bottom-released.top>1,"opening the menu releases the game's cursor confinement without right click");
            std::puts("PASS: opening menu releases cursor confinement without a mouse click.");
        } else {overlay.set_interactive(true);std::puts("SKIP: live cursor confinement case; test host is not foreground.");}
        check(SendMessageW(overlay.hwnd(),WM_MOUSEACTIVATE,0,0)==MA_ACTIVATE,"menu activates on a normal mouse click");
        Config cfg; cfg.menu_open = true;
        AuthInfo info; info.result = AuthResult::OK; info.hwid = "TESTE LOCAL DE INTERFACE";
        info.expires_at = "Ambiente isolado";
        Menu menu; bool running = true, last = false;
        int frames=0,polls=0,event_wakes=0;std::uint64_t previous_revision=0;MenuFrameGate gate;PollTimer visual_timer;
        const auto qa_started=input_clock_ms();
        while (running && overlay.begin_frame(!interactive,false)) {
            if(visual_qa && input_clock_ms()-qa_started>30000)break;
            bool key = interactive && (GetAsyncKeyState(cfg.menu_key) & 0x8000) != 0;
            if (key && !last) cfg.menu_open = !cfg.menu_open;
            last = key;
            overlay.set_interactive(cfg.menu_open);
            if (interactive && !overlay.visible()) { Sleep(20); continue; }
            if(!interactive && polls++>=500)break;
            if(!interactive && !gate.due(polls*16,overlay.revision(),false,false))continue;
            if(overlay.revision()!=previous_revision)++event_wakes;
            previous_revision=overlay.revision();
            overlay.prepare_frame();
            ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
            if(frames==0)ImGui::GetBackgroundDrawList()->AddRectFilled({2,2},{8,8},IM_COL32(128,64,32,128));
            if (cfg.menu_open) menu.render(cfg, info, running);
            overlay.set_interactive(cfg.menu_open);
            ImGui::Render(); ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if(frames==0)check_alpha(overlay);
            overlay.end_frame();
            ++frames;gate.commit(polls*16,overlay.revision());
            if(interactive)visual_timer.wait(16);
        }
        if(!interactive){std::printf("Menu schedule: frames=%d event_wakes=%d polls=500\n",frames,event_wakes);check(frames>=1 && frames<=32+event_wakes,"static menu rebuilt without an event or idle deadline");std::puts("PASS: menu redraws are bounded by idle deadlines plus real window events.");}
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
