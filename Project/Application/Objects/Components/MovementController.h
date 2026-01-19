#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <functional>

namespace KashipanEngine {

class MovementController final : public IObjectComponent3D {
public:
    using EasingFunc = std::function<Vector3(Vector3, Vector3, float)>;

    struct MoveEntry {
        Vector3 from;
        Vector3 to;
        float duration = 1.0f;
        EasingFunc easing{}; // 未指定の場合は Lerp を使用
    };

    /// @brief 移動エントリ列を指定してコンポーネントを作成する
    /// @param moves 移動エントリの配列
    explicit MovementController(std::vector<MoveEntry> moves = {})
        : IObjectComponent3D("MovementController", 1), moves_(std::move(moves)) {}

    /// @brief コンポーネントの複製を作成して返す
    /// @return 新しいコンポーネントの所有ポインタ
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<MovementController>(moves_);
        ptr->currentIndex_ = currentIndex_;
        ptr->elapsed_ = elapsed_;
        ptr->isMoving_ = isMoving_;
        return ptr;
    }

    /// @brief 毎フレームの更新処理。現在の移動を進める
    /// @return 処理に成功した場合は true
    std::optional<bool> Update() override {
        if (moves_.empty()) return true;
        if (!isMoving_) return true;

        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;
        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return false;

        const float dt = std::max(0.0f, GetDeltaTime());
        elapsed_ += dt;
        previousIndex_ = currentIndex_;

        if (currentIndex_ >= moves_.size()) {
            isMoving_ = false;
            currentIndex_ = 0;
            elapsed_ = 0.0f;
            return true;
        }

        const MoveEntry &entry = moves_[currentIndex_];
        const float t = std::min(1.0f, entry.duration > 0.0f ? (elapsed_ / entry.duration) : 1.0f);

        Vector3 pos{};
        if (entry.easing) {
            pos = entry.easing(entry.from, entry.to, t);
        } else {
            pos = Lerp(entry.from, entry.to, t);
        }
        tr->SetTranslate(pos);

        if (t >= 1.0f) {
            currentIndex_++;
            elapsed_ = 0.0f;
            if (currentIndex_ >= moves_.size()) {
                isMoving_ = false;
                currentIndex_ = 0;
            }
        }

        return true;
    }

    /// @brief 移動シーケンスを開始する
    void Start() {
        if (moves_.empty()) return;
        isMoving_ = true;
        currentIndex_ = 0;
        elapsed_ = 0.0f;
    }
    /// @brief 移動を停止する
    void Stop() { isMoving_ = false; }

    /// @brief 現在移動中かどうかを返す
    /// @return 移動中であれば true
    bool IsMoving() const { return isMoving_; }

    /// @brief 移動エントリ列を設定する
    /// @param moves 新しい移動エントリ列
    void SetMoves(std::vector<MoveEntry> moves) {
        moves_ = std::move(moves);
        currentIndex_ = 0;
        elapsed_ = 0.0f;
        isMoving_ = false;
    }

    /// @brief 登録されている移動エントリ列を取得する
    /// @return 移動エントリ列への参照
    const std::vector<MoveEntry> &GetMoves() const { return moves_; }
    /// @brief 現在のエントリインデックスを取得する
    /// @return 現在のインデックス
    size_t GetCurrentIndex() const { return currentIndex_; }
    /// @brief 直前のエントリインデックスを取得する
    /// @return 直前のインデックス
    size_t GetPreviousIndex() const { return previousIndex_; }
    /// @brief 現在のエントリでの経過時間を取得する
    /// @return 経過時間（秒）
    float GetElapsedTime() const { return elapsed_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    std::vector<MoveEntry> moves_;
    size_t currentIndex_ = 0;
    size_t previousIndex_ = 0;
    float elapsed_ = 0.0f;
    bool isMoving_ = false;
};

} // namespace KashipanEngine