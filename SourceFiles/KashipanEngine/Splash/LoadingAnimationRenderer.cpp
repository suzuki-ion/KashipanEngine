#include "LoadingAnimation.h"
#include "Utilities/Conversion/ConvertString.h"
#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <map>
#pragma comment(lib,"d2d1.lib")
#pragma comment(lib,"windowscodecs.lib")
namespace KashipanEngine::LoadingAnimation {
using Microsoft::WRL::ComPtr;
struct Renderer::Impl {
    ComPtr<ID2D1Factory> factory;
    ComPtr<IWICImagingFactory> wic;
    ComPtr<ID2D1HwndRenderTarget> target;
    ComPtr<ID2D1SolidColorBrush> brush;
    struct Image { UINT width=0,height=0; std::vector<BYTE> pixels; ComPtr<ID2D1Bitmap> bitmap; int r=-1,g=-1,b=-1; };
    std::map<std::filesystem::path,Image> images;
    HWND window=nullptr;
    bool hardware=false;
    bool Initialize(HWND hwnd) {
        if(!factory && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf())))return false;
        if(!wic && FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(wic.GetAddressOf()))))return false;
        RECT r{}; GetClientRect(hwnd,&r); const auto size=D2D1::SizeU(std::max(1L,r.right),std::max(1L,r.bottom));
        if(target && window!=hwnd) { target.Reset(); images.clear(); brush.Reset(); }
        window=hwnd;
        if(!target) {
            auto props=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_HARDWARE); props.dpiX=96; props.dpiY=96;
            hardware=SUCCEEDED(factory->CreateHwndRenderTarget(props,D2D1::HwndRenderTargetProperties(hwnd,size),target.ReleaseAndGetAddressOf()));
            if(!hardware) {
                props.type=D2D1_RENDER_TARGET_TYPE_SOFTWARE;
                if(FAILED(factory->CreateHwndRenderTarget(props,D2D1::HwndRenderTargetProperties(hwnd,size),target.ReleaseAndGetAddressOf())))return false;
            }
            if(FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),brush.ReleaseAndGetAddressOf())))return false;
        }
        const auto old=target->GetPixelSize();
        return (old.width==size.width && old.height==size.height) || SUCCEEDED(target->Resize(size));
    }
    ID2D1Bitmap *Bitmap(const std::filesystem::path &path,const Node &n) {
        auto &im=images[path];
        if(im.pixels.empty()) {
            ComPtr<IWICBitmapDecoder> decoder; ComPtr<IWICBitmapFrameDecode> frame; ComPtr<IWICFormatConverter> converter;
            if(FAILED(wic->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,decoder.GetAddressOf())) ||
                FAILED(decoder->GetFrame(0,frame.GetAddressOf())) || FAILED(wic->CreateFormatConverter(converter.GetAddressOf())) ||
                FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)) ||
                FAILED(converter->GetSize(&im.width,&im.height)) || !im.width || !im.height || im.width>16384 || im.height>16384)return nullptr;
            im.pixels.resize(size_t(im.width)*im.height*4);
            if(FAILED(converter->CopyPixels(nullptr,im.width*4,static_cast<UINT>(im.pixels.size()),im.pixels.data()))) { im.pixels.clear(); return nullptr; }
        }
        const int r=static_cast<int>(std::clamp(n.r,0.f,1.f)*255),g=static_cast<int>(std::clamp(n.g,0.f,1.f)*255),b=static_cast<int>(std::clamp(n.b,0.f,1.f)*255);
        if(!im.bitmap || r!=im.r || g!=im.g || b!=im.b) {
            auto pixels=im.pixels;
            for(size_t p=0;p<pixels.size();p+=4) { pixels[p]=BYTE(pixels[p]*b/255); pixels[p+1]=BYTE(pixels[p+1]*g/255); pixels[p+2]=BYTE(pixels[p+2]*r/255); }
            const auto props=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
            if(FAILED(target->CreateBitmap(D2D1::SizeU(im.width,im.height),pixels.data(),im.width*4,props,im.bitmap.ReleaseAndGetAddressOf())))return nullptr;
            im.r=r; im.g=g; im.b=b;
        }
        return im.bitmap.Get();
    }
};
Renderer::Renderer():impl_(std::make_unique<Impl>()){}
Renderer::~Renderer()=default;
bool Renderer::IsHardware() const { return impl_->target && impl_->hardware; }
bool Renderer::Draw(void *handle,const Document &d,const Frame &f) {
    auto &p=*impl_; if(!p.Initialize(static_cast<HWND>(handle)))return false;
    const auto nodes=d.Evaluate(f); const auto size=p.target->GetPixelSize();
    const float scale=std::min(size.width/1920.f,size.height/1080.f);
    const auto viewport=D2D1::Matrix3x2F::Scale(scale,-scale)*D2D1::Matrix3x2F::Translation((size.width-1920*scale)/2,(size.height+1080*scale)/2);
    std::vector<int> order; for(int i=0;i<static_cast<int>(nodes.size());++i)if(!f.finished && nodes[i].sprite && nodes[i].active)order.push_back(i);
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){return nodes[a].priority<nodes[b].priority;});
    p.target->BeginDraw(); p.target->SetTransform(D2D1::Matrix3x2F::Identity()); p.target->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    for(int index:order) {
        auto n=nodes[index];
        if(n.colorBlend==1) { n.r*=n.materialR; n.g*=n.materialG; n.b*=n.materialB; n.a*=n.materialA; }
        const bool identityUV=n.u==0 && n.v==0 && n.us==1 && n.vs==1;
        if(identityUV) { n.u=n.materialU; n.v=n.materialV; n.us=n.materialUS; n.vs=n.materialVS; }
        else if(n.uvCombine==0) { n.u+=n.materialU*n.us; n.v+=n.materialV*n.vs; n.us*=n.materialUS; n.vs*=n.materialVS; }
        else if(n.uvCombine==1) { n.u=n.u*n.materialUS+n.materialU; n.v=n.v*n.materialVS+n.materialV; n.us*=n.materialUS; n.vs*=n.materialVS; }
        auto world=D2D1::Matrix3x2F::Identity(); float alpha=n.a;
        for(int i=index,depth=0;i>=0 && depth++<static_cast<int>(nodes.size());i=nodes[i].parent) {
            const auto &a=nodes[i]; alpha*=a.opacity;
            world=world*D2D1::Matrix3x2F::Scale(a.sx,a.sy)*D2D1::Matrix3x2F::Rotation(a.rotation*180.f/3.14159265359f)*D2D1::Matrix3x2F::Translation(a.x,a.y);
        }
        p.target->SetTransform(D2D1::Matrix3x2F::Scale(1,-1)*world*viewport);
        const auto rect=D2D1::RectF(-n.anchorX,n.anchorY-1,1-n.anchorX,n.anchorY);
        if(n.image.empty()) {
            p.brush->SetColor(D2D1::ColorF(std::clamp(n.r,0.f,1.f),std::clamp(n.g,0.f,1.f),std::clamp(n.b,0.f,1.f),std::clamp(alpha,0.f,1.f)));
            p.target->FillRectangle(rect,p.brush.Get());
        } else if(auto *bitmap=p.Bitmap(d.folder/Utf8StringToPath(n.image),n)) {
            const auto sz=bitmap->GetSize(); const auto source=D2D1::RectF(n.u*sz.width,n.v*sz.height,(n.u+n.us)*sz.width,(n.v+n.vs)*sz.height);
            p.target->DrawBitmap(bitmap,rect,std::clamp(alpha,0.f,1.f),D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,source);
        } else { p.target->EndDraw(); return false; }
    }
    const auto hr=p.target->EndDraw();
    if(FAILED(hr)) { p.target.Reset(); p.images.clear(); p.brush.Reset(); }
    return SUCCEEDED(hr);
}
}
