#pragma once
#include <KashipanEngine.h>
#include <array>

namespace KashipanEngine {

class Camera3D;
class Object3DBase;
class Billboard;

class JustAvoidParticle final : public ISceneComponent {
public:
    /// @brief ジャスト回避のパーティクルを生成するコンポーネントを作成する
    /// @param screenBuffer 描画先のスクリーンバッファ
    /// @param camera3D カメラ参照（向き計算用）
    /// @param mover 親オブジェクト
    JustAvoidParticle(ScreenBuffer* screenBuffer, Camera3D* camera3D, Object3DBase* mover)
        : ISceneComponent("JustAvoidParticle", 1)
        , screenBuffer_(screenBuffer)
        , camera3D_(camera3D)
        , mover_(mover) {}

    /// @brief デストラクタ
    ~JustAvoidParticle() override = default;

    /// @brief 初期化処理
    void Initialize() override;
    /// @brief 毎フレーム更新処理
    void Update() override;

    /// @brief パーティクルをスポーンさせる
    /// @param pos 生成位置
    /// @param dir 進行方向
    void Spawn(const Vector3& pos, const Vector3& dir);

private:
    ScreenBuffer* screenBuffer_ = nullptr;
    Camera3D* camera3D_ = nullptr;
    Object3DBase* mover_ = nullptr;

    std::array<Billboard*, 16> billboards_{};
    Billboard* textBillboard_ = nullptr;
};

} // namespace KashipanEngine
