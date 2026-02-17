#include "Scenes/Components/Tutorial/MoveTutorial.h"

namespace KashipanEngine {

    void MovementTutorial::Initialize() {
        InitializeInternal();
        currentPhase_ = Phase::Initial;
        moveCount_ = 0;
    }

    void MovementTutorial::Update() {
        if (!IsActive()) return;

        switch (currentPhase_) {
        case Phase::Initial:
            // モニターに説明を表示
            StartMonitorText();
            // ここでBackMonitorに「WASDキーで移動できます」などのテキストを設定
            currentPhase_ = Phase::ShowInstruction;
            break;

        case Phase::ShowInstruction:
            // ユーザーがボタンを押すまで待つ
            if (IsSubmit()) {
                // ステージに視点を向ける
                StartTutorial();
                currentPhase_ = Phase::WaitForInput;
            }
            break;

        case Phase::WaitForInput:
            // カメラがステージに向いたら練習開始
            if (IsLookAtStage()) {
                currentPhase_ = Phase::Practicing;
            }
            break;

        case Phase::Practicing:
            if (moveCount_ >= requiredMoves_) {
                // チュートリアル完了
                StartMonitorText(); // モニターに戻って完了メッセージ表示
                currentPhase_ = Phase::Completed;
            }
            break;

        case Phase::Completed:
            if (IsSubmit()) {
                QuitTutorial(); // チュートリアル終了
            }
            break;
        }
    }

} // namespace KashipanEngine