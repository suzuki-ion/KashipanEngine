#pragma once
#include <functional>
#include <string>
#include <unordered_map>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class SceneManager;

/// @brief シーン切り替え時、描画先リソース（ScreenBuffer/Window等）を
///        次のシーンの同名コンポーネントへ引き継ぐための一時預かり所
/// @details 旧シーンのコンポーネントはFinalizeで実際には破棄せずDepositし、
///          新シーンの同名コンポーネントはLoadFromJsonでClaimする。
///          種別（Kind）ごとに独立したプールを持つため、異なる種類の
///          リソース同士が誤って引き継がれることはない。
///          SceneManagerが管理するシーン切り替え中のみ引き継ぎが成立するようにしており
///          （BeginSceneSwitch〜EndSceneSwitchの間だけDepositが有効）、
///          エディターのPlay/Stopや手動でのシーンクリア等、切り替え以外の経路で
///          Finalizeが呼ばれた場合は従来通り即座に破棄される（引き継ぎプールへの
///          預けっぱなしによるリソースリークを防ぐため）。
class RenderTargetCarryOverRegistry {
public:
    /// @brief 引き継ぎプールの種別（異なる種別同士では引き継ぎが発生しない）
    enum class Kind {
        ScreenBuffer,
        NormalWindow,
        OverlayWindow,
    };

    /// @brief シーン切り替えの開始を通知する（SceneManager専用）
    /// @details 呼び出し後、Depositが実際にプールへ預けるようになる
    static void BeginSceneSwitch(Passkey<SceneManager>);
    /// @brief シーン切り替えの終了を通知する（SceneManager専用）
    /// @details 引き取られずプールに残っている全リソースをこの時点で実際に破棄する
    static void EndSceneSwitch(Passkey<SceneManager>);
    /// @brief シーン切り替え中かどうか
    static bool IsSceneSwitchInProgress();

    /// @brief リソースを次のシーンへの引き継ぎ候補として預ける
    /// @details シーン切り替え中でない場合は引き継ぎを行わず、その場でdestroyFnを呼んで破棄する
    /// @param kind プールの種別
    /// @param key 引き継ぎのキー（同じ種別・同じキーの場合のみ引き継がれる。空文字は無視される）
    /// @param resource 預けるリソースのポインタ
    /// @param destroyFn 引き継がれなかった場合に実際の破棄を行うために呼ばれる処理
    static void Deposit(Kind kind, const std::string &key, void *resource, std::function<void()> destroyFn);
    /// @brief 預けられたリソースを引き取る
    /// @return 引き取れた場合はリソースのポインタ、無い場合は nullptr
    static void *Claim(Kind kind, const std::string &key);

private:
    struct Entry {
        void *resource = nullptr;
        std::function<void()> destroyFn;
    };

    static size_t PoolIndex(Kind kind) { return static_cast<size_t>(kind); }

    static inline bool sSceneSwitchInProgress_ = false;
    static inline std::unordered_map<std::string, Entry> sPools_[3];
};

} // namespace KashipanEngine
