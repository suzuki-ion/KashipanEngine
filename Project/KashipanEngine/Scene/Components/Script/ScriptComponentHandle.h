#pragma once
#include <atomic>
#include <type_traits>

#include "Debug/Logger.h"
#include "Objects/ComponentRef.h"
#include "Objects/IObjectComponent.h"
#include "Scene/Components/Script/ScriptBindings.h"
#include "Scene/Components/Script/ScriptObjectHandle.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine {

/// @brief AngelScriptへ公開するコンポーネントハンドルの実体（参照カウント式の参照型）
/// @details 対象コンポーネントの生ポインタを直接持たず、ComponentRef（所有オブジェクトのUUID＋
///          プール内での追加順ID。削除されても再利用されない）だけを保持する。メソッド呼び出しの
///          都度Resolve()で SceneContext::ResolveComponent() により生存中のコンポーネントへ
///          解決し直す。所有オブジェクトごと削除された場合／コンポーネント単体が削除された場合の
///          いずれもResolve()はnullptrを返すため、ScriptObjectHandleと同様に「参照先削除エラーは
///          そのスクリプトの実行時エラーに留まり、エンジン全体は落ちない」という性質を持つ。
/// @details addedIDはプールのスロット再利用時にも重複しないため、生ポインタのアドレス一致判定と違い
///          「削除された別のコンポーネントが同じアドレスに再配置された」誤判定（ABA問題）が起きない。
template <typename T>
class ScriptComponentHandle final {
public:
    explicit ScriptComponentHandle(const ComponentRef &ref) : ref_(ref) {}

    void AddRef() { refCount_.fetch_add(1, std::memory_order_relaxed); }
    void Release() {
        LogScope scope;
        if (refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this;
    }

    /// @brief 現在生存している参照先コンポーネントへ解決する（削除済み、またはシーン外の場合はnullptr）
    T *Resolve() const {
        LogScope scope;
        SceneContext *sceneContext = ScriptExecutionScope::GetCurrentSceneContext();
        if (!sceneContext) return nullptr;
        IObjectComponent *component = sceneContext->ResolveComponent(ref_);
        // addedIDが一意である限り、解決できたコンポーネントは常にCreate()時と同じ実体・同じ型なので
        // 型情報を伴わないstatic_castで十分（dynamic_castのRTTIコストを避ける）
        return static_cast<T *>(component);
    }

    /// @brief 対象コンポーネントからハンドルを作成する（componentがnullptrの場合はnullptrを返す）
    static ScriptComponentHandle<T> *Create(T *component) {
        LogScope scope;
        if (!component) return nullptr;
        return new ScriptComponentHandle<T>(component->GetComponentRef());
    }

private:
    ~ScriptComponentHandle() = default;

    ComponentRef ref_;
    std::atomic<int> refCount_{ 1 };
};

/// @brief SafeCall内部用：Resolve失敗時に返す安全なデフォルト値を作る
/// @details voidはreturn;のみ、参照返しの型はプログラム終了まで生存するstatic const実体への
///          参照を返す（ダングリング参照を作らないため）。それ以外は値初期化した一時オブジェクトを返す
template <typename Ret>
Ret SafeCallDefault() {
    LogScope scope;
    if constexpr (std::is_reference_v<Ret>) {
        static const std::remove_reference_t<Ret> kDefault{};
        return kDefault;
    } else {
        return Ret{};
    }
}

/// @brief SafeCall<&T::Method>()の実装本体
/// @details メンバ関数ポインタの「型」ごとにMake静的メンバテンプレートを用意し、呼び出し側では
///          MemberFunctionTraits<decltype(Method)>::template Make<Method>() が返す非キャプチャの
///          ラムダをそのまま.method()へ渡す。Methodはこのラムダを生成する関数テンプレートの
///          非型テンプレート引数であり、ラムダ本体から参照してもキャプチャは発生しない
///          （テンプレート引数はローカル変数ではなくコンパイル時定数として扱われるため）。
///          そのためasbind20の.method()が要求する「非キャプチャの呼び出し可能オブジェクト」を
///          満たせる（もしMethodをラムダのキャプチャで保持する設計にすると、asbind20が
///          キャプチャ付き呼び出し可能オブジェクトを受け付けずコンパイルエラーになる）。
///          型（decltype(Method)）でパターンマッチする通常の部分特殊化のため、
///          値（Method自体）を非型テンプレート引数として部分特殊化する場合に生じる
///          const/noexcept修飾の組み合わせによる曖昧性も避けられる
template <typename MethodType>
struct MemberFunctionTraits;

template <typename T, typename Ret, typename... Args>
struct MemberFunctionTraits<Ret (T::*)(Args...)> {
    template <auto Method>
    static constexpr auto Make() {
        LogScope scope;
        return [](ScriptComponentHandle<T> &self, Args... args) -> Ret {
            T *obj = self.Resolve();
            if (!obj) {
                ThrowDestroyedObjectException();
                if constexpr (!std::is_void_v<Ret>) return SafeCallDefault<Ret>();
                else return;
            }
            return (obj->*Method)(args...);
        };
    }
};

template <typename T, typename Ret, typename... Args>
struct MemberFunctionTraits<Ret (T::*)(Args...) const> {
    template <auto Method>
    static constexpr auto Make() {
        LogScope scope;
        return [](const ScriptComponentHandle<T> &self, Args... args) -> Ret {
            T *obj = self.Resolve();
            if (!obj) {
                ThrowDestroyedObjectException();
                if constexpr (!std::is_void_v<Ret>) return SafeCallDefault<Ret>();
                else return;
            }
            return (obj->*Method)(args...);
        };
    }
};

template <typename T, typename Ret, typename... Args>
struct MemberFunctionTraits<Ret (T::*)(Args...) noexcept> {
    template <auto Method>
    static constexpr auto Make() {
        LogScope scope;
        return [](ScriptComponentHandle<T> &self, Args... args) -> Ret {
            T *obj = self.Resolve();
            if (!obj) {
                ThrowDestroyedObjectException();
                if constexpr (!std::is_void_v<Ret>) return SafeCallDefault<Ret>();
                else return;
            }
            return (obj->*Method)(args...);
        };
    }
};

template <typename T, typename Ret, typename... Args>
struct MemberFunctionTraits<Ret (T::*)(Args...) const noexcept> {
    template <auto Method>
    static constexpr auto Make() {
        LogScope scope;
        return [](const ScriptComponentHandle<T> &self, Args... args) -> Ret {
            T *obj = self.Resolve();
            if (!obj) {
                ThrowDestroyedObjectException();
                if constexpr (!std::is_void_v<Ret>) return SafeCallDefault<Ret>();
                else return;
            }
            return (obj->*Method)(args...);
        };
    }
};

/// @brief メンバ関数をラップし、呼び出し時にハンドルを解決してから転送する非キャプチャラムダを作る
/// @details 参照先が削除済みの場合はThrowDestroyedObjectException()で例外化し、戻り値型に応じた
///          安全なデフォルト値（SafeCallDefault参照）を返す。使い方: SafeCall<&T::Method>()
template <auto Method>
constexpr auto SafeCall() {
    LogScope scope;
    return MemberFunctionTraits<decltype(Method)>::template Make<Method>();
}

} // namespace KashipanEngine
