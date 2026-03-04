#pragma once
#include <KashipanEngine.h>

#include <vector>

#include <Objects/Puzzle/PuzzleBoard.h>
#include <Objects/Puzzle/PuzzleCursor.h>
#include <Objects/Puzzle/PuzzleGoal.h>
#include <Config/PuzzleGameConfig.h>

namespace KashipanEngine {

class TestScene final : public SceneBase {
public:
    TestScene();
    ~TestScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;
};

} // namespace KashipanEngine