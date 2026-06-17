#pragma once
#include "ComponentSerialize/ComponentSerialize.h"
#include "Objects/IObjectComponent.h"
#include "Scene/Components/ISceneComponent.h"

namespace KashipanEngine {

bool RegisterComponentTypeScene(const std::string &typeName, std::function<std::unique_ptr<ISceneComponent>()> createFunc);
bool RegisterComponentTypeObject2D(const std::string &typeName, std::function<std::unique_ptr<IObjectComponent2D>()> createFunc);
bool RegisterComponentTypeObject3D(const std::string &typeName, std::function<std::unique_ptr<IObjectComponent3D>()> createFunc);

std::unique_ptr<ISceneComponent> CreateSceneComponentByType(const std::string &typeName);
std::unique_ptr<IObjectComponent2D> CreateObject2DComponentByType(const std::string &typeName);
std::unique_ptr<IObjectComponent3D> CreateObject3DComponentByType(const std::string &typeName);

const std::vector<std::string> &GetRegisteredSceneComponentTypes();
const std::vector<std::string> &GetRegisteredObject2DComponentTypes();
const std::vector<std::string> &GetRegisteredObject3DComponentTypes();

#define REGISTER_COMPONENT_SCENE(ComponentClass) \
    static const bool is##ComponentClass##RegisteredInScene = RegisterComponentTypeScene( \
        #ComponentClass, \
        []() -> std::unique_ptr<ISceneComponent> { return std::make_unique<ComponentClass>(); } \
    );
#define REGISTER_COMPONENT_OBJECT2D(ComponentClass) \
    static const bool is##ComponentClass##RegisteredInObject2D = RegisterComponentTypeObject2D( \
        #ComponentClass, \
        []() -> std::unique_ptr<IObjectComponent2D> { return std::make_unique<ComponentClass>(); } \
    );
#define REGISTER_COMPONENT_OBJECT3D(ComponentClass) \
    static const bool is##ComponentClass##RegisteredInObject3D = RegisterComponentTypeObject3D( \
        #ComponentClass, \
        []() -> std::unique_ptr<IObjectComponent3D> { return std::make_unique<ComponentClass>(); } \
    );

} // namespace KashipanEngine