#include "login.hpp"
#include "credentials.hpp"
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <gdiplus.h>
#include <fstream>

static std::atomic<ULONGLONG> animation_clock{1000};
ULONGLONG capri_login_test_clock(){return animation_clock.load();}

static bool capture(HWND window,const std::filesystem::path& output) {
    RECT r{};GetClientRect(window,&r);HDC dc=GetDC(window),memory=CreateCompatibleDC(dc);
    HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto previous=SelectObject(memory,bitmap);
    PrintWindow(window,memory,PW_CLIENTONLY);
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;bool saved=false;
    if(Gdiplus::GdiplusStartup(&token,&input,nullptr)==Gdiplus::Ok) {
        {Gdiplus::Bitmap image(bitmap,nullptr);UINT count=0,size=0;Gdiplus::GetImageEncodersSize(&count,&size);
         std::vector<BYTE> bytes(size);auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(bytes.data());
         Gdiplus::GetImageEncoders(count,size,encoders);
         for(UINT i=0;i<count;++i)if(wcscmp(encoders[i].MimeType,L"image/png")==0)saved=image.Save(output.c_str(),&encoders[i].Clsid,nullptr)==Gdiplus::Ok;}
        Gdiplus::GdiplusShutdown(token);
    }
    SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,dc);return saved;
}
static unsigned long long border_frame(HWND window) {
    RECT r{};GetClientRect(window,&r);HDC dc=GetDC(window),memory=CreateCompatibleDC(dc);
    HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto previous=SelectObject(memory,bitmap);
    PrintWindow(window,memory,PW_CLIENTONLY);unsigned long long hash=1469598103934665603ULL;
    for(int y=3;y<r.bottom-3;++y){hash^=GetPixel(memory,r.right-2,y);hash*=1099511628211ULL;}
    SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,dc);return hash;
}

