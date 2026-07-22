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
        Log("ImageExporter: commandQueue または resource が null です。", LogSeverity::Warning);
        return false;
    }

    DirectX::ScratchImage image;
    HRESULT hr = DirectX::CaptureTexture(commandQueue, resource, false, image, state, state);
    if (FAILED(hr)) {
        Log("ImageExporter: テクスチャのキャプチャに失敗しました。", LogSeverity::Warning);
        return false;
    }

    const auto *img = image.GetImage(0, 0, 0);
    if (!img) {
        Log("ImageExporter: キャプチャした画像が空です。", LogSeverity::Warning);
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
        Log("ImageExporter: 画像ファイルの保存に失敗しました。 path=" + filePath, LogSeverity::Warning);
        return false;
    }
    return true;
}

} // namespace KashipanEngine
