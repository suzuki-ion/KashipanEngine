#include "Objects/Components/ShortTextExtended.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include "Objects/EmptyObject.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/TextureSource.h"
#include "Objects/Components/Render/SpriteRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/TextRenderer.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Objects/Components/Render/NormalWindowObject.h"
#include "Objects/Components/Render/OverlayWindowObject.h"
#include "Assets/FontManager.h"
#include "Scene/SceneContext.h"

namespace KashipanEngine::ShortText {
namespace {
bool Fail(std::string &error, const std::string &reason) { error = reason; return false; }
double Number(const Value &args, const char *key, double fallback) {
    if (!args.contains(key)) return fallback;
    if (!args.at(key).is_number()) throw std::runtime_error(std::string(key) + " must be a number");
    const double value = args.at(key).get<double>();
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) throw std::runtime_error(std::string(key) + " must be finite");
    return value;
}
template<size_t N> void Array(const Value &args, const char *key, float (&out)[N]) {
    const auto &a = args.at(key);
    if (!a.is_array() || a.size() != N) throw std::runtime_error(std::string(key) + " has the wrong dimensions");
    for (size_t i = 0; i < N; ++i) {
        if (!a[i].is_number()) throw std::runtime_error(std::string(key) + " must contain numbers");
        const double value = a[i].get<double>();
        if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) throw std::runtime_error(std::string(key) + " must be finite");
        out[i] = static_cast<float>(value);
    }
}
IObjectComponent *Component(EmptyObject *object, const std::string &type) {
    for (const auto &[component, id] : object->GetAllComponents()) if (component && component->GetComponentType() == type) return component;
    return nullptr;
}
bool IsTarget(EmptyObject *object) {
    return object && (object->GetComponent<ScreenBufferObject>() || object->GetComponent<NormalWindowObject>() || object->GetComponent<OverlayWindowObject>());
}
bool Configure(EmptyObject *object, IObjectComponent *component, const Value &values, std::string &error) {
    if (!component || !values.is_object() || values.empty()) return Fail(error, "A component and nonempty member values are required");
    // Validate all names, dimensions and types before changing any member. Restore on setter failure.
    for (auto it = values.begin(); it != values.end(); ++it) {
        Value old;
        if (!ReadExtendedProperty(object, component->GetComponentType() + "." + it.key(), old)) return Fail(error, "Unknown or unsupported property: " + it.key());
        if (!(old.is_number() && it.value().is_number()) && old.type() != it.value().type()) return Fail(error, "Property type mismatch: " + it.key());
        if (old.is_array() && old.size() != it.value().size()) return Fail(error, "Property dimension mismatch: " + it.key());
    }
    const auto before = object->SaveComponentToJson(component);
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (!WriteExtendedProperty(object, component->GetComponentType() + "." + it.key(), it.value())) {
            object->LoadComponentFromJson(component, before);
            return Fail(error, "Property setter rejected: " + it.key());
        }
    }
    return true;
}
}

