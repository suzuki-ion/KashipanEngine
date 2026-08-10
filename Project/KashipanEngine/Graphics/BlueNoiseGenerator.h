#pragma once
#include <cstdint>
#include <memory>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;
class PipelineManager;
class Renderer;
class RWStructuredBufferResource;

/// @brief ブルーノイズによるディザ閾値テーブルをコンピュートシェーダーで生成するクラス
/// @details 白色ノイズ→2次元DFT(順方向)→周波数フィルタ(低周波抑制)→逆DFT→Bitonic Sortに
///          よるランク付け→正規化、という一連のパスをRendererの構築時に1回だけ実行し
///          （GPU完了まで同期的に待つ）、結果を StructuredBuffer<float>（1辺GetSize()の
///          正方形に並んだ、重複の無い0〜1のディザ閾値）としてObjectPS.hlslから参照できる
///          形で保持する。
///          2次元DFTは高速フーリエ変換ではなく直接計算（O(N^4)）で実装している。このエンジンの
///          コンピュートシェーダーにはgroupshared/GroupMemoryBarrierを使う前例が無く、共有メモリを
///          使うバタフライ演算によるFFTは実装・検証コストが高いため、1回限りの起動時生成であれば
///          直接計算でも実用上十分速い（GPU上でミリ秒〜数十ミリ秒程度）と判断し、単純さを優先した。
///          同様の理由でBitonic Sortも「グローバル」実装（1パスごとにC++側からDispatchを呼び直す）
///          にしている。
class BlueNoiseGenerator final {
public:
    BlueNoiseGenerator() = default;
    ~BlueNoiseGenerator();

    BlueNoiseGenerator(const BlueNoiseGenerator &) = delete;
    BlueNoiseGenerator &operator=(const BlueNoiseGenerator &) = delete;
    BlueNoiseGenerator(BlueNoiseGenerator &&) = delete;
    BlueNoiseGenerator &operator=(BlueNoiseGenerator &&) = delete;

    /// @brief ブルーノイズを生成する（Rendererのコンストラクタから1回だけ呼ぶ想定。
    ///        内部でGPUコマンドを発行し、完了まで同期的に待つ）
    void Generate(DirectXCommon *directXCommon, PipelineManager *pipelineManager, Passkey<Renderer>);

    /// @brief 生成済みかどうか
    bool IsReady() const noexcept { return ready_; }
    /// @brief 生成結果（1辺GetSize()の正方形に並んだ、重複の無い0〜1のディザ閾値）
    RWStructuredBufferResource *GetResultBuffer() const noexcept { return ditherValues_.get(); }
    /// @brief 生成したブルーノイズテーブルの1辺のサイズ
    std::uint32_t GetSize() const noexcept { return kSize; }

private:
    /// @brief ブルーノイズテーブルの1辺のサイズ（2の累乗。Bitonic Sortの都合上、総要素数kSize*kSizeも
    ///        2の累乗である必要がある）
    static constexpr std::uint32_t kSize = 64;

    std::unique_ptr<RWStructuredBufferResource> bufferA_;
    std::unique_ptr<RWStructuredBufferResource> bufferB_;
    std::unique_ptr<RWStructuredBufferResource> sortKeys_;
    std::unique_ptr<RWStructuredBufferResource> sortIndices_;
    /// @brief 最終出力（createSrv=trueで確保し、生成後はPixelステージから読めるようにする）
    std::unique_ptr<RWStructuredBufferResource> ditherValues_;
    bool ready_ = false;
};

} // namespace KashipanEngine
