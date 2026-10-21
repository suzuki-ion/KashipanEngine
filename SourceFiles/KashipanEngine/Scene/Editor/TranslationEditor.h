#pragma once
#ifdef USE_IMGUI

#include <string>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief アプリケーション側（プロジェクト固有）の翻訳キーを編集するパネル
/// @details 編集できるのはプロジェクト層のみで、エンジン共通（エンジンルート直下 Locales/）の
///          翻訳は参照専用として一覧するだけに留める。全プロジェクトで共有される文言を
///          特定のプロジェクトから壊せてしまわないようにするため。
///          エンジン側と同じキーをプロジェクト側で定義した場合は「上書き」として扱われ、
///          そのプロジェクトでだけ文言が差し替わる。
class TranslationEditor final {
public:
    TranslationEditor(Passkey<SceneEditor>) {}

    void ShowImGui();

private:
    /// @brief 編集対象の言語をUIで選ばせる
    void ShowLanguageSelector();
    /// @brief プロジェクト層の一覧と編集UI
    void ShowProjectTranslations();
    /// @brief 新規キーの追加UI
    void ShowAddRow();
    /// @brief エンジン共通の翻訳を参照するだけのUI
    void ShowGlobalTranslationReference();

    /// @brief 編集中の言語コード（空の場合は初回描画時に現在の言語で初期化する）
    std::string language_;
    /// @brief プロジェクト層の絞り込み文字列
    std::string filter_;
    /// @brief エンジン共通の翻訳の絞り込み文字列
    std::string globalFilter_;
    /// @brief 追加行の入力バッファ
    std::string newKeyBuffer_;
    std::string newTextBuffer_;
    /// @brief 保存していない変更があるか
    bool isDirty_ = false;
    /// @brief 直近の操作結果メッセージ（成功・失敗どちらも入る）
    std::string statusMessage_;
    /// @brief statusMessage_ がエラーかどうか（表示色の切り替えに使う）
    bool isStatusError_ = false;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
