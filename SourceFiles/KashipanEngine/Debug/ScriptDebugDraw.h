#pragma once
#include <vector>

#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

/// @brief スクリプト（AngelScriptのDebug::DrawLine）から蓄積されるデバッグ線分バッファ
/// @details 実際の描画はSceneEditorViewが毎フレームGetLines()で取り出してエディターの
///          シーンビュー用デバッグ描画（EditorDebugDrawSettings）へ合流させる。そのためゲーム画面には
///          表示されず、エディターのシーンビューにのみ表示される。取り出し後は毎フレームClear()される
class ScriptDebugDraw {
public:
    struct Line {
        Vector3 start;
        Vector3 end;
        Vector4 color;
    };

    /// @brief 線分を1本追加する（AngelScriptバインディングから呼ばれる）
    static void AddLine(const Vector3 &start, const Vector3 &end, const Vector4 &color);

    /// @brief 蓄積中の線分一覧を取得する
    static const std::vector<Line> &GetLines();

    /// @brief 蓄積内容をクリアする（毎フレーム、描画側で取り出した後に呼ぶ）
    static void Clear();

private:
    static std::vector<Line> lines_;
};

} // namespace KashipanEngine
