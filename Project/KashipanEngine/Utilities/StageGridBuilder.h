#pragma once
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "Math/Vector3.h"
#include "Utilities/StageGraphGenerator.h"
#include "Utilities/WaveFunctionCollapse.h"

namespace KashipanEngine {

/// @brief StageGraphGeneratorが生成した抽象的な部屋グラフを、実際のタイルグリッド
///        （WaveFunctionCollapse）へ展開するユーティリティクラス
/// @details 部屋ごとに指定サイズのタイルブロックを固定し、部屋グラフ上で接続されている
///          部屋同士の間（部屋の隙間）に通路タイルを固定することで、部屋グラフの連結性を
///          そのままタイルグリッド上の物理的な連結性として保証する。部屋の隙間のうち通路
///          にならない部分は固定されないため、WaveFunctionCollapse::Solve()で壁等の装飾
///          として埋められる
/// @note 事前にwfc側で使用する全タイルID（部屋種別ごとのタイル・通路タイル）を
///       RegisterTileで登録しておく必要がある（Build()はタイルの登録は行わない）
class StageGridBuilder final {
public:
    StageGridBuilder() = default;
    ~StageGridBuilder() = default;

    /// @brief 部屋1つ分が占めるタイルサイズを設定する
    void SetRoomSize(std::uint32_t sizeX, std::uint32_t sizeY, std::uint32_t sizeZ);

    /// @brief 隣接する部屋ブロックの間に空ける隙間（タイル数）を設定する
    /// @details 隙間のうち通路として使われない部分はWaveFunctionCollapseの装飾対象として残る
    void SetRoomSpacing(std::uint32_t spacing);

    /// @brief 部屋同士を繋ぐ通路の太さ（タイル数）を設定する
    /// @details 部屋の隙間（RoomSpacing）を超える値を指定しても、隙間の幅で切り詰められる
    void SetCorridorWidth(std::uint32_t width);

    /// @brief タイル1個分に対応する実際のワールド座標上のサイズ
    void SetTileWorldSize(float size);

    /// @brief 部屋種別ごとに固定するタイルIDを設定する
    void SetRoomTileID(RoomType type, std::uint32_t tileID);

    /// @brief SetRoomTileIDで個別設定されていない部屋種別に使う既定のタイルID
    void SetDefaultRoomTileID(std::uint32_t tileID);

    /// @brief 部屋同士を繋ぐ通路に固定するタイルID
    void SetCorridorTileID(std::uint32_t tileID);

    /// @brief 部屋グラフの内容をwfcへ展開する
    /// @details wfc.SetGridSizeを内部で呼び出すため、既存のwfcの固定タイル・生成開始座標は
    ///          クリアされる。事前にwfc.RegisterTileで使用する全タイルIDを登録しておくこと
    /// @return 展開できた場合はtrue（デフォルトタイルID未設定、対応する部屋タイルID未設定、
    ///         タイルが未登録、部屋グラフが未生成、部屋同士が抽象グリッド上で隣接していない
    ///         等の場合はfalse）
    bool Build(const StageGraphGenerator &graph, WaveFunctionCollapse &wfc) const;

    /// @brief 部屋の中心タイル座標（WaveFunctionCollapseのグリッド座標系）を取得する
    /// @return 部屋が存在しない場合はfalse
    bool GetRoomGridCenter(const StageGraphGenerator &graph, std::uint32_t roomID,
        std::uint32_t &outX, std::uint32_t &outY, std::uint32_t &outZ) const;

    /// @brief 部屋の中心位置をワールド座標（Vector3）として取得する
    /// @return 部屋が存在しない場合はfalse
    bool GetRoomWorldCenter(const StageGraphGenerator &graph, std::uint32_t roomID, Vector3 &outPosition) const;

    /// @brief Buildで実際に必要となるWaveFunctionCollapseのグリッドサイズを取得する
    void GetRequiredGridSize(const StageGraphGenerator &graph,
        std::uint32_t &outWidth, std::uint32_t &outHeight, std::uint32_t &outDepth) const;

private:
    /// @brief 抽象部屋グリッド上の1マス分が占めるタイル範囲の原点を求める
    std::uint32_t OriginOf(std::uint32_t slot, std::uint32_t cellSize) const;

    std::uint32_t roomSizeX_ = 4;
    std::uint32_t roomSizeY_ = 3;
    std::uint32_t roomSizeZ_ = 4;
    std::uint32_t roomSpacing_ = 2;
    std::uint32_t corridorWidth_ = 1;
    float tileWorldSize_ = 1.0f;

    std::unordered_map<RoomType, std::uint32_t> roomTileIDs_;
    std::optional<std::uint32_t> defaultRoomTileID_;
    std::optional<std::uint32_t> corridorTileID_;
};

} // namespace KashipanEngine
