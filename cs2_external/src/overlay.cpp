// language: C++20, file: overlay.cpp, target: Windows 11 x64, DX11+DWMAPI
#include "overlay.hpp"
#include <dwmapi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

bool Overlay::create(const std::string& target_title) {
    std::wstring wt(target_title.begin(), target_title.end());
    target_hwnd_ = FindWindowW(nullptr, wt.c_str());
    if (!target_hwnd_) return false;

    RECT rc{};
    GetClientRect(target_hwnd_, &rc);
    w_ = rc.right; h_ = rc.bottom;

    WNDCLASSEXW wc{
        .cbSize        = sizeof(wc),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = wnd_proc,
        .hInstance     = GetModuleHandleW(nullptr),
        .hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
        .lpszClassName = L"_HBOvr"
    };
    RegisterClassExW(&wc);

    POINT pt{};
    ClientToScreen(target_hwnd_, &pt);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"_HBOvr", L" ",
        WS_POPUP, pt.x, pt.y, w_, h_,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd_) return false;

    SetLayeredWindowAttributes(hwnd_, RGB(0,0,0), 0, LWA_COLORKEY);
    SetWindowDisplayAffinity(hwnd_, WDA_EXCLUDEFROMCAPTURE); // hide from OBS/Discord
    MARGINS mg{-1,-1,-1,-1};
    DwmExtendFrameIntoClientArea(hwnd_, &mg);

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return init_dx11();
}

bool Overlay::init_dx11() {
    DXGI_SWAP_CHAIN_DESC sd{
        .BufferDesc   = { .Width  = UINT(w_), .Height = UINT(h_),
                          .Format = DXGI_FORMAT_R8G8B8A8_UNORM },
        .SampleDesc   = { .Count = 1 },
        .BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount  = 2,
        .OutputWindow = hwnd_,
        .Windowed     = TRUE,
        .SwapEffect   = DXGI_SWAP_EFFECT_DISCARD
    };
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
               nullptr, 0, &fl, 1, D3D11_SDK_VERSION,
               &sd, &sc_, &dev_, nullptr, &ctx_))) return false;
    rebuild_rtv();
    return true;
}

void Overlay::rebuild_rtv() {
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
    ID3D11Texture2D* bb = nullptr;
    sc_->GetBuffer(0, IID_PPV_ARGS(&bb));
    dev_->CreateRenderTargetView(bb, nullptr, &rtv_);
    bb->Release();
}

bool Overlay::begin_frame() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) return false;
    }
    sync_to_game();
    float clear[4]{0,0,0,0};
    ctx_->ClearRenderTargetView(rtv_, clear);
    ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
    D3D11_VIEWPORT vp{ .Width=float(w_), .Height=float(h_), .MaxDepth=1.f };
    ctx_->RSSetViewports(1, &vp);
    return true;
}

void Overlay::end_frame() { sc_->Present(0, 0); }

void Overlay::sync_to_game() {
    if (!target_hwnd_) return;
    RECT rc{};
    GetWindowRect(target_hwnd_, &rc);
    int nw = rc.right - rc.left, nh = rc.bottom - rc.top;
    SetWindowPos(hwnd_, HWND_TOPMOST, rc.left, rc.top, nw, nh, SWP_NOACTIVATE);
    if (nw != w_ || nh != h_) {
        w_ = nw; h_ = nh;
        sc_->ResizeBuffers(0, nw, nh, DXGI_FORMAT_UNKNOWN, 0);
        rebuild_rtv();
    }
}

void Overlay::destroy() {
    if (rtv_) rtv_->Release();
    if (sc_)  sc_->Release();
    if (ctx_) ctx_->Release();
    if (dev_) dev_->Release();
    if (hwnd_) DestroyWindow(hwnd_);
    rtv_ = nullptr; sc_ = nullptr; ctx_ = nullptr; dev_ = nullptr; hwnd_ = nullptr;
}

LRESULT CALLBACK Overlay::wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}
