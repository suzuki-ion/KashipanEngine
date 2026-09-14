#pragma once
#include <atomic>

#include "Utilities/UUID128.h"

namespace KashipanEngine {

class EmptyObject;

/// @brief AngelScriptへ公開する Object@ ハンドルの実体（参照カウント式の参照型）
/// @details 対象EmptyObjectの生ポインタを直接持たず、UUID128だけを保持する。
///          メソッド呼び出しの都度 Resolve() で ScriptExecutionScope::GetCurrentSceneContext()
///          経由の SceneContext::GetSceneObject(UUID) により生存中のオブジェクトへ解決し直す。
///          参照先が既に削除されている場合 Resolve() は nullptr を返すため、各バインディング側は
///          ThrowDestroyedObjectException() でAngelScriptの例外として処理できる。これにより
///          解放済みメモリへ直接アクセスすることがなくなり、参照先削除によるエラーは
///          そのスクリプトの実行時エラー（asEXECUTION_EXCEPTION）に留まり、エンジン全体は落ちない。
/// @details ハンドル自体（このインスタンス）の生存期間は参照先EmptyObjectの生存とは独立している。
///          [SerializeField] Object@ 等でフレームをまたいで保持されても、ハンドルは常に有効であり、
///          Resolve()を呼んだ時点で初めて参照先の生死が判定される。
class ScriptObjectHandle final {
public:
    void AddRef() { refCount_.fetch_add(1, std::memory_order_relaxed); }
    void Release() {
        if (refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }

    /// @brief 参照先オブジェクトのUUID（削除済みかどうかに関わらず、常にそのまま返す）
    const UUID128 &GetID() const noexcept { return id_; }

    /// @brief 現在生存している参照先オブジェクトへ解決する（削除済み、またはシーン外の場合はnullptr）
    EmptyObject *Resolve() const;

    /// @brief 対象オブジェクトからハンドルを作成する（objがnullptrの場合はnullptrを返す）
    static ScriptObjectHandle *Create(EmptyObject *obj);
    /// @brief UUIDから直接ハンドルを作成する（JSON読込等、参照先がまだ解決できない場合に使う）
    static ScriptObjectHandle *CreateFromID(const UUID128 &id);

private:
    explicit ScriptObjectHandle(const UUID128 &id) : id_(id) {}
    ~ScriptObjectHandle() = default;

    UUID128 id_;
    std::atomic<int> refCount_{ 1 };
};

/// @brief 現在実行中のAngelScriptコンテキストへ「参照先オブジェクトは既に削除されている」例外を投げる
/// @details asGetActiveContextで取得したコンテキストへSetExceptionする。context_->Execute()が
///          asEXECUTION_EXCEPTIONを返すようになり、ScriptComponent側の既存の例外処理
///          （lastError_への格納・ログ出力）でそのスクリプトの実行だけが安全に止まる
void ThrowDestroyedObjectException();

} // namespace KashipanEngine
