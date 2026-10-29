// language: C++20, file: overlay.hpp, target: Windows 11 x64, DX11
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <string>

class Overlay {
public:
    bool create(const std::string& target_title);
    void destroy();
    bool begin_frame(); // returns false on WM_QUIT
    void end_frame();
    void sync_to_game(); // reposition/resize to match game window

    bool is_valid()     const { return hwnd_ != nullptr; }
    int  width()        const { return w_; }
    int  height()       const { return h_; }
    HWND hwnd()         const { return hwnd_; }

    ID3D11Device*        device()  const { return dev_; }
    ID3D11DeviceContext* context() const { return ctx_; }

private:
    static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);
    bool init_dx11();
    void rebuild_rtv();

    HWND hwnd_        = nullptr;
    HWND target_hwnd_ = nullptr;
    IDXGISwapChain*         sc_  = nullptr;
    ID3D11Device*           dev_ = nullptr;
    ID3D11DeviceContext*    ctx_ = nullptr;
    ID3D11RenderTargetView* rtv_ = nullptr;
    int w_ = 1920, h_ = 1080;
};
