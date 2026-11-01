// language: C++20 / file: overlay.cpp / runtime: MSVC + DX11 / target: Windows x64
#include "overlay.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include <dwmapi.h>
#include <dxgi1_2.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

bool Overlay::create(const std::string& target_title) {
    std::wstring wt(target_title.begin(), target_title.end());
    target_hwnd_ = FindWindowW(nullptr, wt.c_str());
    if (!target_hwnd_) return false;
    RECT rc{};
    if (!GetClientRect(target_hwnd_, &rc)) return false;
    w_ = (rc.right > 0) ? rc.right : 1;
    h_ = (rc.bottom > 0) ? rc.bottom : 1;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"_HBOvr";
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    POINT pt{};
    ClientToScreen(target_hwnd_, &pt);
    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT |
        WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"Capri",
        WS_POPUP, pt.x, pt.y, w_, h_, nullptr, nullptr, wc.hInstance, this);
    if (!hwnd_) return false;
    SetLayeredWindowAttributes(hwnd_, RGB(0,0,0), 0, LWA_COLORKEY);
    MARGINS mg{-1,-1,-1,-1};
    DwmExtendFrameIntoClientArea(hwnd_, &mg);
    if (!init_dx11()) { destroy(); return false; }
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

bool Overlay::init_dx11() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferDesc.Width = UINT(w_); sd.BufferDesc.Height = UINT(h_);
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2; sd.OutputWindow = hwnd_; sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &sd, &sc_, &dev_, nullptr, &ctx_))) return false;
    IDXGIDevice1* dxgi_device=nullptr;
    if(SUCCEEDED(dev_->QueryInterface(__uuidof(IDXGIDevice1),reinterpret_cast<void**>(&dxgi_device)))) {
        dxgi_device->SetMaximumFrameLatency(1);
        dxgi_device->Release();
    }
    return rebuild_rtv();
}

bool Overlay::rebuild_rtv() {
    ID3D11Texture2D* bb = nullptr;
    if (FAILED(sc_->GetBuffer(0, IID_PPV_ARGS(&bb)))) return false;
    HRESULT hr = dev_->CreateRenderTargetView(bb, nullptr, &rtv_);
    bb->Release();
    return SUCCEEDED(hr);
}

void Overlay::set_interactive(bool enabled) {
    if (!hwnd_ || interactive_ == enabled) return;
    interactive_ = enabled;
    auto style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (enabled) style &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    else style |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, style);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (enabled && visible_) {
        SetForegroundWindow(hwnd_);
        SetFocus(hwnd_);
    } else if (!enabled) {
        if (GetCapture() == hwnd_) ReleaseCapture();
        if (GetForegroundWindow() == hwnd_) SetForegroundWindow(target_hwnd_);
    }
}

bool Overlay::begin_frame(bool render_hidden) {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    if (!IsWindow(target_hwnd_) || !hwnd_) return false;
    sync_to_game();
    if (render_hidden && !visible_) sync_geometry();
    if (failed_ || !rtv_) return false;
    if (!visible_ && !render_hidden) return true;
    const float clear[4]{0,0,0,0};
    ctx_->ClearRenderTargetView(rtv_, clear);
    ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
    D3D11_VIEWPORT vp{};
    vp.Width = float(w_); vp.Height = float(h_); vp.MaxDepth = 1.f;
    ctx_->RSSetViewports(1, &vp);
    return true;
}

void Overlay::end_frame() {
    if (visible_ && FAILED(sc_->Present(0, 0))) failed_ = true;
}

void Overlay::sync_to_game() {
    HWND foreground = GetForegroundWindow();
    bool show = !IsIconic(target_hwnd_) && IsWindowVisible(target_hwnd_) &&
        (foreground == target_hwnd_ || foreground == hwnd_);
    if (show != visible_) {
        visible_ = show;
        ShowWindow(hwnd_, show ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
    if (!show) return;
    sync_geometry();
}
void Overlay::sync_geometry() {
    RECT rc{}; POINT pt{};
    if (!GetClientRect(target_hwnd_, &rc) || !ClientToScreen(target_hwnd_, &pt)) return;
    int nw = rc.right, nh = rc.bottom;
    if (nw <= 0 || nh <= 0) return;
    SetWindowPos(hwnd_, HWND_TOPMOST, pt.x, pt.y, nw, nh, SWP_NOACTIVATE);
    if (nw != w_ || nh != h_) {
        ctx_->OMSetRenderTargets(0, nullptr, nullptr);
        if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
        HRESULT hr = sc_->ResizeBuffers(0, nw, nh, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr) || !rebuild_rtv()) { failed_ = true; return; }
        w_ = nw; h_ = nh;
    }
}

void Overlay::destroy() {
    if (ctx_) ctx_->ClearState();
    if (rtv_) rtv_->Release();
    if (sc_) sc_->Release();
    if (ctx_) ctx_->Release();
    if (dev_) dev_->Release();
    rtv_ = nullptr; sc_ = nullptr; ctx_ = nullptr; dev_ = nullptr;
    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    UnregisterClassW(L"_HBOvr", GetModuleHandleW(nullptr));
}

LRESULT CALLBACK Overlay::wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* self = reinterpret_cast<Overlay*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        self = static_cast<Overlay*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    if (m == WM_NCHITTEST && self && !self->interactive_) return HTTRANSPARENT;
    if (m == WM_MOUSEACTIVATE && self && !self->interactive_) return MA_NOACTIVATE;
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}
