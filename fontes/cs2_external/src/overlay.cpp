// language: C++20 / file: overlay.cpp / runtime: MSVC + DX11 / target: Windows x64
#include "overlay.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include <dwmapi.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>
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
    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT |
        WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"Capri",
        WS_POPUP, pt.x, pt.y, w_, h_, nullptr, nullptr, wc.hInstance, this);
    if (!hwnd_) return false;
    SetLayeredWindowAttributes(hwnd_,0,255,LWA_ALPHA);
    if (!init_dx11()) { destroy(); return false; }
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

bool Overlay::init_dx11() {
    using Microsoft::WRL::ComPtr;
    D3D_FEATURE_LEVEL fl=D3D_FEATURE_LEVEL_11_0;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
       &fl,1,D3D11_SDK_VERSION,&dev_,nullptr,&ctx_)))return false;
    ComPtr<IDXGIDevice1> device;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory2> factory;
    if(FAILED(dev_->QueryInterface(IID_PPV_ARGS(&device))) || FAILED(device->GetAdapter(&adapter)))return false;
    device->SetMaximumFrameLatency(1);
    if(SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory)))) {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width=UINT(w_);desc.Height=UINT(h_);desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;
        desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.Scaling=DXGI_SCALING_STRETCH;
        desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
        ComPtr<IDXGISwapChain1> swap;ComPtr<IDCompositionDevice> compositor;
        ComPtr<IDCompositionTarget> target;ComPtr<IDCompositionVisual> visual;
        if(SUCCEEDED(factory->CreateSwapChainForComposition(dev_,&desc,nullptr,&swap)) &&
           SUCCEEDED(DCompositionCreateDevice(device.Get(),IID_PPV_ARGS(&compositor))) &&
           SUCCEEDED(compositor->CreateTargetForHwnd(hwnd_,TRUE,&target)) &&
           SUCCEEDED(compositor->CreateVisual(&visual)) && SUCCEEDED(visual->SetContent(swap.Get())) &&
           SUCCEEDED(target->SetRoot(visual.Get())) && SUCCEEDED(compositor->Commit())) {
            sc_=swap.Detach();composition_device_=compositor.Detach();composition_target_=target.Detach();
            composition_visual_=visual.Detach();composition_=true;
            return rebuild_rtv();
        }
    }
    // Compatibility fallback for machines without a usable composition device.
    ComPtr<IDXGIFactory> legacy;
    if(FAILED(adapter->GetParent(IID_PPV_ARGS(&legacy))))return false;
    auto style=GetWindowLongPtrW(hwnd_,GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd_,GWL_EXSTYLE,(style & ~WS_EX_NOREDIRECTIONBITMAP)|WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd_,RGB(0,0,0),0,LWA_COLORKEY);
    MARGINS margins{-1,-1,-1,-1};DwmExtendFrameIntoClientArea(hwnd_,&margins);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width=UINT(w_);desc.BufferDesc.Height=UINT(h_);desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;
    desc.OutputWindow=hwnd_;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    return SUCCEEDED(legacy->CreateSwapChain(dev_,&desc,&sc_)) && rebuild_rtv();
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
        focus_attempts_=0;next_focus_=0;focus_menu();
    } else if (!enabled) {
        if (GetCapture() == hwnd_) ReleaseCapture();
        if(ImGui::GetCurrentContext()) {
            auto& io=ImGui::GetIO();io.ClearEventsQueue();io.ClearInputKeys();io.ClearInputMouse();io.AddFocusEvent(false);
        }
        restore_game_focus();
    }
}

void Overlay::restore_game_focus() {
    if(interactive_ || !hwnd_ || !IsWindow(target_hwnd_) || GetForegroundWindow()!=hwnd_)return;
    if(GetCapture()==hwnd_)ReleaseCapture();
    SetForegroundWindow(target_hwnd_);
    if(GetForegroundWindow()==hwnd_ && !IsHungAppWindow(target_hwnd_)) {
        const DWORD other=GetWindowThreadProcessId(target_hwnd_,nullptr),own=GetCurrentThreadId();
        if(other && other!=own && AttachThreadInput(own,other,TRUE)) {
            SetForegroundWindow(target_hwnd_);
            if(GetForegroundWindow()==target_hwnd_)SetFocus(target_hwnd_);
            AttachThreadInput(own,other,FALSE);
        }
    }
}

void Overlay::focus_menu() {
    if(!interactive_ || !visible_ || !content_visible_)return;
    HWND foreground=GetForegroundWindow();
    if(foreground==hwnd_) {ClipCursor(nullptr);return;}
    // Never take focus from another app after Alt+Tab.
    if(foreground!=target_hwnd_ || focus_attempts_>=2 || GetTickCount64()<next_focus_)return;
    ++focus_attempts_;next_focus_=GetTickCount64()+100;
    SetForegroundWindow(hwnd_);
    if(GetForegroundWindow()!=hwnd_ && !IsHungAppWindow(target_hwnd_)) {
        const DWORD other=GetWindowThreadProcessId(target_hwnd_,nullptr),own=GetCurrentThreadId();
        if(other && other!=own && AttachThreadInput(own,other,TRUE)) {
            SetForegroundWindow(hwnd_);
            if(GetForegroundWindow()==hwnd_) {
                if(GetCapture()==target_hwnd_)ReleaseCapture();
                SetFocus(hwnd_);
            }
            AttachThreadInput(own,other,FALSE);
        }
    }
    if(GetForegroundWindow()==hwnd_) {
        SetFocus(hwnd_);ClipCursor(nullptr);SetCursor(LoadCursorW(nullptr,MAKEINTRESOURCEW(32512)));
    }
}

