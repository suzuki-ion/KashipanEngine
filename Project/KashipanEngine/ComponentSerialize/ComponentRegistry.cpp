#include "ComponentRegistry.h"
#include "Objects/IObjectComponent.h"
#include "Scene/Components/ISceneComponent.h"
#include "Debug/Logger.h"

namespace KashipanEngine {

namespace Local {
std::unordered_map<std::string, std::function<std::unique_ptr<ISceneComponent>()>> &GetSceneComponentFactoryMap() {
    LogScope scope;
    static std::unordered_map<std::string, std::function<std::unique_ptr<ISceneComponent>()>> sceneComponentFactoryMap;
    return sceneComponentFactoryMap;
}
std::unordered_map<std::string, std::function<std::unique_ptr<IObjectComponent>()>> &GetObjectComponentFactoryMap() {
    LogScope scope;
    static std::unordered_map<std::string, std::function<std::unique_ptr<IObjectComponent>()>> object3DComponentFactoryMap;
    return object3DComponentFactoryMap;
}
std::unordered_map<size_t, std::function<std::unique_ptr<IComponentPoolBase>()>> &GetObjectComponentPoolFactoryMap() {
    LogScope scope;
    static std::unordered_map<size_t, std::function<std::unique_ptr<IComponentPoolBase>()>> poolFactoryMap;
    return poolFactoryMap;
}
std::unordered_map<size_t, bool> &GetObjectComponentBatchProcessedMap() {
    LogScope scope;
    static std::unordered_map<size_t, bool> batchProcessedMap;
    return batchProcessedMap;
}
/// @brief バッチ処理対象として登録された型の数（0であればRegenerateUpdateComponentsList側で
///        型ごとのハッシュ検索そのものを丸ごと省略できる。isBatchProcessed=trueの型が
///        1つも無い今の時点では常に0になる）
size_t &GetBatchProcessedTypeCount() {
    LogScope scope;
    static size_t count = 0;
    return count;
}

std::vector<std::string> &GetRegisteredSceneComponentTypes() {
    LogScope scope;
    static std::vector<std::string> registeredSceneComponentTypes;
    return registeredSceneComponentTypes;
}
std::vector<std::string> &GetRegisteredObjectComponentTypes() {
    LogScope scope;
    static std::vector<std::string> registeredObjectComponentTypes;
    return registeredObjectComponentTypes;
}

std::unordered_map<std::string, std::vector<std::string>> &GetSceneComponentCategoryMap() {
    LogScope scope;
    static std::unordered_map<std::string, std::vector<std::string>> sceneComponentCategoryMap;
    return sceneComponentCategoryMap;
}
std::unordered_map<std::string, std::vector<std::string>> &GetObjectComponentCategoryMap() {
    LogScope scope;
    static std::unordered_map<std::string, std::vector<std::string>> objectComponentCategoryMap;
    return objectComponentCategoryMap;
}
} // namespace Local

bool RegisterComponentTypeScene(const std::string &typeName, std::function<std::unique_ptr<ISceneComponent>()> createFunc, const std::vector<std::string> &category) {
    LogScope scope;
    auto &factoryMap = Local::GetSceneComponentFactoryMap();
    if (factoryMap.find(typeName) != factoryMap.end()) return false;
    factoryMap[typeName] = createFunc;
    Local::GetRegisteredSceneComponentTypes().push_back(typeName);
    Local::GetSceneComponentCategoryMap()[typeName] = category;
    return true;
}
bool RegisterComponentTypeObject(
    const std::string &typeName,
    size_t typeID,
    std::function<std::unique_ptr<IObjectComponent>()> createFunc,
    std::function<std::unique_ptr<IComponentPoolBase>()> poolFactory,
    bool isBatchProcessed,
    const std::vector<std::string> &category) {
    LogScope scope;
    auto &factoryMap = Local::GetObjectComponentFactoryMap();
    if (factoryMap.find(typeName) != factoryMap.end()) return false;
    factoryMap[typeName] = createFunc;
    Local::GetObjectComponentPoolFactoryMap()[typeID] = poolFactory;
    Local::GetObjectComponentBatchProcessedMap()[typeID] = isBatchProcessed;
    if (isBatchProcessed) ++Local::GetBatchProcessedTypeCount();
    Local::GetRegisteredObjectComponentTypes().push_back(typeName);
    Local::GetObjectComponentCategoryMap()[typeName] = category;
    return true;
}
bool RegisterObjectComponentTypeAlias(
    const std::string &aliasName,
    std::function<std::unique_ptr<IObjectComponent>()> createFunc) {
    LogScope scope;
    auto &factoryMap = Local::GetObjectComponentFactoryMap();
    if (factoryMap.find(aliasName) != factoryMap.end()) return false;
    factoryMap[aliasName] = createFunc;
    return true;
}

std::unique_ptr<ISceneComponent> CreateSceneComponentByType(const std::string &typeName) {
    LogScope scope;
    auto it = Local::GetSceneComponentFactoryMap().find(typeName);
    if (it != Local::GetSceneComponentFactoryMap().end()) {
        return it->second();
    }
    return nullptr;
}
std::unique_ptr<IObjectComponent> CreateObjectComponentByType(const std::string &typeName) {
    LogScope scope;
    auto it = Local::GetObjectComponentFactoryMap().find(typeName);
    if (it != Local::GetObjectComponentFactoryMap().end()) {
        return it->second();
    }
    return nullptr;
}
std::unique_ptr<IComponentPoolBase> CreateObjectComponentPoolByTypeID(size_t typeID) {
    LogScope scope;
    auto &map = Local::GetObjectComponentPoolFactoryMap();
    auto it = map.find(typeID);
    if (it != map.end()) {
        return it->second();
    }
    return nullptr;
}
bool IsObjectComponentTypeIDBatchProcessed(size_t typeID) {
    LogScope scope;
    auto &map = Local::GetObjectComponentBatchProcessedMap();
    auto it = map.find(typeID);
    return it != map.end() && it->second;
}
bool HasAnyBatchProcessedObjectComponentType() {
    LogScope scope;
    return Local::GetBatchProcessedTypeCount() > 0;
}

const std::vector<std::string> &GetRegisteredSceneComponentTypes() {
    LogScope scope;
    return Local::GetRegisteredSceneComponentTypes();
}
const std::vector<std::string> &GetRegisteredObjectComponentTypes() {
    LogScope scope;
    return Local::GetRegisteredObjectComponentTypes();
}

const std::vector<std::string> &GetSceneComponentCategory(const std::string &typeName) {
    LogScope scope;
    static const std::vector<std::string> kEmptyCategory;
    const auto &map = Local::GetSceneComponentCategoryMap();
    auto it = map.find(typeName);
    return it != map.end() ? it->second : kEmptyCategory;
}
const std::vector<std::string> &GetObjectComponentCategory(const std::string &typeName) {
    LogScope scope;
    static const std::vector<std::string> kEmptyCategory;
    const auto &map = Local::GetObjectComponentCategoryMap();
    auto it = map.find(typeName);
    return it != map.end() ? it->second : kEmptyCategory;
}

} // namespace KashipanEngine
