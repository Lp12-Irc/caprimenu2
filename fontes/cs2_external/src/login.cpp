// language: C++20 / file: login.cpp / runtime: Win32 + GDI / target: Windows x64
#include "login.hpp"
#include "version.hpp"
#include "credentials.hpp"
#include "ui_motion.hpp"
#include <Windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <future>
#include <chrono>
#include <array>
#include <algorithm>
#include <cstring>
#include <commctrl.h>
#include <cmath>
#include <memory>
#include <filesystem>
#pragma comment(lib,"comctl32.lib")

#ifdef CAPRI_LOGIN_TEST
ULONGLONG capri_login_test_clock();
#endif

namespace {
bool client_assets_ready(){
    std::error_code error;
    for(const char* map:{"de_dust2","de_mirage","de_inferno","de_nuke","de_ancient","de_anubis","de_overpass","de_vertigo","de_train","cs_office","cs_italy"})
        if(!std::filesystem::is_regular_file(std::filesystem::path("assets/collision")/(std::string(map)+".bvh"),error))return false;
    return std::filesystem::is_regular_file("assets/radar/de_dust2.json",error);
}
ULONGLONG login_clock_ms() {
#ifdef CAPRI_LOGIN_TEST
    return capri_login_test_clock();
#else
    return GetTickCount64();
#endif
}

constexpr COLORREF Background = RGB(14, 16, 21);
constexpr COLORREF Surface = RGB(24, 28, 36);
constexpr COLORREF Border = RGB(40, 53, 64);
constexpr COLORREF Accent = RGB(248, 123, 157);
constexpr COLORREF Text = RGB(235, 242, 248);
constexpr COLORREF Muted = RGB(142, 158, 172);
constexpr COLORREF Error = RGB(255, 137, 137);
constexpr COLORREF Warning = RGB(238, 192, 105);

struct Login {
    Config& cfg;
    Auth& auth;
    AuthInfo& info;
    Memory& memory;
    ULONGLONG next_attach=0;
    std::string key;
    std::future<AuthInfo> pending;
    bool authenticated = false;
    bool save_warning = false;
    bool reveal=false,animating=false,validating=false;
    ULONGLONG opened=0;
    unsigned progress=0;
    ui_motion::Plexus particles;
    UINT layout_dpi=96;
    bool entrance_done=false;
    std::array<float,4> hover{};
    std::array<bool,4> hot{};
    struct Part {int id,x,y,w,h,delay;};
    static constexpr Part parts[]={
        {1003,68,25,280,19,0},{1008,68,45,280,14,0},{1007,384,20,28,28,0},
        {1004,28,91,384,36,55},{1005,28,132,384,22,80},
        {1006,28,173,384,16,120},{1001,40,205,310,22,120},{1010,376,200,28,30,120},
        {1002,28,251,384,40,155},{IDOK,28,308,280,44,195},{IDCANCEL,320,308,92,44,215},
        {1009,28,375,384,14,240}};
    COLORREF status_color = Muted;
    HBRUSH background = CreateSolidBrush(Background);
    HBRUSH surface = CreateSolidBrush(Surface);
    std::array<HFONT, 4> fonts{};
    std::vector<std::wstring> private_fonts;
    ULONG_PTR graphics_token=0;
    std::unique_ptr<Gdiplus::Bitmap> avatar;
    ~Login() {
        avatar.reset();if(graphics_token)Gdiplus::GdiplusShutdown(graphics_token);
        for (auto font : fonts) if (font) DeleteObject(font);
        for(const auto& path:private_fonts)RemoveFontResourceExW(path.c_str(),FR_PRIVATE,nullptr);
        DeleteObject(background);
        DeleteObject(surface);
    }
};

std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count);
    return result;
}

std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count, nullptr, nullptr);
    return result;
}

void status(HWND window, Login& state, const std::wstring& message, COLORREF color) {
    state.status_color = color;
    SetDlgItemTextW(window, 1002, message.c_str());
    InvalidateRect(GetDlgItem(window, 1002), nullptr, TRUE);
}