// Offline fixture: authentication transport is verified separately by WinHTTP tests.
AuthInfo Auth::validate_online(const std::string& key) {
    Sleep(350); // Exercise the real loading state with deterministic fixture latency.
    AuthInfo result;result.result=key=="FIXTURE-LOGIN-KEY"?AuthResult::OK:AuthResult::BAD_KEY;return result;
}
int main(int argc,char** argv) {
    const bool missing_assets=argc>1 && std::strcmp(argv[1],"--missing-assets")==0;
    const bool preview=argc>2 && std::strcmp(argv[2],"--preview")==0;
    if(!preview && FindWindowW(nullptr,L"Counter-Strike 2")){std::puts("SKIP: close CS2 to test the waiting screen.");return 77;}
    namespace fs=std::filesystem;
    const auto original=fs::current_path();
    const auto output=argc>1 && !missing_assets?fs::absolute(argv[1]):fs::path{};
    const auto work=original/"verificacao"/"testes-login"/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    fs::create_directories(work);fs::current_path(work);
    if(!missing_assets){
        fs::create_directories("assets/collision");fs::create_directories("assets/radar");
        for(const char* map:{"de_dust2","de_mirage","de_inferno","de_nuke","de_ancient","de_anubis","de_overpass","de_vertigo","de_train","cs_office","cs_italy"})std::ofstream(fs::path("assets/collision")/(std::string(map)+".bvh"))<<"fixture";
        std::ofstream("assets/radar/de_dust2.json")<<"{}";
    }
    Config cfg;Auth auth;AuthInfo info;Memory memory;std::atomic<bool> waiting=false,masked=false,captured=output.empty(),motion=false,loading=false,background=false;
    std::thread driver([&] {
        HWND window=nullptr;
        for(int i=0;i<200 && !window;++i) {
            EnumWindows([](HWND h,LPARAM data)->BOOL {
                DWORD pid=0;GetWindowThreadProcessId(h,&pid);
                wchar_t title[80]{};GetWindowTextW(h,title,80);
                if(pid==GetCurrentProcessId() && IsWindowVisible(h) && wcscmp(title,L"Capri - Acesso")==0){*reinterpret_cast<HWND*>(data)=h;return FALSE;}
                return TRUE;
            },reinterpret_cast<LPARAM>(&window));Sleep(10);
        }
        if(!window)return;
        BOOL animations=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);
        RECT first{},second{},settled{};
        animation_clock=1045;SendMessageW(window,WM_TIMER,2,0);GetWindowRect(GetDlgItem(window,1004),&first);
        animation_clock=1180;SendMessageW(window,WM_TIMER,2,0);GetWindowRect(GetDlgItem(window,1004),&second);
        animation_clock=1660;SendMessageW(window,WM_TIMER,2,0);GetWindowRect(GetDlgItem(window,1004),&settled);
        motion=animations?(first.top>second.top && second.top>=settled.top):(first.top==settled.top);
        const bool foreground=GetForegroundWindow()==window;
        SendMessageW(window,WM_TIMER,3,0);const auto ambient=border_frame(window);
        animation_clock=1910;SendMessageW(window,WM_TIMER,3,0);auto next=border_frame(window);
        if(animations && ambient==next){animation_clock=2310;SendMessageW(window,WM_TIMER,3,0);next=border_frame(window);}
        std::printf("Ambient: foreground=%d first=%llu second=%llu\n",foreground,ambient,next);
        background=animations && foreground?ambient!=next:ambient==next;
        {HDC dc=GetDC(window);auto previous=SelectObject(dc,reinterpret_cast<HFONT>(SendDlgItemMessageW(window,1004,WM_GETFONT,0,0)));wchar_t face[100]{};GetTextFaceW(dc,100,face);std::printf("Heading font: %ls\n",face);SelectObject(dc,previous);ReleaseDC(window,dc);}
        SetDlgItemTextW(window,1001,preview?L"INVALID-PREVIEW-KEY":L"FIXTURE-LOGIN-KEY");
        masked=SendDlgItemMessageW(window,1001,EM_GETPASSWORDCHAR,0,0)!=0;
        SendMessageW(window,WM_COMMAND,MAKEWPARAM(1010,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(window,1010)));
        masked=masked && SendDlgItemMessageW(window,1001,EM_GETPASSWORDCHAR,0,0)==0;
        SendMessageW(window,WM_COMMAND,MAKEWPARAM(1010,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(window,1010)));
        masked=masked && SendDlgItemMessageW(window,1001,EM_GETPASSWORDCHAR,0,0)!=0;
        if(!output.empty())captured=capture(window,output);
        PostMessageW(window,WM_COMMAND,IDOK,0);
        Sleep(70);loading=!IsWindowEnabled(GetDlgItem(window,IDOK));
        if(!output.empty()) {auto progress=output;progress.replace_filename(output.stem().wstring()+L"-validando.png");capture(window,progress);}
        if(preview){Sleep(500);PostMessageW(window,WM_COMMAND,IDCANCEL,0);return;}
        for(int i=0;i<200;++i) {
            wchar_t label[100]{};GetDlgItemTextW(window,IDOK,label,100);
            if(wcscmp(label,L"Aguardando Counter-Strike 2")==0 && IsWindowEnabled(GetDlgItem(window,IDCANCEL))) {waiting=true;break;}
            if(missing_assets && std::wstring(label).find(L"atualizacao")!=std::wstring::npos){waiting=true;break;}
            Sleep(20);
        }
        PostMessageW(window,WM_COMMAND,IDCANCEL,0);
    });
    const bool entered=show_login(cfg,auth,info,memory);driver.join();
    std::string restored,error;
    const bool protected_key=credentials::load("credentials.dat",restored,error)==credentials::Result::Loaded && restored=="FIXTURE-LOGIN-KEY";
    Config profile;const bool sanitized=missing_assets?!fs::exists("configs/default.hbc"):(profile.load("configs/default.hbc",&error) && !profile.saved_key[0]);
    const bool success=!entered && (preview || waiting) && masked && captured && motion && background && loading && !memory.is_valid() && (preview || (protected_key && sanitized));
    fs::current_path(original);
    std::error_code ignored;
    fs::remove_all(work/"assets",ignored);
    fs::remove(work/"credentials.dat",ignored);fs::remove(work/"configs/default.hbc",ignored);fs::remove(work/"configs",ignored);fs::remove(work,ignored);
    std::printf("Results: entered=%d waiting=%d masked=%d captured=%d entrance=%d background=%d loading=%d attached=%d protected=%d sanitized=%d\n",entered,int(waiting),int(masked),int(captured),int(motion),int(background),int(loading),memory.is_valid(),protected_key,sanitized);
    if(preview)std::puts("Preview: rejected fixture key; no game attachment or input.");
    if(preview)std::puts(success?"PASS: preview, foreground-aware ambient motion/pause, entrance, loading, show/hide key, cancellation; rejected fixture key, no attachment or game input.":"FAIL: login preview or motion state.");
    else std::puts(success?"PASS: animated background, entrance, loading, show/hide key, cancellable waiting, DPAPI and sanitized profile; no game input.":"FAIL: login motion/waiting or credential persistence.");
    return success?0:1;
}
