#pragma once
#include "Utilities/UUID128.h"

namespace KashipanEngine {

/// @brief コンポーネントへの安全な参照ハンドル
/// @details コンポーネントの生ポインタをフレームをまたいで直接保持すると、プールのスロット再利用時に
///          別の（無関係な）コンポーネントを指してしまう危険がある。ownerObjectID（所有オブジェクトのUUID）と
///          addedID（EmptyObject内で追加順に振られ、削除されても再利用されないID）の組で対象を一意に識別し、
///          使う直前に毎回 SceneContext::ResolveComponent() 等で生ポインタへ解決する。
struct ComponentRef {
    // UUID128 のデフォルトコンストラクタが explicit なため、集約初期化の暗黙メンバ初期化
    // （ComponentRef{} 等での UUID128 の値初期化はコピーリスト初期化扱いとなり、explicit な
    // コンストラクタは候補から除外されコンパイルエラーになる）を避けるため明示的に定義する
    ComponentRef() = default;
    ComponentRef(const UUID128 &ownerObjectID, size_t addedID) : ownerObjectID(ownerObjectID), addedID(addedID) {}

    UUID128 ownerObjectID;
    size_t addedID = MAXSIZE_T;

    bool IsValid() const { return ownerObjectID.IsValid() && addedID != MAXSIZE_T; }

    bool operator==(const ComponentRef &other) const {
        return ownerObjectID == other.ownerObjectID && addedID == other.addedID;
    }
    bool operator!=(const ComponentRef &other) const { return !(*this == other); }
};

} // namespace KashipanEngine

namespace std {
template <>
struct hash<KashipanEngine::ComponentRef> {
    std::size_t operator()(const KashipanEngine::ComponentRef &ref) const noexcept {
        std::size_t h1 = std::hash<KashipanEngine::UUID128>()(ref.ownerObjectID);
        std::size_t h2 = std::hash<size_t>()(ref.addedID);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};
} // namespace std
