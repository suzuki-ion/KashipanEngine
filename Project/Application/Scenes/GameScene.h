#pragma once
#include <KashipanEngine.h>

#include <Objects/Container/BlockContainer.h>
#include <Objects/Container/BlockSpriteContainer.h>
#include <Objects/Container/HandBlockContainer.h>
#include <Objects/Container/HandBlockSpriteContainer.h>

#include <Objects/GameSystem/BlockScroller.h>
#include <Objects/GameSystem/BlockFaller.h>
#include <Objects/GameSystem/Thermometer.h>
#include <Objects/GameSystem/MatchResolver.h>
#include <Objects/GameSystem/Cursor.h>

namespace KashipanEngine {

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
};

} // namespace KashipanEngine