void busy(HWND window, bool value) {
    auto* state=reinterpret_cast<Login*>(GetWindowLongPtrW(window,DWLP_USER));
    if(state){state->validating=value;if(value && state->animating)SetTimer(window,2,16,nullptr);}
    EnableWindow(GetDlgItem(window, IDOK), !value);
    EnableWindow(GetDlgItem(window, IDCANCEL), !value);
    EnableWindow(GetDlgItem(window, 1007), !value);
    EnableWindow(GetDlgItem(window, 1001), !value);
    EnableWindow(GetDlgItem(window, 1010), !value);
    SetDlgItemTextW(window, IDOK, value ? L"Validando acesso..." : L"Entrar no Capri");
}

RECT units(HWND window, int left, int top, int right, int bottom) {
    auto* state=reinterpret_cast<Login*>(GetWindowLongPtrW(window,DWLP_USER));
    const UINT dpi=state?state->layout_dpi:GetDpiForWindow(window);
    return {MulDiv(left,dpi,96),MulDiv(top,dpi,96),MulDiv(right,dpi,96),MulDiv(bottom,dpi,96)};
}

float entrance_offset(const Login& state,int delay) {
    if(!state.animating || state.entrance_done)return 0;
    const float t=std::clamp((float(login_clock_ms()-state.opened)-delay)/330.f,0.f,1.f);
    return 12.f*std::pow(1.f-t,3.f);
}
void place_parts(HWND window,Login& state) {
    HDWP positions=BeginDeferWindowPos(static_cast<int>(std::size(Login::parts)));
    for(const auto& p:Login::parts) {
        const int shift=static_cast<int>(std::round(entrance_offset(state,p.delay)));
        auto r=units(window,p.x,p.y+shift,p.x+p.w,p.y+p.h+shift);
        if(positions)positions=DeferWindowPos(positions,GetDlgItem(window,p.id),nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    if(positions)EndDeferWindowPos(positions);
    else for(const auto& p:Login::parts){const int shift=int(std::round(entrance_offset(state,p.delay)));auto r=units(window,p.x,p.y+shift,p.x+p.w,p.y+p.h+shift);SetWindowPos(GetDlgItem(window,p.id),nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);}
}
int button_index(int id){return id==IDOK?0:id==IDCANCEL?1:id==1010?2:id==1007?3:-1;}
LRESULT CALLBACK button_motion(HWND button,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    auto* state=reinterpret_cast<Login*>(data);const int index=button_index(GetDlgCtrlID(button));
    if(index>=0 && (message==WM_MOUSEMOVE || message==WM_MOUSELEAVE)) {
        const bool hot=message==WM_MOUSEMOVE;
        if(state->hot[index]!=hot){state->hot[index]=hot;if(state->animating)SetTimer(GetParent(button),2,16,nullptr);else{state->hover[index]=hot?1.f:0.f;InvalidateRect(button,nullptr,FALSE);}}
        if(hot){TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,button,0};TrackMouseEvent(&track);}
    }
    if(message==WM_NCDESTROY)RemoveWindowSubclass(button,button_motion,id);
    return DefSubclassProc(button,message,wp,lp);
}
COLORREF blend(COLORREF a,COLORREF b,float t) {
    return RGB(int(GetRValue(a)+(GetRValue(b)-GetRValue(a))*t),int(GetGValue(a)+(GetGValue(b)-GetGValue(a))*t),int(GetBValue(a)+(GetBValue(b)-GetBValue(a))*t));
}

void rounded(HDC dc, const RECT& rect, COLORREF fill, COLORREF stroke, int radius) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, stroke);
    auto previous_brush = SelectObject(dc, brush);
    auto previous_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, previous_pen);
    SelectObject(dc, previous_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void draw_button(const DRAWITEMSTRUCT& item, Login& state) {
    const bool primary = item.CtlID == IDOK;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool icon = item.CtlID == 1007 || item.CtlID == 1010;
    const int index=button_index(item.CtlID);const float hover=index>=0?state.hover[index]:0;
    COLORREF fill = blend(icon?(item.CtlID==1010?Surface:Background):primary?RGB(178,42,86):Surface,primary?RGB(199,51,98):RGB(33,39,49),hover);
    COLORREF ink = primary?RGB(255,255,255):Text;
    if (pressed) fill = primary ? RGB(154, 35, 74) : Border;
    if (disabled) { fill = Surface; ink = Muted; }
    RECT rect = item.rcItem;
    FillRect(item.hDC, &rect, item.CtlID==1010?state.surface:state.background);
    rounded(item.hDC, rect, fill, (item.itemState&ODS_FOCUS)?Accent:primary&&!disabled?fill:icon?fill:Border, 10);
    if(item.CtlID==1010) {
        const int cx=(rect.left+rect.right)/2,cy=(rect.top+rect.bottom)/2;
        HPEN pen=CreatePen(PS_SOLID,2,state.reveal?Accent:Muted);auto old=SelectObject(item.hDC,pen);auto brush=SelectObject(item.hDC,GetStockObject(NULL_BRUSH));
        Ellipse(item.hDC,cx-9,cy-5,cx+9,cy+5);Ellipse(item.hDC,cx-2,cy-2,cx+3,cy+3);
        if(state.reveal){MoveToEx(item.hDC,cx-9,cy+7,nullptr);LineTo(item.hDC,cx+9,cy-7);}
        SelectObject(item.hDC,brush);SelectObject(item.hDC,old);DeleteObject(pen);return;
    }
    wchar_t label[128]{};
    GetWindowTextW(item.hwndItem, label, 128);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, ink);
    auto font = SelectObject(item.hDC, state.fonts[1]);
    DrawTextW(item.hDC, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(item.hDC, font);
    if(primary && !disabled && !state.authenticated) {
        const int cx=rect.right-MulDiv(23,state.layout_dpi,96)+int(hover*3),cy=(rect.top+rect.bottom)/2;
        HPEN pen=CreatePen(PS_SOLID,2,Text);auto old=SelectObject(item.hDC,pen);
        MoveToEx(item.hDC,cx-7,cy,nullptr);LineTo(item.hDC,cx+4,cy);MoveToEx(item.hDC,cx,cy-4,nullptr);LineTo(item.hDC,cx+4,cy);LineTo(item.hDC,cx,cy+4);SelectObject(item.hDC,old);DeleteObject(pen);
    }
    if(primary && state.validating) {
        auto bar=rect;bar.left+=12;bar.right-=12;bar.top=bar.bottom-5;bar.bottom-=3;
        const int span=(bar.right-bar.left)/4;bar.left+=(bar.right-bar.left-span)*state.progress/100;bar.right=bar.left+span;rounded(item.hDC,bar,Accent,Accent,2);
    }
    if (item.itemState & ODS_FOCUS) {
        InflateRect(&rect, -5, -5);
        DrawFocusRect(item.hDC, &rect);
    }
}

bool connect_game(Login& state) {
    if(login_clock_ms()<state.next_attach)return false;
    state.next_attach=login_clock_ms()+500;
    return FindWindowW(nullptr,L"Counter-Strike 2") && state.memory.attach("cs2.exe");
}
void wait_for_game(HWND window, Login& state) {
    state.authenticated = true;
    state.reveal=false;SendDlgItemMessageW(window,1001,EM_SETPASSWORDCHAR,0x2022,0);
    SetDlgItemTextW(window,1010,L"Mostrar chave");
    EnableWindow(GetDlgItem(window, IDCANCEL), TRUE);
    EnableWindow(GetDlgItem(window, 1007), TRUE);
    EnableWindow(GetDlgItem(window, 1001), FALSE);
    if (state.save_warning) {
        EnableWindow(GetDlgItem(window, IDOK), TRUE);
        SetDlgItemTextW(window, IDOK, L"Continuar sem salvar");
        status(window, state, L"Acesso aprovado, mas a chave nao foi salva. Voce precisara informa-la na proxima abertura.", Warning);
    } else if (connect_game(state)) {
        EndDialog(window, IDOK);
    } else {
        SetDlgItemTextW(window, IDCANCEL, L"Cancelar");
        EnableWindow(GetDlgItem(window, IDOK), FALSE);
        SetDlgItemTextW(window, IDOK, L"Aguardando Counter-Strike 2");
        status(window, state, L"Acesso aprovado. Abra o CS2 em modo janela ou sem bordas para continuar.", Accent);
    }
}

INT_PTR CALLBACK dialog(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto* state = reinterpret_cast<Login*>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<Login*>(lp);
        SetWindowLongPtrW(window, DWLP_USER, lp);
        MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
        const auto& work=monitor.rcWork;
        state->layout_dpi=(std::max)(72U,(std::min)(GetDpiForWindow(window),(std::min)(UINT((work.right-work.left-32)*96/440),UINT((work.bottom-work.top-32)*96/404))));
        const UINT dpi=state->layout_dpi;
        SetWindowLongPtrW(window,GWL_STYLE,GetWindowLongPtrW(window,GWL_STYLE)&~(WS_CAPTION|WS_THICKFRAME|DS_MODALFRAME));
        const int width=MulDiv(440,dpi,96),height=MulDiv(404,dpi,96);
        SetWindowPos(window,nullptr,work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        const int sizes[]{10,10,19,8};
        for (size_t i = 0; i < state->fonts.size(); ++i)
            state->fonts[i] = CreateFontW(-MulDiv(sizes[i], dpi, 72), 0, 0, 0,
                i == 1 || i == 2 ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH, L"Segoe UI");
        for (int id : {1001, 1002, 1005, IDOK, IDCANCEL, 1007,1010})
            SendDlgItemMessageW(window, id, WM_SETFONT, reinterpret_cast<WPARAM>(state->fonts[0]), TRUE);
        SendDlgItemMessageW(window, 1003, WM_SETFONT, reinterpret_cast<WPARAM>(state->fonts[1]), TRUE);
        SendDlgItemMessageW(window, 1004, WM_SETFONT, reinterpret_cast<WPARAM>(state->fonts[2]), TRUE);
        for (int id : {1006, 1008, 1009})
            SendDlgItemMessageW(window, id, WM_SETFONT, reinterpret_cast<WPARAM>(state->fonts[3]), TRUE);
        SendDlgItemMessageW(window, 1001, EM_SETLIMITTEXT, 255, 0);
        SendDlgItemMessageW(window,1001,EM_SETPASSWORDCHAR,0x2022,0);
        SetDlgItemTextW(window, 1001, wide(state->cfg.saved_key).c_str());
        const int corner_preference = 2;
        DwmSetWindowAttribute(window, 33, &corner_preference, sizeof(corner_preference));
        BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);
        state->animating=animations!=FALSE;state->opened=login_clock_ms();
        state->entrance_done=!state->animating;
        place_parts(window,*state);
        for(int id:{IDOK,IDCANCEL,1010,1007})SetWindowSubclass(GetDlgItem(window,id),button_motion,1,reinterpret_cast<DWORD_PTR>(state));
        SetDlgItemTextW(window,1009,wide(std::string("CAPRI ")+CAPRI_CLIENT_VERSION).c_str());
        Gdiplus::GdiplusStartupInput graphics;
        if(Gdiplus::GdiplusStartup(&state->graphics_token,&graphics,nullptr)==Gdiplus::Ok) {
            auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(201),MAKEINTRESOURCEW(10));
            IStream* stream=resource?SHCreateMemStream(static_cast<const BYTE*>(LockResource(LoadResource(nullptr,resource))),SizeofResource(nullptr,resource)):nullptr;
            if(stream){{Gdiplus::Bitmap source(stream);if(source.GetLastStatus()==Gdiplus::Ok)state->avatar.reset(source.Clone(0,0,source.GetWidth(),source.GetHeight(),PixelFormat32bppARGB));}stream->Release();}
        }
        if(state->animating){SetWindowLongPtrW(window,GWL_EXSTYLE,GetWindowLongPtrW(window,GWL_EXSTYLE)|WS_EX_LAYERED);SetLayeredWindowAttributes(window,0,0,LWA_ALPHA);SetTimer(window,2,16,nullptr);}
        if(state->animating)SetTimer(window,3,40,nullptr);
        if(state->cfg.saved_key[0])SetDlgItemTextW(window,1002,L"Clique em Entrar para validar sua chave.");
        if (!SetTimer(window, 1, 100, nullptr)) { EndDialog(window, IDCANCEL); return TRUE; }
        SetFocus(GetDlgItem(window, 1001));
        return FALSE;
    }
    if (!state) return FALSE;
    if(message==WM_ACTIVATEAPP || message==WM_ACTIVATE) {
        const bool active=message==WM_ACTIVATEAPP?wp!=0:LOWORD(wp)!=WA_INACTIVE;
        state->particles.advance(login_clock_ms(),active && state->animating && IsWindowVisible(window) && !IsIconic(window));
        if(active && state->animating)SetTimer(window,3,40,nullptr);else KillTimer(window,3);
    }
    if(message==WM_SIZE || message==WM_SHOWWINDOW) {
        const bool hidden=(message==WM_SIZE && wp==SIZE_MINIMIZED) || (message==WM_SHOWWINDOW && !wp);
        if(hidden){KillTimer(window,3);state->particles.advance(login_clock_ms(),false);}else if(state->animating && GetForegroundWindow()==window)SetTimer(window,3,40,nullptr);
    }
    if(message==WM_TIMER && wp==3) {
        if(IsIconic(window) || !IsWindowVisible(window) || GetForegroundWindow()!=window){KillTimer(window,3);state->particles.advance(login_clock_ms(),false);return TRUE;}
        state->particles.advance(login_clock_ms(),true);
        InvalidateRect(window,nullptr,FALSE);return TRUE;
    }
    if(message==WM_COMMAND && HIWORD(wp)==BN_CLICKED && button_index(LOWORD(wp))>=0 && !state->entrance_done && state->opened) {
        state->entrance_done=true;SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);place_parts(window,*state);InvalidateRect(window,nullptr,FALSE);
    }
    if(message==WM_LBUTTONDOWN && HIWORD(lp)<MulDiv(72,state->layout_dpi,96)) {ReleaseCapture();SendMessageW(window,WM_NCLBUTTONDOWN,HTCAPTION,lp);return TRUE;}
    if(message==WM_TIMER && wp==2) {
        const auto age=login_clock_ms()-state->opened;
        if(!state->entrance_done) {
            SetLayeredWindowAttributes(window,0,static_cast<BYTE>((std::min)(255ULL,age*255/200)),LWA_ALPHA);
            if(age>=580)state->entrance_done=true;
            place_parts(window,*state);RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
        }
        bool hovering=false;
        const int buttons[]={IDOK,IDCANCEL,1010,1007};
        for(int i=0;i<4;++i){const float target=state->hot[i]?1.f:0.f;if(std::fabs(target-state->hover[i])>.005f){state->hover[i]+=(target-state->hover[i])*.3f;hovering=true;InvalidateRect(GetDlgItem(window,buttons[i]),nullptr,FALSE);}else state->hover[i]=target;}
        if(state->validating){state->progress=(state->progress+2)%100;InvalidateRect(GetDlgItem(window,IDOK),nullptr,FALSE);}
        if(state->entrance_done && !state->validating && !hovering)KillTimer(window,2);
        return TRUE;
    }
    if(message==WM_COMMAND && LOWORD(wp)==1010 && HIWORD(wp)==BN_CLICKED) {
        state->reveal=!state->reveal;SendDlgItemMessageW(window,1001,EM_SETPASSWORDCHAR,state->reveal?0:0x2022,0);
        SetDlgItemTextW(window,1010,state->reveal?L"Ocultar chave":L"Mostrar chave");InvalidateRect(GetDlgItem(window,1001),nullptr,TRUE);return TRUE;
    }
    if(message==WM_COMMAND && LOWORD(wp)==1001 && HIWORD(wp)==EN_CHANGE && state->opened && !state->entrance_done) {state->entrance_done=true;SetLayeredWindowAttributes(window,0,255,LWA_ALPHA);place_parts(window,*state);InvalidateRect(window,nullptr,FALSE);}
    if(message==WM_COMMAND && LOWORD(wp)==1001 && (HIWORD(wp)==EN_SETFOCUS || HIWORD(wp)==EN_KILLFOCUS)) {
        auto field=units(window,28,194,412,238);InvalidateRect(window,&field,FALSE);
    }
    if (message == WM_ERASEBKGND) {
        RECT rect{};
        GetClientRect(window, &rect);
        FillRect(reinterpret_cast<HDC>(wp), &rect, state->background);
        return TRUE;
    }
    if (message == WM_PAINT || message==WM_PRINTCLIENT) {
        PAINTSTRUCT paint{};
        HDC dc = message==WM_PAINT?BeginPaint(window, &paint):reinterpret_cast<HDC>(wp);
        if(message==WM_PRINTCLIENT)GetClientRect(window,&paint.rcPaint);
        RECT all{};
        GetClientRect(window, &all);
        FillRect(dc, &all, state->background);
        RECT edge=all;--edge.right;--edge.bottom;rounded(dc,edge,Background,Border,12);
        if(state->graphics_token) {
            Gdiplus::Graphics ambient(dc);ambient.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            const float w=float(all.right),h=float(all.bottom),pulse=state->particles.pulse();
            Gdiplus::LinearGradientBrush gradient(Gdiplus::Point(0,0),Gdiplus::Point(all.right,all.bottom),Gdiplus::Color(255,16,19,26),Gdiplus::Color(255,12,15,20));
            ambient.FillRectangle(&gradient,1,1,all.right-2,all.bottom-2);
            Gdiplus::Pen link(Gdiplus::Color(BYTE(17+10*pulse),219,39,119),1.f);
            for(const auto& l:state->particles.links) {
                const auto a=state->particles.points[l.a],b=state->particles.points[l.b];const float dx=a.x-b.x,dy=a.y-b.y;
                if(dx*dx+dy*dy<.13f)ambient.DrawLine(&link,a.x*w,a.y*h,b.x*w,b.y*h);
            }
            Gdiplus::SolidBrush dot(Gdiplus::Color(90,236,72,153));
            for(const auto p:state->particles.points)ambient.FillEllipse(&dot,p.x*w-1.5f,p.y*h-1.5f,3.f,3.f);
            Gdiplus::Pen edgeGlow(Gdiplus::Color(BYTE(55+40*pulse),236,72,153),1.f);
            ambient.DrawRectangle(&edgeGlow,1,1,all.right-3,all.bottom-3);
        }
        const int shift=int(std::round(entrance_offset(*state,120)));
        rounded(dc, units(window,28,194+shift,412,238+shift), Surface, GetFocus()==GetDlgItem(window,1001)?Accent:Border, 10);
        const int brand_shift=int(std::round(entrance_offset(*state,0)));
        RECT mark=units(window,28,24+brand_shift,56,52+brand_shift);
        RECT intersection{};
        if(IntersectRect(&intersection,&paint.rcPaint,&mark)) {
        if(state->avatar) {
                { auto& bitmap=*state->avatar;Gdiplus::Graphics graphics(dc);
                  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                  Gdiplus::GraphicsPath circle; int size = mark.bottom-mark.top;
                  circle.AddEllipse(mark.left,mark.top,size,size); graphics.SetClip(&circle);
                  int side = (std::min)(bitmap.GetWidth(),bitmap.GetHeight());
                  graphics.DrawImage(&bitmap,Gdiplus::Rect(mark.left,mark.top,size,size),(bitmap.GetWidth()-side)/2,(bitmap.GetHeight()-side)/2,side,side,Gdiplus::UnitPixel); }
        }
        }
        HPEN line = CreatePen(PS_SOLID, 1, Border);
        auto old = SelectObject(dc, line);
        RECT separator=units(window,28,365,412,365);
        MoveToEx(dc, separator.left, separator.top, nullptr);
        LineTo(dc, separator.right, separator.bottom);
        SelectObject(dc, old);
        DeleteObject(line);
        if(message==WM_PAINT)EndPaint(window, &paint);
        return TRUE;
    }
    if (message == WM_CTLCOLORDLG) return reinterpret_cast<INT_PTR>(state->background);
    if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT) {
        HDC dc = reinterpret_cast<HDC>(wp);
        const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lp));
        const bool field = id == 1001;
        SetBkColor(dc, field ? Surface : Background);
        SetTextColor(dc, field || id == 1003 || id == 1004 ? Text : id == 1002 ? state->status_color : Muted);
        return reinterpret_cast<INT_PTR>(field ? state->surface : state->background);
    }
    if (message == WM_DRAWITEM) {
        draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lp), *state);
        return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND &&
        (LOWORD(wp) == IDCANCEL || LOWORD(wp) == 1007))) {
        if (!state->pending.valid()) EndDialog(window, IDCANCEL);
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(wp) == IDOK) {
        if(state->info.result==AuthResult::UPDATE_REQUIRED){
            if(state->info.package_sha256.empty()){open_release_url(state->info.download_url);return TRUE;}
            strcpy_s(state->cfg.saved_key,state->key.c_str());std::string error;
            if(!credentials::save("credentials.dat",state->key,error)){status(window,*state,L"Nao foi possivel proteger sua chave antes de atualizar.",Error);return TRUE;}
            if(launch_client_update(state->info))EndDialog(window,IDCANCEL);
            else status(window,*state,L"Nao foi possivel iniciar o atualizador. Extraia o pacote completo do Capri.",Error);
            return TRUE;
        }
        if (state->authenticated) {
            state->save_warning = false;
            wait_for_game(window, *state);
            return TRUE;
        }
        if (state->pending.valid()) return TRUE;
        wchar_t key[256]{};
        GetDlgItemTextW(window, 1001, key, 256);
        state->key = utf8(key);
        const auto first = state->key.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            status(window, *state, L"Informe sua chave de acesso para continuar.", Error);
            SetFocus(GetDlgItem(window, 1001));
            return TRUE;
        }
        state->key = state->key.substr(first, state->key.find_last_not_of(" \t\r\n") - first + 1);
        if (state->key.size() >= sizeof(state->cfg.saved_key)) {
            status(window, *state, L"A chave excede o tamanho permitido.", Error);
            return TRUE;
        }
        busy(window, true);
        status(window, *state, L"Conferindo sua chave de acesso. Aguarde um instante.", Accent);
        try {
            state->pending = std::async(std::launch::async, [state] {
                return state->auth.validate_online(state->key);
            });
        } catch (...) {
            busy(window, false);
            status(window, *state, L"Nao foi possivel iniciar a validacao. Tente novamente.", Error);
        }
        return TRUE;
    }
    if (message == WM_TIMER && state->authenticated) {
        if (!state->save_warning && connect_game(*state)) EndDialog(window, IDOK);
        return TRUE;
    }
    if (message == WM_TIMER && state->pending.valid() &&
        state->pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try { state->info = state->pending.get(); }
        catch (...) { state->info = {}; }
        state->validating=false;
        if(state->info.result==AuthResult::OK && !client_assets_ready()){
            std::string error;credentials::save("credentials.dat",state->key,error);
            state->info.result=AuthResult::UPDATE_REQUIRED;
            state->info.message="Conclua a instalacao dos arquivos do cliente neste PC. Clique em Atualizar agora.";
        }
        if (state->info.result == AuthResult::OK) {
            strcpy_s(state->cfg.saved_key, state->key.c_str());
            std::string save_error;
            state->save_warning = !credentials::save("credentials.dat",state->key,save_error);
            wait_for_game(window, *state);
        } else {
            std::wstring text = L"Chave invalida. Confira e tente novamente.";
            if (state->info.result == AuthResult::EXPIRED) text = L"Esta chave expirou.";
            if (state->info.result == AuthResult::HWID_MISMATCH) text = L"Esta chave esta vinculada a outro computador.";
            if (state->info.result == AuthResult::NET_ERROR) text = L"Nao foi possivel validar o acesso. Confira o servico e sua conexao.";
            if (!state->info.message.empty()) text = wide(state->info.message);
            status(window, *state, text, Error);
            busy(window, false);
            if(state->info.result==AuthResult::UPDATE_REQUIRED) {
                SetDlgItemTextW(window,IDOK,state->info.package_sha256.empty()?L"Baixar atualizacao obrigatoria":L"Atualizar agora");
                EnableWindow(GetDlgItem(window,1001),FALSE);
            }
            SetFocus(GetDlgItem(window, 1001));
        }
        return TRUE;
    }
    if (message == WM_DESTROY) {KillTimer(window, 1);KillTimer(window,2);KillTimer(window,3);}
    return FALSE;
}
}

bool show_login(Config& cfg, Auth& auth, AuthInfo& info, Memory& memory) {
    Login state{cfg, auth, info, memory};
    wchar_t windows[MAX_PATH]{};GetWindowsDirectoryW(windows,MAX_PATH);
    for(const auto* face:{L"segoeui.ttf",L"segoeuib.ttf",L"seguisb.ttf"}) {
        auto path=std::wstring(windows)+L"\\Fonts\\"+face;
        if(AddFontResourceExW(path.c_str(),FR_PRIVATE,nullptr))state.private_fonts.push_back(std::move(path));
    }
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), nullptr,
                           dialog, reinterpret_cast<LPARAM>(&state)) == IDOK;
}