bool ExecuteRenderingCommand(const std::string &command, const Value &args, CommandContext &context, std::string &error) {
    error.clear();
    const bool known = command == "render.texture" || command == "render.material" || command == "render.pipeline" || command == "render.uv" ||
        command == "render.layout" || command == "render.target" || command == "render.order" || command == "render.layer" ||
        command == "render.material_parameter" || command == "post.configure" || command == "text.style" || command == "text.format" ||
        command == "camera.configure" || command == "render_target.configure" || command == "component.add" || command == "component.remove" || command == "component.configure";
    if (!known) return false;
    if (!context.target || !context.scene) return Fail(error, "A live target and scene are required");
    if (!args.is_object()) return Fail(error, "Command arguments must be an object");
    auto *object = context.target;
    auto *sprite = object->GetComponent<SpriteRenderer>();
    auto *mesh = object->GetComponent<MeshRenderer>();
    auto *text = object->GetComponent<TextRenderer>();
    try {
        if (command == "render.layer") {
            if (!sprite && !mesh && !text) return Fail(error,"A renderer is required");
            const auto &layer = args.at("layer");
            int32_t priority = 0;
            if (layer.is_string()) {
                const auto name = layer.get<std::string>();
                if (name == "Background") priority = -10000; else if (name == "World") priority = 0;
                else if (name == "UI") priority = 10000; else if (name == "Overlay") priority = 20000;
                else return Fail(error,"Layer must be Background, World, UI, Overlay or an int32 priority");
            } else if (layer.is_number_integer()) {
                const double value = layer.get<double>();
                if (value < INT32_MIN || value > INT32_MAX) return Fail(error,"Layer priority is outside int32");
                priority = static_cast<int32_t>(value);
            } else return Fail(error,"Layer must be a name or integer");
            if (sprite) sprite->SetRenderPriority(priority); if (mesh) mesh->SetRenderPriority(priority); if (text) text->SetRenderPriority(priority);
            return true;
        }
        if (command == "component.add") {
            const std::string type = args.at("type").get<std::string>();
            if (type == "ShortTextBehavior") return Fail(error, "Recursive behavior creation is not supported");
            if (Component(object, type)) return Fail(error, "Component already exists");
            auto candidate = CreateObjectComponentByType(type);
            if (!candidate) return Fail(error, "Unknown component type");
            auto *added = object->AddComponent(std::move(candidate));
            if (!added) return Fail(error, "Component construction failed");
            if (args.contains("values") && !Configure(object, added, args.at("values"), error)) { object->RemoveComponent(added); return false; }
            return true;
        }
        if (command == "component.remove") {
            const std::string type = args.at("type").get<std::string>();
            if (type == "Transform" || type == "ShortTextBehavior") return Fail(error, "Required or currently executing components cannot be removed");
            auto *component = Component(object, type);
            return component && object->RemoveComponent(component) ? true : Fail(error, "Component removal failed");
        }
        if (command == "component.configure" || command == "post.configure") {
            const std::string type = args.at("type").get<std::string>();
            if (command == "post.configure" && !type.ends_with("Effect")) return Fail(error, "post.configure requires an effect component");
            return Configure(object, Component(object, type), args.at("values"), error);
        }
        if (command == "render.texture") {
            const auto path = args.at("asset").get<std::string>();
            if (!sprite && !mesh) return Fail(error, "SpriteRenderer or MeshRenderer is required");
            if (TextureManager::GetTextureFromAssetPath(path) == TextureManager::kInvalidHandle) return Fail(error, "Texture asset is not loaded");
            auto *source = object->GetComponent<TextureSource>();
            if (!source) source = object->AddComponent<TextureSource>();
            if (!source) return Fail(error, "TextureSource creation failed");
            source->SetTextureAssetPath(path);
            return true;
        }
        if (command == "render.material" || command == "render.pipeline") {
            const auto name = args.at("name").get<std::string>();
            if (name.empty() || (!sprite && !mesh && !text)) return Fail(error, "A renderer and nonempty resource name are required");
            if (command == "render.material") {
                if (MaterialManager::GetMaterialHandleFromName(name) == MaterialManager::kInvalidHandle) return Fail(error, "Material is not loaded");
                if (sprite) sprite->SetMaterialName(name); else if (mesh) mesh->SetMaterialName(name); else text->SetMaterialName(name);
            } else {
                if (sprite) sprite->SetPipelineName(name); else if (mesh) mesh->SetPipelineName(name); else text->SetPipelineName(name);
            }
            return true;
        }
        if (command == "render.uv") {
            if (!sprite) return Fail(error, "SpriteRenderer is required");
            float offset[2]{0,0}, size[2]{1,1};
            if (args.contains("offset")) Array(args, "offset", offset);
            if (args.contains("size")) Array(args, "size", size);
            const float rotation = static_cast<float>(Number(args, "rotation", 0));
            const bool flipX = args.value("flipX", false), flipY = args.value("flipY", false);
            if (size[0] <= 0 || size[1] <= 0) return Fail(error, "UV rectangle size must be positive");
            if (flipX) { offset[0] += size[0]; size[0] = -size[0]; }
            if (flipY) { offset[1] += size[1]; size[1] = -size[1]; }
            sprite->SetInstanceUvTranslate({offset[0],offset[1]});
            sprite->SetInstanceUvScale({size[0],size[1]});
            sprite->SetInstanceUvRotation(rotation);
            sprite->SetInstanceUvCombineMode(SpriteRenderer::UVCombineMode::InstanceOnly);
            return true;
        }
        if (command == "render.target") {
            auto *target = context.scene->GetSceneObject(args.at("name").get<std::string>());
            if (!IsTarget(target)) return Fail(error, "Target must own a Window or ScreenBuffer component");
            auto *camera = object->GetComponent<CameraRenderer>();
            if (!sprite && !mesh && !text && !camera) return Fail(error, "A renderer is required");
            if (sprite) sprite->SetTargetObject(target); if (mesh) mesh->SetTargetObject(target); if (text) text->SetTargetObject(target); if (camera) camera->SetTargetObject(target);
            return true;
        }
        if (command == "render.order") {
            const double priority = Number(args, "priority", 0);
            if (std::floor(priority) != priority || priority < INT32_MIN || priority > INT32_MAX) return Fail(error, "Priority must be an int32");
            auto *buffer = object->GetComponent<ScreenBufferObject>();
            if (!sprite && !mesh && !text && !buffer) return Fail(error, "A renderer or ScreenBuffer is required");
            if (sprite) sprite->SetRenderPriority(static_cast<int32_t>(priority)); if (mesh) mesh->SetRenderPriority(static_cast<int32_t>(priority));
            if (text) text->SetRenderPriority(static_cast<int32_t>(priority)); if (buffer) buffer->SetRenderOrderPriority(static_cast<int32_t>(priority));
            return true;
        }
        if (command == "render.layout") {
            if (!sprite) return Fail(error, "Layout requires a SpriteRenderer with a unit quad mesh");
            auto *transform = object->GetComponent<Transform>();
            if (!transform || transform->GetParentObject()) return Fail(error, "Layout currently requires a root Transform");
            auto *source = context.scene->GetSceneObject(args.at("sourceName").get<std::string>());
            if (!IsTarget(source)) return Fail(error, "Layout source must be a Window or ScreenBuffer object");
            double width = 0, height = 0;
            if (auto *buffer = source->GetComponent<ScreenBufferObject>()) {
                uint32_t w = 0, h = 0;
                buffer->GetMemberValue("width_", w); buffer->GetMemberValue("height_", h); width = w; height = h;
                if (auto *live = buffer->GetScreenBuffer()) { width = live->GetWidth(); height = live->GetHeight(); }
            } else {
                IWindowObjectComponent *window = source->GetComponent<NormalWindowObject>();
                if (!window) window = source->GetComponent<OverlayWindowObject>();
                uint32_t w = 0, h = 0;
                window->GetMemberValue("width_", w); window->GetMemberValue("height_", h); width = w; height = h;
                if (auto *live = window->GetWindow()) { width = live->GetClientWidth(); height = live->GetClientHeight(); }
            }
            Vector3 center{};
            EmptyObject *cameraObject = nullptr;
            if (args.contains("cameraName")) cameraObject = context.scene->GetSceneObject(args.at("cameraName").get<std::string>());
            else {
                for (auto *candidate : context.scene->GetSceneObjects()) {
                    auto *renderer = candidate ? candidate->GetComponent<CameraRenderer>() : nullptr;
                    if (!renderer || !candidate->GetComponent<Camera2D>()) continue;
                    if (renderer->GetTargetObjectID().IsValid() && renderer->GetTargetObjectID() != source->GetObjectID()) continue;
                    if (cameraObject) return Fail(error,"Multiple matching Camera2D objects; specify cameraName");
                    cameraObject = candidate;
                }
            }
            {
                auto *camera = cameraObject ? cameraObject->GetComponent<Camera2D>() : nullptr;
                auto *cameraTransform = cameraObject ? cameraObject->GetComponent<Transform>() : nullptr;
                if (!camera || !cameraTransform || cameraTransform->GetParentObject() || cameraTransform->GetRotate() != Vector3{} || cameraTransform->GetScale() != Vector3{1,1,1}) return Fail(error, "Layout requires one root unrotated unscaled Camera2D; specify cameraName");
                if (!camera->GetAutoSyncSize()) { width = camera->GetWidth(); height = camera->GetHeight(); }
                center = cameraTransform->GetTranslate();
                // CameraRenderer's orthographic range is [0,width] x [0,height], not centered on zero.
                center.x += static_cast<float>(width * 0.5); center.y += static_cast<float>(height * 0.5);
            }
            if (width <= 0 || height <= 0) return Fail(error, "Layout source has invalid dimensions");
            float anchor[2]{0.5f,0.5f}, pivot[2]{0.5f,0.5f}, offset[2]{0,0};
            if (args.contains("anchor")) Array(args,"anchor",anchor); if (args.contains("pivot")) Array(args,"pivot",pivot); if (args.contains("offset")) Array(args,"offset",offset);
            const std::string edge = args.value("edge", std::string("center"));
            if (edge != "center" && edge != "left" && edge != "right" && edge != "top" && edge != "bottom" && edge != "top_right" && edge != "top_left" && edge != "bottom_left" && edge != "bottom_right") return Fail(error, "Unknown screen edge");
            if (args.value("cover", false)) { auto scale = transform->GetScale(); scale.x = static_cast<float>(width); scale.y = static_cast<float>(height); transform->SetScale(scale); }
            auto position = transform->GetTranslate();
            position.x = center.x + offset[0]; position.y = center.y + offset[1];
            if (edge.find("left") != std::string::npos) position.x -= static_cast<float>(width * 0.5);
            if (edge.find("right") != std::string::npos) position.x += static_cast<float>(width * 0.5);
            if (edge.find("top") != std::string::npos) position.y += static_cast<float>(height * 0.5);
            if (edge.find("bottom") != std::string::npos) position.y -= static_cast<float>(height * 0.5);
            sprite->SetAnchor({anchor[0],anchor[1]}); sprite->SetPivot({pivot[0],pivot[1]}); sprite->SetTargetObject(source); transform->SetTranslate(position);
            return true;
        }
        if (command == "render.material_parameter") {
            if (!args.value("shared", false)) return Fail(error, "Material changes affect all users; explicitly specify shared:true");
            const auto materialName = args.at("material").get<std::string>();
            auto *material = MaterialManager::GetMaterial(materialName);
            if (!material) return Fail(error, "Material is not loaded");
            const auto key = args.at("parameter").get<std::string>();
            const auto &value = args.at("value");
            if (key == "color" || key == "specularColor" || key == "rimColor") {
                float a[4]; Array(args,"value",a); Vector4 color{a[0],a[1],a[2],a[3]};
                if (key == "color") material->color = color; else if (key == "rimColor") material->rimColor = color; else material->specularColor = color;
            } else if (key == "shininess" || key == "rimPower" || key == "rimIntensity" || key == "uvRotation") {
                const float v = static_cast<float>(Number(args,"value",0));
                if (key == "shininess") material->shininess = v; else if (key == "rimPower") material->rimPower = v; else if (key == "rimIntensity") material->rimIntensity = v; else material->uvRotation = v;
            } else if (key == "enableLighting" || key == "enableShadowMapProjection") {
                if (!value.is_boolean()) return Fail(error,"Lighting flag must be boolean");
                if (key == "enableLighting") material->enableLighting = value.get<bool>(); else material->enableShadowMapProjection = value.get<bool>();
            } else if (key == "uvScale" || key == "uvTranslate" || key == "uvPivot") {
                float a[2]; Array(args,"value",a); Vector2 v{a[0],a[1]};
                if (key == "uvScale") material->uvScale = v; else if (key == "uvTranslate") material->uvTranslate = v; else material->uvPivot = v;
            } else if (args.value("extra", false)) {
                if (value.is_number()) material->extraParameters[key] = static_cast<float>(Number(args,"value",0));
                else if (value.is_boolean()) material->extraParameters[key] = value.get<bool>();
                else if (value.is_array() && value.size() == 4) { float a[4]; Array(args,"value",a); material->extraParameters[key] = Vector4{a[0],a[1],a[2],a[3]}; }
                else return Fail(error,"Extra parameter supports float, bool or vec4");
            } else return Fail(error,"Unknown material parameter; extra:true requires a matching shader field");
            return true;
        }
        if (command == "text.style") {
            if (!text) return Fail(error, "TextRenderer is required");
            const double size = Number(args,"size",text->GetFontSize());
            if (size <= 0) return Fail(error,"Font size must be positive");
            auto color = text->GetColor();
            if (args.contains("color")) { float a[4]; Array(args,"color",a); color = {a[0],a[1],a[2],a[3]}; }
            if (args.contains("alpha")) { const double alpha = Number(args,"alpha",1); if (alpha < 0 || alpha > 1) return Fail(error,"Alpha must be 0..1"); color.w = static_cast<float>(alpha); }
            const auto font = args.value("font", std::string{});
            if (args.contains("font") && FontManager::GetFontHandleFromName(font) == 0) return Fail(error,"Font is not loaded");
            text->SetFontSize(static_cast<float>(size)); text->SetColor(color); if (args.contains("font")) text->SetFontName(font);
            return true;
        }
        if (command == "text.format") {
            if (!text) return Fail(error,"TextRenderer is required");
            const int decimals = args.value("decimals",0), width = args.value("width",0);
            if (decimals < 0 || decimals > 9 || width < 0 || width > 128) return Fail(error,"Invalid number formatting limits");
            const double number = Number(args,"value",0);
            std::ostringstream formatted; formatted << args.value("prefix",std::string{}) << std::fixed << std::setprecision(decimals) << std::setw(width) << std::setfill(args.value("zeroPad",false) ? '0' : ' ') << number << args.value("suffix",std::string{});
            text->SetText(formatted.str()); return true;
        }
        if (command == "camera.configure") {
            const auto mode = args.value("mode",std::string("2d"));
            if (mode != "2d" && mode != "3d") return Fail(error,"Camera mode must be 2d or 3d");
            const auto &values = args.at("values");
            if (!values.is_object() || values.empty()) return Fail(error,"Camera values must be a nonempty object");
            for (const auto *key : {"width_","height_","aspectRatio_","orthoSize_"}) if (values.contains(key) && Number(values,key,0) <= 0) return Fail(error,"Camera dimensions must be positive");
            if (values.contains("fovY_") && (Number(values,"fovY_",0) <= 0 || Number(values,"fovY_",0) >= 3.141592653589793)) return Fail(error,"FOV must be in (0,pi) radians");
            const auto type = mode == "2d" ? "Camera2D" : "Camera3D";
            if (!Component(object,type) && args.value("create",false)) {
                auto *added = object->AddComponent(CreateObjectComponentByType(type));
                if (!added) return Fail(error,"Camera creation failed");
                if (!Configure(object,added,args.at("values"),error)) { object->RemoveComponent(added); return false; }
                return true;
            }
            return Configure(object,Component(object,type),args.at("values"),error);
        }
        if (command == "render_target.configure") {
            const auto kind = args.at("kind").get<std::string>();
            if (kind != "screen_buffer" && kind != "window" && kind != "overlay") return Fail(error,"Unknown render target kind");
            const double width = Number(args,"width",1280), height = Number(args,"height",720);
            if (width < 1 || height < 1 || width > 16384 || height > 16384 || std::floor(width) != width || std::floor(height) != height) return Fail(error,"Render target size must be integer 1..16384");
            const auto name = args.value("name",object->GetName());
            if (args.contains("syncTransform") && !args.at("syncTransform").is_boolean()) return Fail(error,"syncTransform must be boolean");
            if (kind == "screen_buffer") {
                auto *buffer = object->GetComponent<ScreenBufferObject>(); if (!buffer && args.value("create",false)) buffer = object->AddComponent<ScreenBufferObject>();
                if (!buffer) return Fail(error,"ScreenBufferObject is required"); buffer->SetSize(static_cast<uint32_t>(width),static_cast<uint32_t>(height)); buffer->SetName(name);
            } else {
                IWindowObjectComponent *window = kind == "window" ? static_cast<IWindowObjectComponent *>(object->GetComponent<NormalWindowObject>()) : static_cast<IWindowObjectComponent *>(object->GetComponent<OverlayWindowObject>());
                if (!window && args.value("create",false)) window = kind == "window" ? static_cast<IWindowObjectComponent *>(object->AddComponent<NormalWindowObject>()) : static_cast<IWindowObjectComponent *>(object->AddComponent<OverlayWindowObject>());
                if (!window) return Fail(error,"Window component is required"); window->SetSize(static_cast<uint32_t>(width),static_cast<uint32_t>(height)); window->SetTitle(name);
                if (args.contains("syncTransform")) window->SetSyncWithTransform(args.at("syncTransform").get<bool>());
            }
            return true;
        }
    } catch (const std::exception &exception) { return Fail(error,std::string("Invalid rendering arguments: ") + exception.what()); }
    return Fail(error,"Unsupported rendering command");
}
} // namespace KashipanEngine::ShortText
