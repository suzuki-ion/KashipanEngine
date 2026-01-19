#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/ObjectContext.h"

#include "Math/Vector3.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Utilities/TimeUtils.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace KashipanEngine {

// レーン（CatmullRom 曲線）に沿って、指定時間でオブジェクトを移動させるコンポーネント
class RailMovement final : public IObjectComponent3D {
public:
    static const std::string &GetStaticComponentType() {
        static const std::string type = "RailMovement";
        return type;
    }

    /// @brief 制御点列と移動時間からコンポーネントを作成する
    /// @param points Catmull-Rom の制御点列（4点以上推奨）
    /// @param durationSec 始点から終点までの時間（秒）
    explicit RailMovement(std::vector<Vector3> points = {}, float durationSec = 1.0f)
        : IObjectComponent3D(GetStaticComponentType(), 1), points_(std::move(points)), durationSec_(durationSec) {
    }

    /// @brief デストラクタ
    ~RailMovement() override = default;

    /// @brief コンポーネントを複製して返す
    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<RailMovement>(points_, durationSec_);
        ptr->elapsedSec_ = elapsedSec_;
        return ptr;
    }

    /// @brief 毎フレームの更新。レール上での位置を計算して設定する
    /// @return 更新成功時は true
    std::optional<bool> Update() override {
        auto *ctx = GetOwner3DContext();
        if (!ctx) return false;
        auto *tr = ctx->GetComponent<Transform3D>();
        if (!tr) return false;

        const float dt = std::max(0.0f, GetDeltaTime());
        elapsedSec_ += dt;

        // 指定時間で 0.0f -> 1.0f を進める
        const float denom = (durationSec_ > 0.0f) ? durationSec_ : 0.0001f;
        float t = elapsedSec_ / denom;
        t = std::clamp(t, 0.0f, 1.0f);

        // レーン上の座標を求めて移動
        const Vector3 pos = Vector3::CatmullRomPosition(points_, t, false);
        tr->SetTranslate(pos);

        return true;
    }

    /// @brief レーンの制御点を設定する
    /// @param points 制御点列
    void SetPoints(std::vector<Vector3> points) { points_ = std::move(points); }
    /// @brief レーンの制御点列を取得する
    const std::vector<Vector3> &GetPoints() const { return points_; }

    /// @brief 移動にかかる時間（秒）を設定する
    /// @param s 秒数
    void SetDurationSec(float s) { durationSec_ = s; }
    /// @brief 移動にかかる時間（秒）を取得する
    float GetDurationSec() const { return durationSec_; }

    /// @brief 経過時間（秒）を設定する（リセット等に使用）
    /// @param s 秒数
    void SetElapsedSec(float s) { elapsedSec_ = s; }
    /// @brief 経過時間（秒）を取得する
    float GetElapsedSec() const { return elapsedSec_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    std::vector<Vector3> points_;
    float durationSec_ = 1.0f;
    float elapsedSec_ = 0.0f;
};

} // namespace KashipanEngine
