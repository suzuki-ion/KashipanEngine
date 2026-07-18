#pragma once
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace KashipanEngine {

/// @brief ステージを構成する部屋の意味的な種別
enum class RoomType : std::uint8_t {
    Start = 0,
    Goal,
    /// @brief メインルート上の通常の部屋
    Normal,
    /// @brief メインルートから外れた行き止まりの部屋（種別未指定の寄り道）
    Branch,
    /// @brief 建物の入り口として使う部屋
    Building,
    /// @brief 奥行方向のギミック用の部屋
    GimmickDepth,
    /// @brief 宝箱等の収集アイテムを配置する部屋
    Treasure,
};

/// @brief 部屋グラフの1ノード分の情報
struct RoomNode final {
    std::uint32_t id = 0;
    RoomType type = RoomType::Normal;
    /// @brief 抽象部屋グリッド上の座標（タイル座標ではない）
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t z = 0;
    /// @brief 接続している部屋IDの一覧（双方向）
    std::vector<std::uint32_t> connectedRoomIDs;
};

/// @brief スタートからゴールまで必ず繋がるメインルートと、そこから枝分かれする
///        行き止まりの部屋（建物入口・奥行ギミック・収集アイテム等）を持つ、
///        抽象的な「部屋グラフ」をランダム生成するユーティリティクラス
/// @details 生成される座標はあくまで抽象的な部屋グリッド上のもので、実際のタイル
///          グリッドへの展開（WaveFunctionCollapseへの固定タイル書き込み等）は
///          StageGridBuilderが担当する
class StageGraphGenerator final {
public:
    StageGraphGenerator() = default;
    ~StageGraphGenerator() = default;

    /// @brief 乱数生成に使うシード値を設定する
    void SetSeed(std::uint32_t seed);
    std::uint32_t GetSeed() const noexcept { return seed_; }

    /// @brief 抽象部屋グリッドのサイズを設定する
    /// @param width メインルートの進行方向（横スクロールの奥行ではなく横方向）のスロット数
    /// @param height 上下方向（ジャンプ等の縦の寄り道）のスロット数
    /// @param depth 奥行方向（建物・奥行ギミック用の寄り道）のスロット数
    void SetGridSize(std::uint32_t width, std::uint32_t height, std::uint32_t depth);
    std::uint32_t GetGridWidth() const noexcept { return width_; }
    std::uint32_t GetGridHeight() const noexcept { return height_; }
    std::uint32_t GetGridDepth() const noexcept { return depth_; }

    /// @brief メインルートの各部屋から、寄り道の部屋（行き止まり）が生成される確率（0.0～1.0）
    void SetBranchProbability(float probability);
    float GetBranchProbability() const noexcept { return branchProbability_; }

    /// @brief 寄り道部屋の種別として使う候補を重み付きで登録する
    /// @details 未登録の場合、寄り道部屋は全てRoomType::Branchになる
    /// @return 登録できた場合はtrue（重みが0以下の場合はfalse）
    bool AddSideRoomType(RoomType type, float weight);

    /// @brief 登録済みの寄り道部屋種別候補をすべて削除する
    void ClearSideRoomTypes();

    /// @brief 部屋グラフを生成する（既存の生成結果は破棄される）
    /// @details グリッドサイズが幅2未満、または高さ・奥行が0の場合は何も生成しない
    void Generate();

    std::size_t GetRoomCount() const noexcept { return rooms_.size(); }

    /// @brief 登録順（生成順）でi番目の部屋を取得する
    /// @return インデックスが範囲外の場合はnullptr
    const RoomNode *GetRoomByIndex(std::size_t index) const;

    /// @brief 部屋IDから部屋を取得する
    /// @return 該当する部屋が無い場合はnullptr
    const RoomNode *GetRoom(std::uint32_t roomID) const;

    const std::vector<RoomNode> &GetRooms() const noexcept { return rooms_; }

    std::optional<std::uint32_t> GetStartRoomID() const noexcept { return startRoomID_; }
    std::optional<std::uint32_t> GetGoalRoomID() const noexcept { return goalRoomID_; }

private:
    RoomType PickSideRoomType();
    std::uint32_t AddRoom(RoomType type, std::uint32_t x, std::uint32_t y, std::uint32_t z);
    void Connect(std::uint32_t roomA, std::uint32_t roomB);
    bool IsOccupied(std::uint32_t x, std::uint32_t y, std::uint32_t z) const;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t depth_ = 0;

    std::uint32_t seed_ = 0;
    std::mt19937 randomEngine_{ seed_ };

    float branchProbability_ = 0.3f;

    struct SideRoomTypeEntry final {
        RoomType type;
        float weight;
    };
    std::vector<SideRoomTypeEntry> sideRoomTypes_;

    std::vector<RoomNode> rooms_;
    /// @brief 抽象部屋グリッド上で既に部屋が置かれているかどうか（[x][y][z]）
    std::vector<std::vector<std::vector<bool>>> occupied_;

    std::optional<std::uint32_t> startRoomID_;
    std::optional<std::uint32_t> goalRoomID_;
};

} // namespace KashipanEngine
