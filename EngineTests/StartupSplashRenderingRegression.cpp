// The software WIC target verifies pixels; the HWND test verifies native GPU resources.
#include "../SourceFiles/KashipanEngine/Splash/SplashScreen.cpp"
#include <cstdlib>
#include <iostream>
#include <vector>

namespace KashipanEngine {
std::string ProjectPaths::InEngineRoot(const std::string &path) { return path; }
void Require(bool value, const char *message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
void Check(HRESULT hr) { if (FAILED(hr)) { std::cerr << "HRESULT=" << std::hex << hr << '\n'; std::exit(2); } }
struct Surface { ComPtr<IWICBitmap> bitmap; ComPtr<ID2D1RenderTarget> target; };
Surface CreateSurface(WindowState &state, UINT width, UINT height) {
    Surface surface;
    Check(state.wic->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, surface.bitmap.GetAddressOf()));
    const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
    Check(state.factory->CreateWicBitmapRenderTarget(surface.bitmap.Get(), properties, surface.target.GetAddressOf()));
    return surface;
}
ComPtr<IWICBitmap> SolidSource(WindowState &state, BYTE r, BYTE g, BYTE b, BYTE alpha) {
    ComPtr<IWICBitmap> bitmap;
    Check(state.wic->CreateBitmap(16, 8, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.GetAddressOf()));
    ComPtr<IWICBitmapLock> lock;
    Check(bitmap->Lock(nullptr, WICBitmapLockWrite, lock.GetAddressOf()));
    UINT stride = 0, bytes = 0; BYTE *pixels = nullptr;
    Check(lock->GetStride(&stride)); Check(lock->GetDataPointer(&bytes, &pixels));
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 16; ++x) {
        BYTE *p = pixels + y * stride + x * 4;
        p[0] = b; p[1] = g; p[2] = r; p[3] = alpha;
    }
    return bitmap;
}
void RequirePixel(IWICBitmap *bitmap, int x, int y, BYTE r, BYTE g, BYTE b) {
    WICRect area{x,y,1,1}; BYTE pixel[4]{};
    Check(bitmap->CopyPixels(&area, 4, 4, pixel));
    Require(std::abs(int(pixel[2])-r)<=2 && std::abs(int(pixel[1])-g)<=2 && std::abs(int(pixel[0])-b)<=2,
        "Unexpected rendered pixel");
}
void SavePng(WindowState &state, IWICBitmap *bitmap, const wchar_t *path) {
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    Check(state.wic->CreateStream(stream.GetAddressOf()));
    Check(stream->InitializeFromFilename(path, GENERIC_WRITE));
    Check(state.wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf()));
    Check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
    Check(encoder->CreateNewFrame(frame.GetAddressOf(), nullptr)); Check(frame->Initialize(nullptr));
    UINT width=0,height=0; Check(bitmap->GetSize(&width,&height)); Check(frame->SetSize(width,height));
    auto format=GUID_WICPixelFormat32bppBGRA; Check(frame->SetPixelFormat(&format));
    Check(frame->WriteSource(bitmap,nullptr)); Check(frame->Commit()); Check(encoder->Commit());
}
void Render(Surface &surface, WindowState &state, double time, UINT width, UINT height) {
    surface.target->BeginDraw(); DrawLogoFrame(surface.target.Get(), state, SplashAnimation::Sample(time), width, height);
    Check(surface.target->EndDraw());
}
void RunRenderingChecks() {
    WindowState fixture;
    Require(InitializeFactories(fixture), "Factory initialization failed");
    fixture.gearSource=SolidSource(fixture,255,255,255,255);
    fixture.breadSource=SolidSource(fixture,0,0,0,0);
    fixture.textSource=SolidSource(fixture,0,255,0,255);
    for (const auto size : {SplashAnimation::ClientSize{1280,720}, SplashAnimation::ClientSize{1920,1080}}) {
        auto surface=CreateSurface(fixture,size.width,size.height);
        Require(CreateTargetResources(surface.target.Get(),fixture), "WIC target resources failed");
        Render(surface,fixture,2,size.width,size.height);
        RequirePixel(surface.bitmap.Get(),size.width/2,size.height/2,255,255,255);
        RequirePixel(surface.bitmap.Get(),size.width/2,size.height*4/5,0,255,0);
        RequirePixel(surface.bitmap.Get(),size.width/2,size.height/5,247,192,82);
        Render(surface,fixture,1.75,size.width,size.height);
        RequirePixel(surface.bitmap.Get(),size.width/2,size.height*4/5,101,64,24);
    }
    auto rotated=CreateSurface(fixture,1280,720);
    auto marker=SolidSource(fixture,255,0,0,255);
    {
        ComPtr<IWICBitmapLock> lock; Check(marker->Lock(nullptr,WICBitmapLockWrite,lock.GetAddressOf()));
        UINT stride=0,bytes=0; BYTE *pixels=nullptr; Check(lock->GetStride(&stride)); Check(lock->GetDataPointer(&bytes,&pixels));
        for(int y=0;y<8;++y) for(int x=8;x<16;++x) { BYTE *p=pixels+y*stride+x*4; p[0]=255;p[1]=0;p[2]=0;p[3]=255; }
    }
    ComPtr<ID2D1Bitmap> markerBitmap;
    Check(rotated.target->CreateBitmapFromWicBitmap(marker.Get(),nullptr,markerBitmap.GetAddressOf()));
    rotated.target->BeginDraw(); rotated.target->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    rotated.target->SetTransform(D2D1::Matrix3x2F::Scale(2.0f/3.0f,2.0f/3.0f));
    DrawSprite(rotated.target.Get(),markerBitmap.Get(),960,540,256,64,90); Check(rotated.target->EndDraw());
    RequirePixel(rotated.bitmap.Get(),640,320,0,0,255); RequirePixel(rotated.bitmap.Get(),640,400,255,0,0);

    WindowState logo; logo.folder=L"EngineData/UI/Splash";
    Require(InitializeFactories(logo) && DecodeBitmap(logo,L"engineLogoGear.png",logo.gearSource) &&
        DecodeBitmap(logo,L"engineLogoBread.png",logo.breadSource) && DecodeBitmap(logo,L"engineLogoText.png",logo.textSource), "Logo decoding failed");
    auto preview=CreateSurface(logo,1280,720);
    Require(CreateTargetResources(preview.target.Get(),logo), "Preview resources failed");
    Render(preview,logo,2,1280,720); SavePng(logo,preview.bitmap.Get(),L"Generated/StartupSplashRegression/HD-Logo.png");
    Render(preview,logo,3.4,1280,720); SavePng(logo,preview.bitmap.Get(),L"Generated/StartupSplashRegression/HD-Loading.png");
    std::cout << "Direct2D software pixel regression passed (HD/FHD placement, wipe, rotation)\n";
}
void RunDeviceChecks() {
    WindowState state; state.folder=L"EngineData/UI/Splash";
    WNDCLASSW wc{}; wc.lpfnWndProc=WindowProc; wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"SplashD2DRegression";
    Require(RegisterClassW(&wc)!=0,"Test class failed");
    HWND hwnd=CreateWindowW(wc.lpszClassName,L"Direct2D regression",WS_OVERLAPPEDWINDOW,0,0,800,600,nullptr,nullptr,wc.hInstance,&state);
    Require(hwnd!=nullptr,"Test window failed"); state.firstPaintTick=NowTicks();
    Require(InitializeGraphics(hwnd,state),"Native target failed");
    std::cout << "Direct2D HWND backend=" << (SplashScreen::GetRenderingBackend()==SplashScreen::RenderingBackend::Hardware ? "hardware" : "software") << '\n';
    Paint(hwnd,state);
    DiscardDeviceResources(state);
    Require(!state.target && !state.gear && !state.brown && state.gearSource,"Dependent resources must be released while decoded sources survive");
    Require(InitializeGraphics(hwnd,state),"Device resource recreation failed");
    SetWindowPos(hwnd,nullptr,0,0,900,700,SWP_NOACTIVATE|SWP_NOZORDER); Paint(hwnd,state);
    RECT client{}; GetClientRect(hwnd,&client);
    Require(state.target->GetPixelSize().width==static_cast<UINT32>(client.right) &&
        state.target->GetPixelSize().height==static_cast<UINT32>(client.bottom),"Render target resize failed");
    DiscardDeviceResources(state);
    Require(InitializeGraphics(hwnd,state,true) && SplashScreen::GetRenderingBackend()==SplashScreen::RenderingBackend::Software,"Explicit software fallback failed");
    Paint(hwnd,state); DiscardDeviceResources(state); DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName,wc.hInstance);
    std::cout << "Direct2D device resource regression passed (recreate, resize, software fallback)\n";
}
}
int main() { KashipanEngine::RunRenderingChecks(); KashipanEngine::RunDeviceChecks(); }
