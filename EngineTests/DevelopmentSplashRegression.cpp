// 実際の Development 用実装を取り込み、その STA スレッド上で DOM を調べる。
#include "../SourceFiles/KashipanEngine/Splash/DevelopmentSplashScreen.cpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
namespace {
std::mutex captureMutex, probeMutex;
std::vector<KashipanEngine::LogLine> captured;
std::atomic<bool> capturing{false}, probeBusy{false}, sawClosing{false};
std::atomic<DWORD> probeThread{0};
KashipanEngine::JSON probeResult = KashipanEngine::JSON::object();
WNDPROC previousProcedure = nullptr;
constexpr UINT kProbe = WM_APP + 51;
bool missingUI = false;
void Require(bool value, const char *text) { if (!value) throw std::runtime_error(text); }
void QueueLines() {
    std::lock_guard lock(captureMutex);
    for (int i = 0; i < 12; ++i) captured.push_back({i == 11 ? "ログ <b>最終</b>" : "起動ログ " + std::to_string(i), KashipanEngine::LogSeverity::Error});
}
}
namespace KashipanEngine {
std::string ProjectPaths::InEngineRoot(const std::string &path) { return std::filesystem::absolute(missingUI ? "Generated/DevelopmentSplashRegression/missing-ui" : path).generic_string(); }
void BeginSplashLogCapture(Passkey<SplashScreen>) { std::lock_guard lock(captureMutex); captured.clear(); capturing.store(true); }
void EndSplashLogCapture(Passkey<SplashScreen>) { std::lock_guard lock(captureMutex); capturing.store(false); captured.clear(); }
std::vector<LogLine> FetchNewSplashLogLines(Passkey<SplashScreen>) { std::lock_guard lock(captureMutex); auto result=std::move(captured); captured.clear(); return result; }
class Window { public: static HWND Claim() { return SplashScreen::ClaimWindow({}); } };
class GameEngine { public: static void Ready() { SplashScreen::NotifyReady({}); } static void Finish() { SplashScreen::Finish({}); } };
LRESULT CALLBACK ProbeProcedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == kProbe) {
        if (!sWebView) { probeBusy.store(false); return 0; }
        sWebView->ExecuteScript(L"JSON.stringify({ready:!!document.querySelector('.logo-text'),title:document.querySelector('.logo-text')?.textContent,count:document.querySelectorAll('#log-list li').length,last:document.querySelector('#log-list li:last-child')?.textContent,markup:!!document.querySelector('#log-list b'),closing:document.body.classList.contains('closing')})",
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>([](HRESULT result,LPCWSTR data)->HRESULT {
                if (SUCCEEDED(result) && data) {
                    try {
                        auto outer=JSON::parse(ConvertString(std::wstring(data)));
                        if (outer.is_string()) {
                            auto value=JSON::parse(outer.get<std::string>());
                            if (value.value("closing",false)) sawClosing.store(true);
                            std::lock_guard lock(probeMutex); probeResult=std::move(value);
                        }
                    } catch (...) {}
                }
                probeThread.store(GetCurrentThreadId()); probeBusy.store(false); return S_OK;
            }).Get());
        return 0;
    }
    return CallWindowProcW(previousProcedure,hwnd,message,wp,lp);
}
int Execute(PasskeyForWinMain, const std::string &mode) {
    missingUI=mode=="missing-ui";
    std::filesystem::create_directories("Generated/DevelopmentSplashRegression/missing-ui");
    const auto profile=std::filesystem::absolute("Generated/DevelopmentSplashRegression/Profile-"+std::to_string(GetCurrentProcessId()));
    SetEnvironmentVariableW(L"KASHIPAN_SPLASH_USER_DATA_FOLDER",profile.c_str());
    const auto start=GetTickCount64();
    try {
        SplashScreen::Show({});
        HWND hwnd=sHwnd.load(); Require(hwnd && capturing.load(),"Legacy window/capture not started");
        Require(GetWindowThreadProcessId(hwnd,nullptr)!=GetCurrentThreadId(),"Legacy window must own a separate UI thread");
        RECT client{};GetClientRect(hwnd,&client);
        Require(client.right==640 && client.bottom==360,"Legacy size changed");
        Require((GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_POPUP) && (GetWindowLongPtrW(hwnd,GWL_EXSTYLE)&WS_EX_TOOLWINDOW),"Legacy styles changed");
        Require(!SplashScreen::HasStartupWindow() && !Window::Claim(),"Development must never adopt a startup HWND");
        Require(SplashScreen::GetRenderingBackend()==SplashScreen::RenderingBackend::None,"Development uses the Release renderer");
        if (mode=="content") {
            previousProcedure=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd,GWLP_WNDPROC));
            SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(ProbeProcedure));
            bool loaded=false,logs=false;
            const auto deadline=GetTickCount64()+20000;
            while (GetTickCount64()<deadline) {
                if (!probeBusy.exchange(true)) PostMessageW(hwnd,kProbe,0,0);
                { std::lock_guard lock(probeMutex);
                  if (probeResult.value("ready",false)) {
                    loaded=true;
                    if (probeResult.value("count",0)==6 && probeResult.value("last",std::string{})=="ログ <b>最終</b>") { logs=true; break; }
                  }
                }
                if (loaded) QueueLines();
                Sleep(35);
            }
            Require(loaded && logs,"Legacy HTML or streaming logs did not appear");
            { std::lock_guard lock(probeMutex);
              Require(probeResult.value("title",std::string{})=="KashipanEngine" && !probeResult.value("markup",true),"Legacy page/textContent mismatch");
              std::ofstream file("Generated/DevelopmentSplashRegression/Content.json"); file<<probeResult.dump(2);
            }
            Require(probeThread.load()==GetWindowThreadProcessId(hwnd,nullptr),"DOM access escaped the UI STA");
            std::thread monitor([hwnd] {
                const auto end=GetTickCount64()+1500;
                while (IsWindow(hwnd) && GetTickCount64()<end) {
                    if (!probeBusy.exchange(true)) PostMessageW(hwnd,kProbe,0,0);
                    Sleep(12);
                }
            });
            SplashScreen::Close({});monitor.join();
            Require(sawClosing.load(),"Legacy fade notification was not received");
        } else if (mode=="user-close") {
            PostMessageW(hwnd,WM_CLOSE,0,0);
            const auto end=GetTickCount64()+5000;
            while (SplashScreen::IsActive() && GetTickCount64()<end) Sleep(10);
            SplashScreen::Close({}); SplashScreen::CheckCancellation();
        } else {
            SplashScreen::Close({});
            Require(GetTickCount64()-start>=900,"Legacy minimum display/fade was skipped");
        }
        Require(!IsWindow(hwnd) && !SplashScreen::IsActive() && !capturing.load(),"Legacy window/capture not closed");
        SplashScreen::Close({});
        SplashScreen::Show({});SplashScreen::Close({}); // 起動、遅延コールバック、イベントの寿命を繰り返し検証する。
        Require(!SplashScreen::IsActive() && !capturing.load(),"Repeated startup failed");
        std::cout<<"Development splash "<<mode<<" passed\n";return 0;
    } catch(const std::exception &error) { SplashScreen::Close({});std::cerr<<error.what()<<'\n';return 1; }
}
}
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR args,int) { return KashipanEngine::Execute({},args); }
int main(int argc,char **argv) { return WinMain(nullptr,nullptr,argc>1?argv[1]:const_cast<char *>("lifecycle"),0); }
