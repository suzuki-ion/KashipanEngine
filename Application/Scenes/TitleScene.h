#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class TitleScene final : public SceneBase {
public:
    TitleScene();
    ~TitleScene() override;

protected:
    void OnUpdate() override;

private:
    // 画面表示用
    ScreenBuffer *screenBuffer_ = nullptr;
    Camera2D *screenCamera2D_ = nullptr;
    Sprite *screenSprite_ = nullptr;

    // メインカメラ
    Camera3D *mainCamera3D_ = nullptr;

    // プレイヤー
    Object3DBase *player_;

    // BGM用プレイハンドル
    AudioManager::PlayHandle bgmPlay_ = AudioManager::kInvalidPlayHandle;
};

} // namespace KashipanEngine