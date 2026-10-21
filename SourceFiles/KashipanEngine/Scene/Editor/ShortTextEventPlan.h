#pragma once
#ifdef USE_IMGUI
#include <memory>
#include <string>
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Editor/ShortTextComponentPlan.h"
#include "Utilities/ShortTextEventRequest.h"

namespace KashipanEngine {

/// @brief シーンのイベント定義をまとめて置き換える Undo 可能なコマンド（必要なら GameEventRouter を追加する）
/// @details before / after は GameEvents::EncodeDefinitions 形式。追加した GameEventRouter は Undo で取り除く。
class GameEventDefinitionsCommand final : public IEditorCommand {
public:
    GameEventDefinitionsCommand(JSON before, JSON after, bool addRouter)
        : before_(std::move(before)), after_(std::move(after)), addRouter_(addRouter) {}
    bool Execute(SceneEditorContext *context) override;
    bool Undo(SceneEditorContext *context) override;
    std::string GetName() const override { return "イベント定義の変更"; }
private:
    JSON before_;
    JSON after_;
    bool addRouter_ = false;
    bool added_ = false;
};

/// @brief 短文のイベント・ゲーム部品の要求から、実際のコンポーネント追加・設定とイベント定義を1つの Undo 操作として作る
/// @details 計画（plan）とコマンドを作るだけではシーンを変更しない。名前・タグの重複、未定義イベント、引数の型の不一致、
///          手作りの部品との衝突は理由を示して nullptr を返す（何も変更しない）。
///          短文由来の部品・受信処理・定義（shortTextManaged）だけを更新し、手作りのものは削除・上書きしない。
std::unique_ptr<IEditorCommand> MakeShortTextEventCommand(SceneEditorContext *context, EmptyObject *target,
    const ShortText::EventRequest &request, const std::string &sourceText, ShortTextComponentPlan *plan = nullptr);

} // namespace KashipanEngine
#endif
