#include "ComponentRegistry.h"

namespace KashipanEngine {

namespace Local {
std::unordered_map<std::string, std::function<std::unique_ptr<ISceneComponent>()>> &GetSceneComponentFactoryMap() {
    static std::unordered_map<std::string, std::function<std::unique_ptr<ISceneComponent>()>> sceneComponentFactoryMap;
    return sceneComponentFactoryMap;
}
std::unordered_map<std::string, std::function<std::unique_ptr<IObjectComponent2D>()>> &GetObject2DComponentFactoryMap() {
    static std::unordered_map<std::string, std::function<std::unique_ptr<IObjectComponent2D>()>> object2DComponentFactoryMap;
    return object2DComponentFactoryMap;
}
std::unordered_map<std::string, std::function<std::unique_ptr<IObjectComponent3D>()>> &GetObject3DComponentFactoryMap() {
    static std::unordered_map<std::string, std::function<std::unique_ptr<IObjectComponent3D>()>> object3DComponentFactoryMap;
    return object3DComponentFactoryMap;
}

std::vector<std::string> &GetRegisteredSceneComponentTypes() {
    static std::vector<std::string> registeredSceneComponentTypes;
    return registeredSceneComponentTypes;
}
std::vector<std::string> &GetRegisteredObject2DComponentTypes() {
    static std::vector<std::string> registeredObject2DComponentTypes;
    return registeredObject2DComponentTypes;
}
std::vector<std::string> &GetRegisteredObject3DComponentTypes() {
    static std::vector<std::string> registeredObject3DComponentTypes;
    return registeredObject3DComponentTypes;
}
} // namespace Local

bool RegisterComponentTypeScene(const std::string &typeName, std::function<std::unique_ptr<ISceneComponent>()> createFunc) {
    auto &factoryMap = Local::GetSceneComponentFactoryMap();
    if (factoryMap.find(typeName) != factoryMap.end()) return false;
    factoryMap[typeName] = createFunc;
    Local::GetRegisteredSceneComponentTypes().push_back(typeName);
    return true;
}
bool RegisterComponentTypeObject2D(const std::string &typeName, std::function<std::unique_ptr<IObjectComponent2D>()> createFunc) {
    auto &factoryMap = Local::GetObject2DComponentFactoryMap();
    if (factoryMap.find(typeName) != factoryMap.end()) return false;
    factoryMap[typeName] = createFunc;
    Local::GetRegisteredObject2DComponentTypes().push_back(typeName);
    return true;
}
bool RegisterComponentTypeObject3D(const std::string &typeName, std::function<std::unique_ptr<IObjectComponent3D>()> createFunc) {
    auto &factoryMap = Local::GetObject3DComponentFactoryMap();
    if (factoryMap.find(typeName) != factoryMap.end()) return false;
    factoryMap[typeName] = createFunc;
    Local::GetRegisteredObject3DComponentTypes().push_back(typeName);
    return true;
}

std::unique_ptr<ISceneComponent> CreateSceneComponentByType(const std::string &typeName) {
    auto it = Local::GetSceneComponentFactoryMap().find(typeName);
    if (it != Local::GetSceneComponentFactoryMap().end()) {
        return it->second();
    }
    return nullptr;
}
std::unique_ptr<IObjectComponent2D> CreateObject2DComponentByType(const std::string &typeName) {
    auto it = Local::GetObject2DComponentFactoryMap().find(typeName);
    if (it != Local::GetObject2DComponentFactoryMap().end()) {
        return it->second();
    }
    return nullptr;
}
std::unique_ptr<IObjectComponent3D> CreateObject3DComponentByType(const std::string &typeName) {
    auto it = Local::GetObject3DComponentFactoryMap().find(typeName);
    if (it != Local::GetObject3DComponentFactoryMap().end()) {
        return it->second();
    }
    return nullptr;
}

const std::vector<std::string> &GetRegisteredSceneComponentTypes() {
    return Local::GetRegisteredSceneComponentTypes();
}
const std::vector<std::string> &GetRegisteredObject2DComponentTypes() {
    return Local::GetRegisteredObject2DComponentTypes();
}
const std::vector<std::string> &GetRegisteredObject3DComponentTypes() {
    return Local::GetRegisteredObject3DComponentTypes();
}

} // namespace KashipanEngine
