#pragma once
#include "ComponentSerialize/ComponentSerialize.h"
#include "ComponentSerialize/TypeToString.h"

namespace KashipanEngine {

class ISceneComponent;
class IObjectComponent;

bool RegisterComponentTypeScene(const std::string &typeName, std::function<std::unique_ptr<ISceneComponent>()> createFunc);
bool RegisterComponentTypeObject(const std::string &typeName, std::function<std::unique_ptr<IObjectComponent>()> createFunc);

std::unique_ptr<ISceneComponent> CreateSceneComponentByType(const std::string &typeName);
std::unique_ptr<IObjectComponent> CreateObjectComponentByType(const std::string &typeName);

const std::vector<std::string> &GetRegisteredSceneComponentTypes();
const std::vector<std::string> &GetRegisteredObjectComponentTypes();

#define REGISTER_COMPONENT_SCENE(ComponentClass) \
    static const bool is##ComponentClass##RegisteredInScene = RegisterComponentTypeScene( \
        #ComponentClass, \
        []() -> std::unique_ptr<ISceneComponent> { return std::make_unique<ComponentClass>(); } \
    );
#define REGISTER_COMPONENT_OBJECT(ComponentClass) \
    static const bool is##ComponentClass##RegisteredInObject = RegisterComponentTypeObject( \
        #ComponentClass, \
        []() -> std::unique_ptr<IObjectComponent> { return std::make_unique<ComponentClass>(); } \
    );

} // namespace KashipanEngine