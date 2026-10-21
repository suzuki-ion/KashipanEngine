#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "Utilities/FileIO/JSON.h"

namespace KashipanEngine {

/// @brief 波動関数崩壊アルゴリズム（Wave Function Collapse）で使うタイル群と、
///        タイルを配置していく3次元グリッドを管理するユーティリティクラス
/// @details タイルは名前（文字列）で管理する。Solve()の内部では性能のため
///          名前を整数インデックスへ変換してから候補集合の演算を行う
class WaveFunctionCollapse final {
public:
    /// @brief タイルの接続方向
    enum class Direction : std::uint8_t {
        Up = 0,
        Down,
        Left,
        Right,
        Front,
        Back,
    };

    /// @brief 接続方向の数（上下左右前奥の最大6方向）
    static constexpr std::size_t kDirectionCount = 6;

    /// @brief WFCで扱うタイル1種類分の定義
    struct Tile final {
        /// @brief タイルを識別する名前（空文字は不可）
        std::string name;
        /// @brief 各方向ごとに接続可能なタイル名の一覧
        std::array<std::vector<std::string>, kDirectionCount> connections;
    };

    WaveFunctionCollapse() = default;
    ~WaveFunctionCollapse() = default;

    /// @brief 乱数生成に使うシード値を設定する
    /// @param seed シード値
    void SetSeed(std::uint32_t seed);

    /// @brief 現在設定されている乱数シード値を取得する
    std::uint32_t GetSeed() const noexcept { return seed_; }

    /// @brief グリッドのサイズを設定する（内部の3次元配列を再構築する）
    /// @details 既存の固定タイルと生成開始位置はクリアされる
    /// @param width X方向のサイズ
    /// @param height Y方向のサイズ
    /// @param depth Z方向のサイズ
    void SetGridSize(std::uint32_t width, std::uint32_t height, std::uint32_t depth);

    std::uint32_t GetGridWidth() const noexcept { return width_; }
    std::uint32_t GetGridHeight() const noexcept { return height_; }
    std::uint32_t GetGridDepth() const noexcept { return depth_; }

    /// @brief タイルを登録する
    /// @param tile 登録するタイル情報
    /// @return 登録に成功した場合はtrue（名前が空、または同名のタイルが既に登録済みの場合はfalse）
    bool RegisterTile(const Tile &tile);

    /// @brief 接続情報を持たない空のタイルを登録する（登録後にAddTileConnectionで接続を追加する用途）
    /// @return 登録に成功した場合はtrue（名前が空、または同名のタイルが既に登録済みの場合はfalse）
    bool RegisterTile(const std::string &name);

    /// @brief 登録済みタイルの、指定方向の接続先タイル名を1つ追加する
    /// @param tileName 接続を追加するタイルの名前
    /// @param direction 接続方向
    /// @param connectedTileName その方向に接続可能とするタイルの名前
    /// @return 追加できた場合はtrue（タイルが未登録の場合はfalse。既に追加済みの場合は何もせずtrue）
    bool AddTileConnection(const std::string &tileName, Direction direction, const std::string &connectedTileName);

    /// @brief 登録済みのタイルを削除する
    /// @details 削除対象のタイルで固定・確定されていたグリッド上のセルは解除される
    /// @param tileName 削除するタイルの名前
    /// @return 削除できた場合はtrue（該当タイルが存在しない場合はfalse）
    bool RemoveTile(const std::string &tileName);

    /// @brief 登録済みのタイル一覧を取得する
    const std::unordered_map<std::string, Tile> &GetTiles() const noexcept { return tiles_; }

    /// @brief 指定した座標のタイルを固定する
    /// @param x,y,z 固定するグリッド座標
    /// @param tileName 固定するタイルの名前
    /// @return 固定できた場合はtrue（座標が範囲外、またはタイルが未登録の場合はfalse）
    bool FixTile(std::uint32_t x, std::uint32_t y, std::uint32_t z, const std::string &tileName);

    /// @brief 指定した座標に固定されているタイル名を取得する
    /// @return 固定されていない、または座標が範囲外の場合はstd::nullopt
    std::optional<std::string> GetFixedTile(std::uint32_t x, std::uint32_t y, std::uint32_t z) const;

    /// @brief タイル生成を開始する座標を指定する
    /// @return 座標が範囲内であればtrue
    bool SetStartPosition(std::uint32_t x, std::uint32_t y, std::uint32_t z);

    /// @brief 指定済みの生成開始座標を取得する
    /// @return 未指定の場合はstd::nullopt
    std::optional<std::array<std::uint32_t, 3>> GetStartPosition() const noexcept { return startPosition_; }

    /// @brief 波動関数崩壊アルゴリズムを実行し、グリッド全体のタイルを確定させる
    /// @details 固定済みのセルはFixTileで指定したタイルを制約として扱う。生成開始座標が
    ///          指定されている場合、最初の1マスはその座標から崩壊させる（以降はエントロピー
    ///          （残り候補数）が最小のセルを乱数で選びながら崩壊・伝播を繰り返す）。
    ///          矛盾（候補が0件になるセル）が発生した場合はバックトラックせずfalseを返す。
    ///          再実行するたびに前回の解決結果はクリアされ、シードに基づき再抽選される。
    /// @return グリッド全体を矛盾なく確定できた場合はtrue
    bool Solve();

    /// @brief Solve()によって確定したタイル名を取得する
    /// @return 未確定（Solve未実行・矛盾で失敗・座標が範囲外）の場合はstd::nullopt
    std::optional<std::string> GetResolvedTile(std::uint32_t x, std::uint32_t y, std::uint32_t z) const;

    /// @brief シード値・グリッドサイズ・登録タイル・固定タイル・生成開始座標をJSONへ保存する
    /// @details Solve()による確定結果（GetResolvedTile）は保存されない（再度Solve()を呼べば復元できるため）
    JSON SaveToJson() const;

    /// @brief SaveToJsonで保存したJSONから状態を復元する
    /// @details 復元前に登録済みタイル・グリッド内容（固定タイル・生成開始座標含む）はすべて破棄される
    /// @return 復元に成功した場合はtrue（グリッドサイズが未指定などの必須キー欠如時はfalse）
    bool LoadFromJson(const JSON &json);

private:
    /// @brief グリッド上の1マス分の状態
    struct Cell final {
        /// @brief 固定されているタイル名（未固定の場合はstd::nullopt）
        std::optional<std::string> fixedTileName;
        /// @brief Solve()によって確定したタイル名（未確定の場合はstd::nullopt）
        std::optional<std::string> resolvedTileName;
    };

    bool IsInBounds(std::uint32_t x, std::uint32_t y, std::uint32_t z) const noexcept;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t depth_ = 0;
    /// @brief タイルを格納するための3次元配列（[x][y][z]の順で参照する）
    std::vector<std::vector<std::vector<Cell>>> grid_;

    std::uint32_t seed_ = 0;
    std::mt19937 randomEngine_{ seed_ };

    /// @brief 公開API経由で登録されたタイル群（キーはタイル名）
    std::unordered_map<std::string, Tile> tiles_;

    std::optional<std::array<std::uint32_t, 3>> startPosition_;
};

} // namespace KashipanEngine
