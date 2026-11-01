// language: C++20 / file: login.cpp / runtime: Win32 + GDI / target: Windows x64
#include "login.hpp"
#include <Windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <future>
#include <chrono>
#include <array>
#include <algorithm>
#include <cstring>

namespace {
constexpr COLORREF Background = RGB(12, 15, 20);
constexpr COLORREF Surface = RGB(22, 29, 37);
constexpr COLORREF Border = RGB(40, 53, 64);
constexpr COLORREF Accent = RGB(25, 245, 96);
constexpr COLORREF Text = RGB(235, 242, 248);
constexpr COLORREF Muted = RGB(142, 158, 172);
constexpr COLORREF Error = RGB(255, 137, 137);
constexpr COLORREF Warning = RGB(238, 192, 105);

struct Login {
    Config& cfg;
    Auth& auth;
    AuthInfo& info;
    std::string key;
    std::future<AuthInfo> pending;
    bool authenticated = false;
    bool save_warning = false;
    COLORREF status_color = Muted;
    HBRUSH background = CreateSolidBrush(Background);
    HBRUSH surface = CreateSolidBrush(Surface);
    std::array<HFONT, 4> fonts{};
    ~Login() {
        for (auto font : fonts) if (font) DeleteObject(font);
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
    EnableWindow(GetDlgItem(window, IDOK), !value);
    EnableWindow(GetDlgItem(window, IDCANCEL), !value);
    EnableWindow(GetDlgItem(window, 1007), !value);
    EnableWindow(GetDlgItem(window, 1001), !value);
    SetDlgItemTextW(window, IDOK, value ? L"Validando acesso..." : L"Entrar no Capri");
}

RECT units(HWND window, int left, int top, int right, int bottom) {
    RECT rect{left, top, right, bottom};
    MapDialogRect(window, &rect);
    return rect;
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
    const bool close = item.CtlID == 1007;
    COLORREF fill = close ? Background : primary ? Accent : Surface;
    COLORREF ink = primary ? Background : Text;
    if (pressed) fill = primary ? RGB(47, 185, 154) : Border;
    if (disabled) { fill = Surface; ink = Muted; }
    RECT rect = item.rcItem;
    FillRect(item.hDC, &rect, state.background);
    rounded(item.hDC, rect, fill, primary && !disabled ? fill : close ? Background : Border, 12);
    wchar_t label[128]{};
    GetWindowTextW(item.hwndItem, label, 128);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, ink);
    auto font = SelectObject(item.hDC, state.fonts[1]);
    DrawTextW(item.hDC, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(item.hDC, font);
    if (item.itemState & ODS_FOCUS) {
        InflateRect(&rect, -5, -5);
        DrawFocusRect(item.hDC, &rect);
    }
}

void wait_for_game(HWND window, Login& state) {
    state.authenticated = true;
    EnableWindow(GetDlgItem(window, IDCANCEL), TRUE);
    EnableWindow(GetDlgItem(window, 1007), TRUE);
    EnableWindow(GetDlgItem(window, 1001), FALSE);
    if (state.save_warning) {
        EnableWindow(GetDlgItem(window, IDOK), TRUE);
        SetDlgItemTextW(window, IDOK, L"Continuar sem salvar");
        status(window, state, L"Acesso aprovado, mas a chave nao foi salva. Voce precisara informa-la na proxima abertura.", Warning);
    } else if (FindWindowW(nullptr, L"Counter-Strike 2")) {
        EndDialog(window, IDOK);
    } else {
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
        const UINT dpi = GetDpiForWindow(window);
        const int sizes[]{11, 11, 22, 9};
        for (size_t i = 0; i < state->fonts.size(); ++i)
            state->fonts[i] = CreateFontW(-MulDiv(sizes[i], dpi, 72), 0, 0, 0,
                i == 1 || i == 2 ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH, L"Segoe UI");
        for (int id : {1001, 1002, 1005, IDOK, IDCANCEL, 1007})
            SendDlgItemMessageW(window, id, WM_SETFONT, reinterpret_cast<WPARAM>(state->fonts[0]), TRUE);
        SendDlgItemMessageW(window, 1003, WM_SETFONT, reinterpret_cast<WPARAM>(state->fonts[1]), TRUE);
        SendDlgItemMessageW(window, 1004, WM_SETFONT, reinterpret_cast<WPARAM>(state->fonts[2]), TRUE);
        for (int id : {1006, 1008, 1009})
            SendDlgItemMessageW(window, id, WM_SETFONT, reinterpret_cast<WPARAM>(state->fonts[3]), TRUE);
        SendDlgItemMessageW(window, 1001, EM_SETLIMITTEXT, 255, 0);
        SetDlgItemTextW(window, 1001, wide(state->cfg.saved_key).c_str());
        const int corner_preference = 2;
        DwmSetWindowAttribute(window, 33, &corner_preference, sizeof(corner_preference));
        if (!SetTimer(window, 1, 100, nullptr)) { EndDialog(window, IDCANCEL); return TRUE; }
        SetFocus(GetDlgItem(window, 1001));
        return FALSE;
    }
    if (!state) return FALSE;
    if (message == WM_ERASEBKGND) {
        RECT rect{};
        GetClientRect(window, &rect);
        FillRect(reinterpret_cast<HDC>(wp), &rect, state->background);
        return TRUE;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT all{};
        GetClientRect(window, &all);
        FillRect(dc, &all, state->background);
        rounded(dc, units(window, 22, 145, 398, 179), Surface, Border, 12);
        RECT mark = units(window, 24, 20, 49, 43);
        Gdiplus::GdiplusStartupInput startup; ULONG_PTR token = 0;
        if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) == Gdiplus::Ok) {
            auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(201), MAKEINTRESOURCEW(10));
            IStream* stream = resource ? SHCreateMemStream(static_cast<const BYTE*>(LockResource(LoadResource(nullptr,resource))),SizeofResource(nullptr,resource)) : nullptr;
            if (stream) {
                { Gdiplus::Bitmap bitmap(stream); Gdiplus::Graphics graphics(dc);
                  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                  Gdiplus::GraphicsPath circle; int size = mark.bottom-mark.top;
                  circle.AddEllipse(mark.left,mark.top,size,size); graphics.SetClip(&circle);
                  int side = (std::min)(bitmap.GetWidth(),bitmap.GetHeight());
                  graphics.DrawImage(&bitmap,Gdiplus::Rect(mark.left,mark.top,size,size),(bitmap.GetWidth()-side)/2,(bitmap.GetHeight()-side)/2,side,side,Gdiplus::UnitPixel); }
                stream->Release();
            }
            Gdiplus::GdiplusShutdown(token);
        }
        HPEN line = CreatePen(PS_SOLID, 1, Border);
        auto old = SelectObject(dc, line);
        RECT separator = units(window, 24, 59, 396, 59);
        MoveToEx(dc, separator.left, separator.top, nullptr);
        LineTo(dc, separator.right, separator.bottom);
        SelectObject(dc, old);
        DeleteObject(line);
        EndPaint(window, &paint);
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
    if (message == WM_LBUTTONDOWN) {
        POINT point{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
        const auto header = units(window, 0, 0, 420, 60);
        if (PtInRect(&header, point)) {
            ReleaseCapture();
            SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return TRUE;
        }
    }
    if (message == WM_CLOSE || (message == WM_COMMAND &&
        (LOWORD(wp) == IDCANCEL || LOWORD(wp) == 1007))) {
        if (!state->pending.valid()) EndDialog(window, IDCANCEL);
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(wp) == IDOK) {
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
        if (!state->save_warning && FindWindowW(nullptr, L"Counter-Strike 2")) EndDialog(window, IDOK);
        return TRUE;
    }
    if (message == WM_TIMER && state->pending.valid() &&
        state->pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try { state->info = state->pending.get(); }
        catch (...) { state->info = {}; }
        if (state->info.result == AuthResult::OK) {
            strcpy_s(state->cfg.saved_key, state->key.c_str());
            std::string save_error;
            state->save_warning = !state->cfg.save("configs/default.hbc", &save_error);
            wait_for_game(window, *state);
        } else {
            std::wstring text = L"Chave invalida. Confira e tente novamente.";
            if (state->info.result == AuthResult::EXPIRED) text = L"Esta chave expirou.";
            if (state->info.result == AuthResult::HWID_MISMATCH) text = L"Esta chave esta vinculada a outro computador.";
            if (state->info.result == AuthResult::NET_ERROR) text = L"Nao foi possivel validar o acesso. Confira o servico e sua conexao.";
            if (!state->info.message.empty()) text = wide(state->info.message);
            status(window, *state, text, Error);
            busy(window, false);
            SetFocus(GetDlgItem(window, 1001));
        }
        return TRUE;
    }
    if (message == WM_DESTROY) KillTimer(window, 1);
    return FALSE;
}
}

bool show_login(Config& cfg, Auth& auth, AuthInfo& info) {
    Login state{cfg, auth, info};
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), nullptr,
                           dialog, reinterpret_cast<LPARAM>(&state)) == IDOK;
}
