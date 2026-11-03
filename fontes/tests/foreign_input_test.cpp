// Input routing integration test. The recipient is a dedicated child fixture,
// never CS2. Run only when no game window or keyboard modifiers are active.
#include "overlay.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include <dwmapi.h>
#include <cstdio>
#include <string>
#include <stdexcept>

static unsigned clicks=0,keys=0;
static LRESULT CALLBACK host_proc(HWND h,UINT m,WPARAM w,LPARAM l) {
    if(m==WM_LBUTTONDOWN){++clicks;return 0;}
    if(m==WM_KEYDOWN && w=='K'){++keys;return 0;}
    if(m==WM_APP+1)return clicks;
    if(m==WM_APP+2)return keys;
    if(m==WM_CLOSE){DestroyWindow(h);return 0;}
    if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,m,w,l);
}
static void check(bool result,const char* message){if(!result)throw std::runtime_error(message);}
static bool modifiers() {for(int key:{VK_MENU,VK_CONTROL,VK_SHIFT,VK_LWIN,VK_RWIN})if(GetAsyncKeyState(key)&0x8000)return true;return false;}
static void pump(Overlay& overlay,DWORD milliseconds) {
    const auto end=GetTickCount64()+milliseconds;
    do {check(overlay.begin_frame(true,false),"fixture overlay pump");Sleep(5);}while(GetTickCount64()<end);
}
static unsigned counter(HWND host,UINT message) {
    DWORD_PTR value=0;check(SendMessageTimeoutW(host,message,0,0,SMTO_ABORTIFHUNG,1000,&value)!=0,"foreign fixture query");return unsigned(value);
}
static COLORREF pixel(POINT point) {
    HDC screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,1,1);auto old=SelectObject(memory,bitmap);
    BitBlt(memory,0,0,1,1,screen,point.x,point.y,SRCCOPY|CAPTUREBLT);
    const auto color=GetPixel(memory,0,0);
    SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(nullptr,screen);return color;
}
static void click_fixture(HWND host,HWND overlay,POINT point,bool menu_open) {
    const HWND expected=menu_open?overlay:host;
    check(GetForegroundWindow()==expected && !modifiers(),"fixture lost foreground before input; no input sent");
    HWND at=GetAncestor(WindowFromPoint(point),GA_ROOT);
    check(at==host || at==overlay,"unrelated app covers the fixture; no input sent");
    SetCursorPos(point.x,point.y);
    INPUT input[2]{};for(auto& item:input)item.type=INPUT_MOUSE;
    input[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;input[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    check(SendInput(2,input,sizeof(INPUT))==2,"fixture mouse injection");
}
int main(int argc,char** argv) {
    ImGui_ImplWin32_EnableDpiAwareness();
    if(argc>1 && std::string(argv[1])=="--host") {
        WNDCLASSW wc{};wc.lpfnWndProc=host_proc;wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpszClassName=L"CapriForeignFixture";wc.hbrBackground=CreateSolidBrush(RGB(24,32,40));RegisterClassW(&wc);
        const auto title=L"Capri input fixture "+std::to_wstring(GetCurrentProcessId());
        HWND h=CreateWindowW(wc.lpszClassName,title.c_str(),WS_OVERLAPPEDWINDOW,100,80,1000,700,nullptr,nullptr,wc.hInstance,nullptr);
        ShowWindow(h,SW_SHOW);ShowWindow(h,SW_SHOW);SetForegroundWindow(h);
        const auto end=GetTickCount64()+20000;MSG msg{};
        while(IsWindow(h) && GetTickCount64()<end){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)break;TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(5);}
        if(IsWindow(h))DestroyWindow(h);DeleteObject(wc.hbrBackground);return 0;
    }
    if(FindWindowW(nullptr,L"Counter-Strike 2") || modifiers()){std::puts("SKIP: game or keyboard modifiers active; no input sent.");return 77;}
    wchar_t self[32768]{};GetModuleFileNameW(nullptr,self,32768);
    std::wstring command=L"\""+std::wstring(self)+L"\" --host";
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
    check(CreateProcessW(self,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=0,"launch foreign fixture");
    HWND host=nullptr;Overlay overlay;bool imgui=false;int result=1;POINT original{};GetCursorPos(&original);
    try {
        const auto title=L"Capri input fixture "+std::to_wstring(process.dwProcessId);
        for(int i=0;i<200 && !host;++i){host=FindWindowW(nullptr,title.c_str());if(host && !IsWindowVisible(host))host=nullptr;Sleep(5);}
        check(host!=nullptr,"foreign fixture window");
        SetForegroundWindow(host);
        if(GetForegroundWindow()!=host){std::puts("SKIP: fixture could not obtain foreground; no input sent.");result=77;}
        else {
            RECT client{};GetClientRect(host,&client);POINT center{client.right/2,client.bottom/2};ClientToScreen(host,&center);
            UpdateWindow(host);Sleep(400);DwmFlush();const auto background=pixel(center);
            std::printf("Host desktop pixel: %u,%u,%u\n",GetRValue(background),GetGValue(background),GetBValue(background));
            const std::string title8="Capri input fixture "+std::to_string(process.dwProcessId);
            check(overlay.create(title8),"create cross-process overlay");
            ImGui::CreateContext();imgui=true;check(ImGui_ImplWin32_Init(overlay.hwnd()),"fixture backend");
            unsigned char* pixels;int width,height;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
            ImGui::GetIO().DisplaySize={float(overlay.width()),float(overlay.height())};ImGui::GetIO().DeltaTime=1.f/60;
            overlay.prepare_frame();overlay.end_frame();pump(overlay,350);DwmFlush();
            auto clear=pixel(center);
            std::printf("Transparent desktop pixel: %u,%u,%u\n",GetRValue(clear),GetGValue(clear),GetBValue(clear));
            check(std::abs(int(GetRValue(clear))-int(GetRValue(background)))<5 && std::abs(int(GetGValue(clear))-int(GetGValue(background)))<5 && std::abs(int(GetBValue(clear))-int(GetBValue(background)))<5,"transparent composition exposes the host background");
            overlay.prepare_frame();
            ID3D11RenderTargetView* view=nullptr;overlay.context()->OMGetRenderTargets(1,&view,nullptr);
            const float color[4]{.8f,.1f,.2f,1};overlay.context()->ClearRenderTargetView(view,color);view->Release();
            overlay.end_frame();pump(overlay,350);DwmFlush();
            auto painted=pixel(center);std::printf("Opaque desktop pixel: %u,%u,%u\n",GetRValue(painted),GetGValue(painted),GetBValue(painted));
            check(GetRValue(painted)>150,"composition remains visible with layered input routing");
            for(int cycle=0;cycle<3;++cycle) {
                overlay.set_interactive(true);pump(overlay,150);
                check(GetForegroundWindow()==overlay.hwnd(),"menu receives foreground");
                const auto before=counter(host,WM_APP+1);
                click_fixture(host,overlay.hwnd(),center,true);pump(overlay,40);
                check(counter(host,WM_APP+1)==before,"open menu retains mouse clicks");
                SetCapture(overlay.hwnd());ImGui::GetIO().AddKeyEvent(ImGuiKey_W,true);ImGui::GetIO().AddMouseButtonEvent(0,true);
                overlay.set_interactive(false);pump(overlay,100);
                check(GetCapture()!=overlay.hwnd() && GetForegroundWindow()==host,"closing releases capture and returns foreground");
                GUITHREADINFO info{sizeof(info)};GetGUIThreadInfo(GetWindowThreadProcessId(host,nullptr),&info);
                check(info.hwndFocus==host,"foreign host owns keyboard focus");
                click_fixture(host,overlay.hwnd(),center,false);pump(overlay,60);
                check(counter(host,WM_APP+1)==before+1,"closed overlay passes real mouse clicks to a different process");
                const auto key_before=counter(host,WM_APP+2);
                check(GetForegroundWindow()==host && !modifiers(),"fixture retains foreground before keyboard input");
                INPUT keyboard[2]{};for(auto& item:keyboard){item.type=INPUT_KEYBOARD;item.ki.wVk='K';}keyboard[1].ki.dwFlags=KEYEVENTF_KEYUP;
                check(SendInput(2,keyboard,sizeof(INPUT))==2,"fixture keyboard injection");pump(overlay,60);
                check(counter(host,WM_APP+2)==key_before+1,"closed overlay passes real keyboard input to a different process");
                SendMessageW(overlay.hwnd(),WM_CHAR,'z',0);
                ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
                check(ImGui::GetIO().InputQueueCharacters.empty() && !ImGui::IsKeyDown(ImGuiKey_W) && !ImGui::GetIO().MouseDown[0],"closed menu has no stuck key, button or queued character");ImGui::EndFrame();
            }
            std::puts("PASS: three open/close cycles; cross-process mouse, keyboard, focus, capture and visible/transparent composition.");result=0;
        }
    } catch(const std::exception& error){std::fprintf(stderr,"FAIL: %s\n",error.what());}
    overlay.set_interactive(false);
    if(GetForegroundWindow()==host || GetForegroundWindow()==overlay.hwnd())SetCursorPos(original.x,original.y);
    if(imgui){ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();}overlay.destroy();
    if(host)PostMessageW(host,WM_CLOSE,0,0);WaitForSingleObject(process.hProcess,2000);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);return result;
}
