#include "ComponentRegistry.h"

namespace KashipanEngine {

namespace Local {
std::unordered_map<std::string, std::function<std::unique_ptr<ISceneComponent>()>> &GetSceneComponentFactoryMap() {
    static std::unordered_map<std::string, std::function<std::unique_ptr<ISceneComponent>()>> sceneComponentFactoryMap;
    return sceneComponentFactoryMap;
}
std::unordered_map<std::string, std::function<std::unique_ptr<IObjectComponent>()>> &GetObjectComponentFactoryMap() {
    static std::unordered_map<std::string, std::function<std::unique_ptr<IObjectComponent>()>> object3DComponentFactoryMap;
    return object3DComponentFactoryMap;
}

std::vector<std::string> &GetRegisteredSceneComponentTypes() {
    static std::vector<std::string> registeredSceneComponentTypes;
    return registeredSceneComponentTypes;
}
std::vector<std::string> &GetRegisteredObjectComponentTypes() {
    static std::vector<std::string> registeredObjectComponentTypes;
    return registeredObjectComponentTypes;
}
} // namespace Local

bool RegisterComponentTypeScene(const std::string &typeName, std::function<std::unique_ptr<ISceneComponent>()> createFunc) {
    auto &factoryMap = Local::GetSceneComponentFactoryMap();
    if (factoryMap.find(typeName) != factoryMap.end()) return false;
    factoryMap[typeName] = createFunc;
    Local::GetRegisteredSceneComponentTypes().push_back(typeName);
    return true;
}
bool RegisterComponentTypeObject(const std::string &typeName, std::function<std::unique_ptr<IObjectComponent>()> createFunc) {
    auto &factoryMap = Local::GetObjectComponentFactoryMap();
    if (factoryMap.find(typeName) != factoryMap.end()) return false;
    factoryMap[typeName] = createFunc;
    Local::GetRegisteredObjectComponentTypes().push_back(typeName);
    return true;
}

std::unique_ptr<ISceneComponent> CreateSceneComponentByType(const std::string &typeName) {
    auto it = Local::GetSceneComponentFactoryMap().find(typeName);
    if (it != Local::GetSceneComponentFactoryMap().end()) {
        return it->second();
    }
    return nullptr;
}
std::unique_ptr<IObjectComponent> CreateObjectComponentByType(const std::string &typeName) {
    auto it = Local::GetObjectComponentFactoryMap().find(typeName);
    if (it != Local::GetObjectComponentFactoryMap().end()) {
        return it->second();
    }
    return nullptr;
}

const std::vector<std::string> &GetRegisteredSceneComponentTypes() {
    return Local::GetRegisteredSceneComponentTypes();
}
const std::vector<std::string> &GetRegisteredObjectComponentTypes() {
    return Local::GetRegisteredObjectComponentTypes();
}

} // namespace KashipanEngine
