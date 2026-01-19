#pragma once

#include "Objects/IObjectComponent.h"
#include "Objects/ObjectContext.h"

#include "Math/Vector3.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Utilities/TimeUtils.h"

#include <algorithm>
#include <memory>

namespace KashipanEngine {

class AlwaysRotate final : public IObjectComponent3D {
public:
    /// @brief 回転速度（ラジアン/秒）を指定してコンポーネントを作成する
    /// @param rotateSpeedRadPerSec 回転速度ベクトル（rad/sec）
    explicit AlwaysRotate(const Vector3 &rotateSpeedRadPerSec = Vector3{0.0f, 1.0f, 0.0f})
        : IObjectComponent3D("AlwaysRotate", 1), rotateSpeed_(rotateSpeedRadPerSec) {
    }

    /// @brief デストラクタ
    ~AlwaysRotate() override = default;

    /// @brief コンポーネントの複製を作成して返す
    /// @return 複製されたコンポーネントの所有ポインタ
    std::unique_ptr<IObjectComponent> Clone() const override {
        return std::make_unique<AlwaysRotate>(rotateSpeed_);
    }

    /// @brief 毎フレームの更新処理。回転を加算する
    /// @return 更新が成功した場合は true
    std::optional<bool> Update() override {
        Transform3D *tr = GetOwner3DContext()->GetComponent<Transform3D>();
        if (!tr) return false;

        const float dt = std::max(0.0f, GetDeltaTime());
        Vector3 r = tr->GetRotate();
        r += rotateSpeed_ * dt;
        tr->SetRotate(r);
        return true;
    }

    /// @brief 回転速度を設定する
    /// @param v 回転速度ベクトル（rad/sec）
    void SetRotateSpeed(const Vector3 &v) { rotateSpeed_ = v; }
    /// @brief 回転速度を取得する
    /// @return 回転速度ベクトル（rad/sec）
    const Vector3 &GetRotateSpeed() const { return rotateSpeed_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Vector3 rotateSpeed_{0.0f, 1.0f, 0.0f};
};

} // namespace KashipanEngine
