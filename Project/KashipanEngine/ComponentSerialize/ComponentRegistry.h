#pragma once
#include <string>
#include <type_traits>
#include <vector>
#include "ComponentSerialize/ComponentSerialize.h"
#include "ComponentSerialize/TypeToString.h"
#include "Objects/ComponentPool.h"

namespace KashipanEngine {

class ISceneComponent;
class IObjectComponent;

/// @brief コンポーネントのカテゴリ取得用トレイト
/// @details クラスに public static な GetComponentCategory() が定義されていればそれを使い、
///          未定義の場合はカテゴリ無し（ルート直下）として扱う。
///          カテゴリは階層構造を表す文字列リスト（例: {"Collision", "Collider"}）。
template <typename T, typename = void>
struct ComponentCategoryOf {
    static std::vector<std::string> Get() { return {}; }
};
template <typename T>
struct ComponentCategoryOf<T, std::void_t<decltype(T::GetComponentCategory())>> {
    static std::vector<std::string> Get() { return T::GetComponentCategory(); }
};

bool RegisterComponentTypeScene(const std::string &typeName, std::function<std::unique_ptr<ISceneComponent>()> createFunc, const std::vector<std::string> &category = {});
/// @brief オブジェクトコンポーネント型を登録する
/// @param typeID コンポーネントの型ID（IObjectComponent::GetComponentTypeID<T>()）
/// @param createFunc 型名からデタッチされたunique_ptrインスタンスを生成するファクトリ（JSON等からの状態転送元として使用）
/// @param poolFactory その型専用の空のComponentPool<T>を生成するファクトリ
/// @param isBatchProcessed ComponentBatchTraits<T>::kIsBatchProcessed の値
bool RegisterComponentTypeObject(
    const std::string &typeName,
    size_t typeID,
    std::function<std::unique_ptr<IObjectComponent>()> createFunc,
    std::function<std::unique_ptr<IComponentPoolBase>()> poolFactory,
    bool isBatchProcessed,
    const std::vector<std::string> &category = {});

std::unique_ptr<ISceneComponent> CreateSceneComponentByType(const std::string &typeName);
std::unique_ptr<IObjectComponent> CreateObjectComponentByType(const std::string &typeName);
/// @brief 型IDから、その型専用の空のコンポーネントプールを生成する（未登録の場合は nullptr）
std::unique_ptr<IComponentPoolBase> CreateObjectComponentPoolByTypeID(size_t typeID);
/// @brief 型IDが指すコンポーネント型がバッチ処理対象としてマークされているか（未登録の場合は false）
bool IsObjectComponentTypeIDBatchProcessed(size_t typeID);
/// @brief バッチ処理対象としてマークされた型が1つでも存在するか
/// @details false の場合、呼び出し側は型ごとの IsObjectComponentTypeIDBatchProcessed 検索を
///          丸ごと省略できる（毎フレームのホットパスでの無駄なハッシュ検索を避けるためのショートカット）
bool HasAnyBatchProcessedObjectComponentType();

const std::vector<std::string> &GetRegisteredSceneComponentTypes();
const std::vector<std::string> &GetRegisteredObjectComponentTypes();

/// @brief 登録済みシーンコンポーネントのカテゴリを取得（未登録・カテゴリ無しの場合は空リスト）
const std::vector<std::string> &GetSceneComponentCategory(const std::string &typeName);
/// @brief 登録済みオブジェクトコンポーネントのカテゴリを取得（未登録・カテゴリ無しの場合は空リスト）
const std::vector<std::string> &GetObjectComponentCategory(const std::string &typeName);

/// @brief コンポーネントのカテゴリを宣言するマクロ（クラス定義内の public 部に書く）
/// @details 例: COMPONENT_CATEGORY("Render") や COMPONENT_CATEGORY("Collision", "Collider")
///          カテゴリは Add Component メニューでツリー表示される。
#define COMPONENT_CATEGORY(...) \
    static std::vector<std::string> GetComponentCategory() { return { __VA_ARGS__ }; }

#define REGISTER_COMPONENT_SCENE(ComponentClass) \
    static const bool is##ComponentClass##RegisteredInScene = RegisterComponentTypeScene( \
        #ComponentClass, \
        []() -> std::unique_ptr<ISceneComponent> { return std::make_unique<ComponentClass>(); }, \
        ComponentCategoryOf<ComponentClass>::Get() \
    );
#define REGISTER_COMPONENT_OBJECT(ComponentClass) \
    static const bool is##ComponentClass##RegisteredInObject = RegisterComponentTypeObject( \
        #ComponentClass, \
        IObjectComponent::GetComponentTypeID<ComponentClass>(), \
        []() -> std::unique_ptr<IObjectComponent> { return std::make_unique<ComponentClass>(); }, \
        []() -> std::unique_ptr<IComponentPoolBase> { return std::make_unique<ComponentPool<ComponentClass>>(); }, \
        ComponentBatchTraits<ComponentClass>::kIsBatchProcessed, \
        ComponentCategoryOf<ComponentClass>::Get() \
    );

} // namespace KashipanEngine