void Overlay::set_content_visible(bool enabled) {
    if(content_visible_==enabled)return;
    content_visible_=enabled;
    if(hwnd_)ShowWindow(hwnd_,visible_ && enabled?SW_SHOWNOACTIVATE:SW_HIDE);
}

bool Overlay::begin_frame(bool render_hidden,bool prepare_render) {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    if (!IsWindow(target_hwnd_) || !hwnd_) return false;
    sync_to_game();
    if(interactive_)focus_menu();else restore_game_focus();
    if (render_hidden && !visible_) sync_geometry();
    if (failed_ || !rtv_) return false;
    if (!visible_ && !render_hidden) return true;
    if(prepare_render)prepare_frame();
    return true;
}
void Overlay::prepare_frame() {
    const float clear[4]{0,0,0,0};
    ctx_->ClearRenderTargetView(rtv_, clear);
    ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
    D3D11_VIEWPORT vp{};
    vp.Width = float(w_); vp.Height = float(h_); vp.MaxDepth = 1.f;
    ctx_->RSSetViewports(1, &vp);
}

void Overlay::end_frame() {
    if (visible_ && content_visible_) {++presents_;if(FAILED(sc_->Present(0, 0)))failed_=true;}
}

void Overlay::sync_to_game() {
    HWND foreground = GetForegroundWindow();
    bool show = !IsIconic(target_hwnd_) && IsWindowVisible(target_hwnd_) &&
        (foreground == target_hwnd_ || foreground == hwnd_);
    if (show != visible_) {
        visible_ = show;
        ++revision_;
        positioned_=false;
        ShowWindow(hwnd_, show && content_visible_ ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
    if (!show) return;
    sync_geometry();
}
void Overlay::sync_geometry() {
    RECT rc{}; POINT pt{};
    if (!GetClientRect(target_hwnd_, &rc) || !ClientToScreen(target_hwnd_, &pt)) return;
    int nw = rc.right, nh = rc.bottom;
    if (nw <= 0 || nh <= 0) return;
    if(!positioned_ || pt.x!=x_ || pt.y!=y_ || nw!=w_ || nh!=h_) {
        SetWindowPos(hwnd_, HWND_TOPMOST, pt.x, pt.y, nw, nh, SWP_NOACTIVATE);
        ++placements_;
        ++revision_;
        x_=pt.x;y_=pt.y;positioned_=true;
    }
    if (nw != w_ || nh != h_) {
        ctx_->OMSetRenderTargets(0, nullptr, nullptr);
        if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
        HRESULT hr = sc_->ResizeBuffers(0, nw, nh, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr) || !rebuild_rtv()) { failed_ = true; return; }
        w_ = nw; h_ = nh;
    }
}

void Overlay::destroy() {
    if(hwnd_)set_interactive(false);
    if(composition_target_)composition_target_->SetRoot(nullptr);
    if(composition_device_)composition_device_->Commit();
    if(composition_visual_)composition_visual_->Release();
    if(composition_target_)composition_target_->Release();
    if(composition_device_)composition_device_->Release();
    composition_visual_=nullptr;composition_target_=nullptr;composition_device_=nullptr;composition_=false;
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
    if(self) switch(m) {
        case WM_MOUSEMOVE:case WM_MOUSELEAVE:case WM_MOUSEWHEEL:case WM_MOUSEHWHEEL:
        case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_RBUTTONDOWN:case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:case WM_MBUTTONUP:case WM_XBUTTONDOWN:case WM_XBUTTONUP:
        case WM_KEYDOWN:case WM_KEYUP:case WM_SYSKEYDOWN:case WM_SYSKEYUP:case WM_CHAR:
        case WM_SETFOCUS:case WM_KILLFOCUS:case WM_SIZE:case WM_PAINT:case WM_INPUTLANGCHANGE:++self->revision_;break;
    }
    if(self && self->interactive_ && m==WM_SETFOCUS && GetForegroundWindow()==h)ClipCursor(nullptr);
    if(self && self->interactive_ && m==WM_MOUSEACTIVATE)return MA_ACTIVATE;
    const bool cleanup=m==WM_KILLFOCUS || m==WM_MOUSELEAVE || m==WM_DESTROY || m==WM_INPUTLANGCHANGE;
    if (ImGui::GetCurrentContext() && self && (self->interactive_ || cleanup) && ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return 1;
    if (m == WM_NCHITTEST && self && !self->interactive_) return HTTRANSPARENT;
    if (m == WM_MOUSEACTIVATE && self && !self->interactive_) return MA_NOACTIVATE;
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}
