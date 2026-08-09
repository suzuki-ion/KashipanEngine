#include "ImageExporter.h"

#include <DirectXTex.h>
#include <wincodec.h>
#include <filesystem>
#include <system_error>

#include "Debug/Logger.h"
#include "Utilities/Conversion/ConvertString.h"

namespace KashipanEngine {

bool ImageExporter::SaveTextureToFile(ID3D12CommandQueue *commandQueue, ID3D12Resource *resource,
    D3D12_RESOURCE_STATES state, const std::string &filePath) {
    LogScope scope;
    if (!commandQueue || !resource) {
        Log(Translation("engine.imageexporter.failed.nullargument"), LogSeverity::Warning);
        return false;
    }

    DirectX::ScratchImage image;
    HRESULT hr = DirectX::CaptureTexture(commandQueue, resource, false, image, state, state);
    if (FAILED(hr)) {
        Log(Translation("engine.imageexporter.failed.capture"), LogSeverity::Warning);
        return false;
    }

    // ScreenBufferは背景等の未描画ピクセルがalpha=0のまま残ることがある。
    // 画面表示時はalphaが無視される（不透明ウィンドウ／エディターのアルファブレンドで背景色に馴染む）が、
    // PNG等はalphaを保持するため、多くのビューアで透明部分が白系に合成され画面と見た目が変わってしまう。
    // 保存画像は常に不透明として書き出す。
    DirectX::ScratchImage opaqueImage;
    hr = DirectX::TransformImage(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        [](DirectX::XMVECTOR *outPixels, const DirectX::XMVECTOR *inPixels, size_t width, size_t) {
            for (size_t x = 0; x < width; ++x) {
                outPixels[x] = DirectX::XMVectorSetW(inPixels[x], 1.0f);
            }
        }, opaqueImage);
    if (FAILED(hr)) {
        Log(Translation("engine.imageexporter.failed.alpha"), LogSeverity::Warning);
        return false;
    }

    const auto *img = opaqueImage.GetImage(0, 0, 0);
    if (!img) {
        Log(Translation("engine.imageexporter.failed.empty"), LogSeverity::Warning);
        return false;
    }

    const auto path = Utf8StringToPath(filePath);
    const auto ext = path.extension().string();
    GUID containerFormat = GUID_ContainerFormatPng;
    if (ext == ".jpg" || ext == ".jpeg") {
        containerFormat = GUID_ContainerFormatJpeg;
    } else if (ext == ".bmp") {
        containerFormat = GUID_ContainerFormatBmp;
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    hr = DirectX::SaveToWICFile(*img, DirectX::WIC_FLAGS_NONE, containerFormat, path.c_str());
    if (FAILED(hr)) {
        Log(Translation("engine.imageexporter.failed.save") + filePath, LogSeverity::Warning);
        return false;
    }
    return true;
}

} // namespace KashipanEngine
