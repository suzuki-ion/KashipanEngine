#pragma once
#include <string>
#include <type_traits>
#include <vector>
#include "ComponentSerialize/ComponentSerialize.h"
#include "ComponentSerialize/TypeToString.h"

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
bool RegisterComponentTypeObject(const std::string &typeName, std::function<std::unique_ptr<IObjectComponent>()> createFunc, const std::vector<std::string> &category = {});

std::unique_ptr<ISceneComponent> CreateSceneComponentByType(const std::string &typeName);
std::unique_ptr<IObjectComponent> CreateObjectComponentByType(const std::string &typeName);

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
        []() -> std::unique_ptr<IObjectComponent> { return std::make_unique<ComponentClass>(); }, \
        ComponentCategoryOf<ComponentClass>::Get() \
    );

} // namespace KashipanEngine
