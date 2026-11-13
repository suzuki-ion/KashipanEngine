#pragma once
#ifdef USE_IMGUI
#include <memory>
#include <string>
#include <vector>
#include "Utilities/ShortTextBehavior.h"
#include "Utilities/ShortTextPropertyRequest.h"

namespace KashipanEngine {
class EmptyObject;
class IEditorCommand;
class CompositeCommand;

// 適用前に表示する編集時の構成計画。計画やコマンドの構築だけではシーンを変更しない。
struct ShortTextComponentPlan {
    bool success = false;
    std::string message;
    std::vector<std::string> steps;        // 実際に追加・設定・更新・削除する内容
    std::vector<std::string> requirements; // 自動では作らない、利用者が設定すべき内容
};

// エンジンの登録済みコンポーネントとメンバー。対象の既存インスタンスを優先する。
ShortText::PropertySchema MakeShortTextPropertySchema(const EmptyObject *target);
ShortText::PropertyParseResult ParseShortTextPropertyRequest(const std::string &text, const EmptyObject *target);

// Setは編集時に実際のコンポーネントを編集し、PingPongはPropertyMotionを追加または更新する。
// Tweenはメンバーに結び付けたKeyframeAnimationを内包するKeyFrameAnimatorエントリーを追加または更新する。
// 同じメンバー・チャンネルを駆動する短文由来の処理（PropertyMotion・キーフレーム項目・従来の動作）を置き換える。
// 不足する対象コンポーネントは既定値で追加し、全体をUndo可能な1つのCompositeCommandにまとめる。
// イベント駆動クリップ用のTween設定。自動再生なし、指定エントリー名、確定後のエントリー名を扱う。
// 更新対象の短文由来エントリーは既存の名前を維持する。
struct ShortTextTweenOptions {
    bool playOnStart = true;
    std::string entryName;
    std::string *appliedName = nullptr;
};
std::unique_ptr<IEditorCommand> MakeShortTextPropertyCommand(EmptyObject *target, const ShortText::PropertyRequest &request,
    const std::string &sourceText, ShortTextComponentPlan *plan = nullptr, const ShortTextTweenOptions *tween = nullptr);

// 選択オブジェクト上で従来のレシピが必要とするコンポーネント。アセット不要のものだけを追加し、
// 形状不明のコライダー・音声・テキスト・物理ボディ・別の対象は報告のみ行う。
struct ShortTextRecipeNeeds {
    std::vector<std::string> add;
    std::vector<std::string> notes;
};
ShortTextRecipeNeeds AnalyzeShortTextRecipeNeeds(const EmptyObject *target, const ShortText::Recipe &recipe);
// 従来の単純なレシピと同じTransformのメンバー・軸を駆動する、短文由来のPropertyMotion。
// 短文によって1つのプロパティが二重に駆動されないよう、Undo可能な方法で置き換える。
std::vector<class PropertyMotion *> FindConflictingShortTextMotions(const EmptyObject *target, const ShortText::Recipe &recipe);
ShortTextComponentPlan PlanShortTextBehavior(const EmptyObject *target, const ShortText::Recipe &recipe);
// 従来の単純なレシピと同じTransformのメンバー・軸を駆動する、短文由来のKeyFrameAnimatorエントリー。
// group指定時はUndo可能な削除を追加し、計画には常に手順を追加する。
void AppendShortTextKeyframeConflicts(CompositeCommand *group, const EmptyObject *target, const ShortText::Recipe &recipe, ShortTextComponentPlan &plan);
} // namespace KashipanEngine
#endif
