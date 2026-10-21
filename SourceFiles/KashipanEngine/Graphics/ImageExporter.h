#pragma once
#include <d3d12.h>
#include <string>

namespace KashipanEngine {

/// @brief GPU上のテクスチャリソースを画像ファイルへ保存するユーティリティ
/// @details DirectXTexの CaptureTexture / SaveToWICFile を利用する。エディター・アプリケーション
///          いずれからも、また用途を問わず（ScreenBuffer以外の任意のID3D12Resourceにも）使える
///          汎用ユーティリティとして独立させている。
class ImageExporter final {
public:
    ImageExporter() = delete;

    /// @brief GPU上のテクスチャリソースをキャプチャして画像ファイルへ保存する
    /// @details 内部でコマンドキューへコピー用コマンドを発行し、GPU完了を同期的に待つ（スコールする）。
    ///          そのため毎フレーム呼ぶような用途ではなく、スクリーンショット等の単発利用を想定している。
    /// @param commandQueue キャプチャに使うコマンドキュー
    /// @param resource キャプチャ対象のリソース
    /// @param state 呼び出し時点でのリソースの状態（呼び出し前後でこの状態のまま変化しないこと）
    /// @param filePath 保存先パス。拡張子から形式を判定する（.png/.jpg/.jpeg/.bmp、それ以外はpng扱い）
    /// @return 成功した場合は true
    static bool SaveTextureToFile(ID3D12CommandQueue *commandQueue, ID3D12Resource *resource,
        D3D12_RESOURCE_STATES state, const std::string &filePath);
};

} // namespace KashipanEngine
