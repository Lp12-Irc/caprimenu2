// language: C++20, file: overlay.hpp, target: Windows 11 x64, DX11
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <string>
#include <cstdint>

struct IDCompositionDevice;
struct IDCompositionTarget;
struct IDCompositionVisual;

class Overlay {
public:
    bool create(const std::string& target_title);
    ~Overlay() { destroy(); }
    void destroy();
    bool begin_frame(bool render_hidden=false,bool prepare_render=true);
    void prepare_frame();
    void end_frame();
    void sync_to_game();
    void set_interactive(bool enabled);
    void set_content_visible(bool enabled);
    bool visible() const { return visible_; }

    bool is_valid()     const { return hwnd_ != nullptr; }
    int  width()        const { return w_; }
    int  height()       const { return h_; }
    HWND hwnd()         const { return hwnd_; }
    uint64_t presents()   const { return presents_; }
    uint64_t placements() const { return placements_; }
    uint64_t revision()   const { return revision_; }
    bool uses_composition() const { return composition_; }

    ID3D11Device*        device()  const { return dev_; }
    ID3D11DeviceContext* context() const { return ctx_; }

private:
    static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);
    bool init_dx11();
    bool rebuild_rtv();
    void sync_geometry();
    void focus_menu();
    void restore_game_focus();

    HWND hwnd_        = nullptr;
    HWND target_hwnd_ = nullptr;
    IDXGISwapChain*         sc_  = nullptr;
    IDCompositionDevice* composition_device_ = nullptr;
    IDCompositionTarget* composition_target_ = nullptr;
    IDCompositionVisual* composition_visual_ = nullptr;
    bool composition_ = false;
    ID3D11Device*           dev_ = nullptr;
    ID3D11DeviceContext*    ctx_ = nullptr;
    ID3D11RenderTargetView* rtv_ = nullptr;
    int w_ = 1920, h_ = 1080;
    bool interactive_     = false;
    unsigned focus_attempts_ = 0;
    ULONGLONG next_focus_    = 0;
    bool visible_         = true;
    bool content_visible_ = true;
    bool failed_          = false;
    int x_ = 0, y_ = 0;
    bool positioned_ = false;
    uint64_t presents_ = 0, placements_ = 0;
    uint64_t revision_ = 0;

    // Nome de classe gerado em runtime — sem string fixa enumeravel pelo AC.
    std::wstring class_name_;
};
