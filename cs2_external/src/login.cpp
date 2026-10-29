#include "login.hpp"
#include <Windows.h>
#include <future>
#include <chrono>
#include <filesystem>

namespace {
struct Login {
    Config& cfg;
    Auth& auth;
    AuthInfo& info;
    std::string key;
    std::future<AuthInfo> pending;
    bool authenticated = false;
};
void busy(HWND w, bool value) {
    EnableWindow(GetDlgItem(w, IDOK), !value);
    EnableWindow(GetDlgItem(w, IDCANCEL), !value);
    EnableWindow(GetDlgItem(w, 1001), !value);
}
INT_PTR CALLBACK dialog(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    auto* s = reinterpret_cast<Login*>(GetWindowLongPtrW(w, DWLP_USER));
    if (msg == WM_INITDIALOG) {
        s = reinterpret_cast<Login*>(lp);
        SetWindowLongPtrW(w, DWLP_USER, lp);
        SendDlgItemMessageW(w, 1001, EM_SETLIMITTEXT, 255, 0);
        SetDlgItemTextA(w, 1001, s->cfg.saved_key);
        SetFocus(GetDlgItem(w, 1001));
        return FALSE;
    }
    if (!s) return FALSE;
    if (msg == WM_CLOSE || (msg == WM_COMMAND && LOWORD(wp) == IDCANCEL)) {
        if (!s->pending.valid()) EndDialog(w, IDCANCEL);
        return TRUE;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == IDOK) {
        if (s->authenticated) return TRUE;
        if (s->pending.valid()) return TRUE;
        char key[256]{};
        GetDlgItemTextA(w, 1001, key, sizeof(key));
        s->key = key;
        auto first = s->key.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            SetDlgItemTextW(w, 1002, L"Digite uma key antes de continuar.");
            return TRUE;
        }
        s->key = s->key.substr(first, s->key.find_last_not_of(" \t\r\n") - first + 1);
        busy(w, true);
        SetDlgItemTextW(w, 1002, L"Validando key...");
        s->pending = std::async(std::launch::async, [s] { return s->auth.validate_online(s->key); });
        SetTimer(w, 1, 100, nullptr);
        return TRUE;
    }
    if (msg == WM_TIMER && s->authenticated) {
        if (FindWindowW(nullptr, L"Counter-Strike 2")) EndDialog(w, IDOK);
        return TRUE;
    }
    if (msg == WM_TIMER && s->pending.valid() &&
        s->pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        KillTimer(w, 1);
        try { s->info = s->pending.get(); }
        catch (...) { s->info.result = AuthResult::NET_ERROR; }
        if (s->info.result == AuthResult::OK) {
            strcpy_s(s->cfg.saved_key, s->key.c_str());
            std::error_code ec;
            std::filesystem::create_directories("configs", ec);
            if (!ec) s->cfg.save("configs/default.hbc");
            if (FindWindowW(nullptr, L"Counter-Strike 2")) EndDialog(w, IDOK);
            else {
                s->authenticated = true;
                EnableWindow(GetDlgItem(w, IDCANCEL), TRUE);
                SetDlgItemTextW(w, 1002, L"Key aprovada! Abra o CS2 em modo janela ou sem bordas para continuar.");
                SetTimer(w, 1, 500, nullptr);
            }
        } else {
            const wchar_t* message = L"Key invalida. Confira e tente novamente.";
            if (s->info.result == AuthResult::EXPIRED) message = L"Esta key expirou.";
            if (s->info.result == AuthResult::HWID_MISMATCH) message = L"Esta key esta vinculada a outro computador.";
            if (s->info.result == AuthResult::NET_ERROR) message = L"Nao foi possivel validar no servidor. Confira a API e sua conexao.";
            SetDlgItemTextW(w, 1002, message);
            if (!s->info.message.empty()) SetDlgItemTextA(w, 1002, s->info.message.c_str());
            busy(w, false);
        }
        return TRUE;
    }
    return FALSE;
}
}
bool show_login(Config& cfg, Auth& auth, AuthInfo& info) {
    Login state{cfg, auth, info};
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), nullptr,
                           dialog, reinterpret_cast<LPARAM>(&state)) == IDOK;
}
