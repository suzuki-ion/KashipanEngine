#pragma once
#include "Scenes/Components/Tutorial/TutorialBase.h"

namespace KashipanEngine {

    // 移動チュートリアルの例
    class MovementTutorial : public TutorialBase {
    public:
        MovementTutorial(InputCommand* inputCommand,
            const Vector3& gameTargetPos,
            const Vector3& gameTargetRot,
            const Vector3& menuTargetPos,
            const Vector3& menuTargetRot)
            : TutorialBase(inputCommand, gameTargetPos, gameTargetRot, menuTargetPos, menuTargetRot) {}

        ~MovementTutorial() override = default;

        void Initialize() override;
        void Update() override;

    private:
        enum class Phase {
            Initial,          // 初期状態
            ShowInstruction,  // 説明を表示
            WaitForInput,     // 入力待ち
            Practicing,       // 実践中
            Completed         // 完了
        };

        Phase currentPhase_ = Phase::Initial;
        int moveCount_ = 0;           // 移動回数カウント
        const int requiredMoves_ = 4; // 必要な移動回数
    };

} // namespace KashipanEngine