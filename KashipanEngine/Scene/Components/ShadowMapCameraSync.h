#pragma once

#include "Scene/Components/ISceneComponent.h"

namespace KashipanEngine {

class Camera3D;
class DirectionalLight;
class ShadowMapBinder;

/// @brief メインカメラの視錐台をカバーするようにシャドウマップ用カメラ（正射影）を同期する
class ShadowMapCameraSync final : public ISceneComponent {
public:
    /// @brief コンポーネント生成
    ShadowMapCameraSync();
    ~ShadowMapCameraSync() override = default;

    /// @brief 同期元となるメインカメラを設定
    void SetMainCamera(Camera3D* camera) { mainCamera_ = camera; }
    /// @brief 同期先となるシャドウマップ用ライトカメラを設定
    void SetLightCamera(Camera3D* camera) { lightCamera_ = camera; }
    /// @brief 方向ライトを設定（ライト方向からライトカメラの向きを決定）
    void SetDirectionalLight(DirectionalLight* light) { light_ = light; }
    /// @brief ShadowMapBinder を設定（ライト VP 行列をシェーダへ反映する）
    void SetShadowMapBinder(ShadowMapBinder* binder) { shadowMapBinder_ = binder; }

    /// @brief ライトカメラをメインカメラ中心からライト逆方向へ離す距離を設定
    void SetDistanceFromTarget(float distance) { distanceFromTarget_ = distance; }
    /// @brief ライトカメラの Near/Far に追加するマージン値を設定
    void SetDepthMargin(float margin) { depthMargin_ = margin; }

    /// @brief 毎フレーム同期処理を実行
    void Update() override;

private:
    void SyncOnce();

    Camera3D* mainCamera_ = nullptr;
    Camera3D* lightCamera_ = nullptr;
    DirectionalLight* light_ = nullptr;
    ShadowMapBinder* shadowMapBinder_ = nullptr;

    float distanceFromTarget_ = 10.0f;
    float depthMargin_ = 10.0f;
};

} // namespace KashipanEngine
