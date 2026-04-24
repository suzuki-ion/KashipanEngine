#include "Scene/SceneBase.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneContext.h"
#include "Scene/Components/SceneDefaultVariables.h"
#include "Utilities/FileIO/JSON.h"

#include "Objects/Components/2D/Material2D.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/GameObjects/2D/Ellipse.h"
#include "Objects/GameObjects/2D/Line2D.h"
#include "Objects/GameObjects/2D/Rect.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/GameObjects/2D/Triangle2D.h"
#include "Objects/GameObjects/3D/Box.h"
#include "Objects/GameObjects/3D/Line3D.h"
#include "Objects/GameObjects/3D/Model.h"
#include "Objects/GameObjects/3D/Plane3D.h"
#include "Objects/GameObjects/3D/Sphere.h"
#include "Objects/GameObjects/3D/Triangle3D.h"

#include <algorithm>
#include <cstring>

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

namespace {

JSON ToJson(const Vector3 &v) {
    return JSON{ { "x", v.x }, { "y", v.y }, { "z", v.z } };
}

JSON ToJson(const Vector4 &v) {
    return JSON{ { "x", v.x }, { "y", v.y }, { "z", v.z }, { "w", v.w } };
}

Vector3 Vector3FromJson(const JSON &j, const Vector3 &def = Vector3{ 0.0f, 0.0f, 0.0f }) {
    if (!j.is_object()) return def;
    return Vector3(
        j.value("x", def.x),
        j.value("y", def.y),
        j.value("z", def.z));
}

Vector4 Vector4FromJson(const JSON &j, const Vector4 &def = Vector4{ 0.0f, 0.0f, 0.0f, 0.0f }) {
    if (!j.is_object()) return def;
    return Vector4(
        j.value("x", def.x),
        j.value("y", def.y),
        j.value("z", def.z),
        j.value("w", def.w));
}

const char *GetObject2DTypeName(const Object2DBase *obj) {
    if (dynamic_cast<const Sprite *>(obj)) return "Sprite";
    if (dynamic_cast<const Rect *>(obj)) return "Rect";
    if (dynamic_cast<const Triangle2D *>(obj)) return "Triangle2D";
    if (dynamic_cast<const Ellipse *>(obj)) return "Ellipse";
    if (dynamic_cast<const Line2D *>(obj)) return "Line2D";
    return nullptr;
}

const char *GetObject3DTypeName(const Object3DBase *obj) {
    if (dynamic_cast<const Box *>(obj)) return "Box";
    if (dynamic_cast<const Sphere *>(obj)) return "Sphere";
    if (dynamic_cast<const Plane3D *>(obj)) return "Plane3D";
    if (dynamic_cast<const Triangle3D *>(obj)) return "Triangle3D";
    if (dynamic_cast<const Line3D *>(obj)) return "Line3D";
    if (dynamic_cast<const Model *>(obj)) return "Model";
    return nullptr;
}

#if defined(USE_IMGUI)
void ShowTextureSelectorForMaterial2D(Material2D *mat) {
    if (!mat) return;

    const auto entries = TextureManager::GetLoadedTextureListEntries();
    const auto currentPath = TextureManager::GetTextureAssetPath(mat->GetTexture());
    const char *preview = currentPath.empty() ? "(none)" : currentPath.c_str();

    if (ImGui::BeginCombo("Texture##Material2D", preview)) {
        if (ImGui::Selectable("(none)", currentPath.empty())) {
            mat->SetTexture(TextureManager::kInvalidHandle);
        }
        for (const auto &e : entries) {
            const bool selected = (e.assetPath == currentPath);
            if (ImGui::Selectable(e.assetPath.c_str(), selected)) {
                mat->SetTexture(e.handle);
            }
        }
        ImGui::EndCombo();
    }
}

void ShowTextureSelectorForMaterial3D(Material3D *mat) {
    if (!mat) return;

    const auto entries = TextureManager::GetLoadedTextureListEntries();
    const auto currentPath = TextureManager::GetTextureAssetPath(mat->GetTexture());
    const char *preview = currentPath.empty() ? "(none)" : currentPath.c_str();

    if (ImGui::BeginCombo("Texture##Material3D", preview)) {
        if (ImGui::Selectable("(none)", currentPath.empty())) {
            mat->SetTexture(TextureManager::kInvalidHandle);
        }
        for (const auto &e : entries) {
            const bool selected = (e.assetPath == currentPath);
            if (ImGui::Selectable(e.assetPath.c_str(), selected)) {
                mat->SetTexture(e.handle);
            }
        }
        ImGui::EndCombo();
    }
}

void ShowModelSelectorForObjectCreation(ModelManager::ModelHandle &selectedHandle) {
    const auto entries = ModelManager::GetLoadedModelListEntries();
    const auto current = std::find_if(entries.begin(), entries.end(), [&](const auto &e) { return e.handle == selectedHandle; });
    const char *preview = (current != entries.end()) ? current->assetPath.c_str() : "(none)";

    if (ImGui::BeginCombo("Model##ObjectAdd", preview)) {
        if (ImGui::Selectable("(none)", selectedHandle == ModelManager::kInvalidHandle)) {
            selectedHandle = ModelManager::kInvalidHandle;
        }
        for (const auto &e : entries) {
            const bool selected = (e.handle == selectedHandle);
            if (ImGui::Selectable(e.assetPath.c_str(), selected)) {
                selectedHandle = e.handle;
            }
        }
        ImGui::EndCombo();
    }
}
#endif

} // namespace

void SceneBase::SetEnginePointers(
    Passkey<GameEngine>,
    AudioManager *audioManager,
    ModelManager *modelManager,
    SamplerManager *samplerManager,
    TextureManager *textureManager,
    Input *input,
    InputCommand *inputCommand) {
    sAudioManager = audioManager;
    sModelManager = modelManager;
    sSamplerManager = samplerManager;
    sTextureManager = textureManager;
    sInput = input;
    sInputCommand = inputCommand;
}

SceneBase::SceneBase(const std::string &sceneName)
    : name_(sceneName) {
#if defined(USE_IMGUI)
    strncpy_s(objectJsonPath_.data(), objectJsonPath_.size(), "Assets/KashipanEngine/SceneObjects.json", _TRUNCATE);
#endif

    sceneContext_ = std::make_unique<SceneContext>(Passkey<SceneBase>{}, this);
    // デフォルトのシーン変数コンポーネントを追加
    auto defaultVarsComp = std::make_unique<SceneDefaultVariables>();
    auto *ptr = defaultVarsComp.get();
    AddSceneComponent(std::move(defaultVarsComp));
    ptr->SetSceneComponents([this](std::unique_ptr<ISceneComponent> comp) {
        return AddSceneComponent(std::move(comp));
    });
}

SceneBase::~SceneBase() {
    ClearObjects2D();
    ClearObjects3D();
    ClearSceneComponents();
}

void SceneBase::Update() {
    for (auto &o : objects2D_) {
        if (o) o->Update();
    }
    for (auto &o : objects3D_) {
        if (o) o->Update();
    }

    {
        std::vector<ISceneComponent *> sorted;
        sorted.reserve(sceneComponents_.size());
        for (auto &c : sceneComponents_) {
            if (c) sorted.push_back(c.get());
        }
        std::stable_sort(sorted.begin(), sorted.end(), [](const ISceneComponent *a, const ISceneComponent *b) {
            return a->GetUpdatePriority() < b->GetUpdatePriority();
        });
        for (auto *c : sorted) c->Update();
    }

    OnUpdate();
}

#if defined(USE_IMGUI)
void SceneBase::ShowImGui() {
    Object2DBase *remove2D = nullptr;
    Object3DBase *remove3D = nullptr;

    ImGui::Begin("Scene Objects");
    ImGui::TextUnformatted("Scene");
    ImGui::SameLine();
    ImGui::TextUnformatted(name_.c_str());

    if (ImGui::CollapsingHeader("Objects2D", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto &o : objects2D_) {
            if (!o) continue;
            const std::string &objName = o->GetName();
            if (ImGui::TreeNodeEx(static_cast<void *>(o.get()), ImGuiTreeNodeFlags_None, "%s", objName.c_str())) {
                if (GetObject2DTypeName(o.get()) != nullptr) {
                    if (ImGui::Button("Delete##Object2D")) {
                        remove2D = o.get();
                    }
                }
                if (auto *mat = o->GetComponent2D<Material2D>()) {
                    ShowTextureSelectorForMaterial2D(mat);
                }
                o->ShowImGui();
                ImGui::TreePop();
            }
        }
    }

    if (ImGui::CollapsingHeader("Objects3D", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto &o : objects3D_) {
            if (!o) continue;
            const std::string &objName = o->GetName();
            if (ImGui::TreeNodeEx(static_cast<void *>(o.get()), ImGuiTreeNodeFlags_None, "%s", objName.c_str())) {
                if (GetObject3DTypeName(o.get()) != nullptr) {
                    if (ImGui::Button("Delete##Object3D")) {
                        remove3D = o.get();
                    }
                }
                if (auto *mat = o->GetComponent3D<Material3D>()) {
                    ShowTextureSelectorForMaterial3D(mat);
                }
                o->ShowImGui();
                ImGui::TreePop();
            }
        }
    }

    if (ImGui::CollapsingHeader("Object Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
        constexpr const char *kDimensionItems[] = { "2D", "3D" };
        constexpr const char *kType2DItems[] = { "Sprite", "Rect", "Triangle2D", "Ellipse", "Line2D" };
        constexpr const char *kType3DItems[] = { "Box", "Sphere", "Plane3D", "Triangle3D", "Line3D", "Model" };

        ImGui::InputText("JSON Path", objectJsonPath_.data(), objectJsonPath_.size());
        if (ImGui::Button("Save Objects")) {
            (void)SaveObjectsToJson(objectJsonPath_.data());
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Objects")) {
            (void)LoadObjectsFromJson(objectJsonPath_.data());
        }

        ImGui::Separator();
        ImGui::InputText("Object Name", objectAddName_.data(), objectAddName_.size());
        ImGui::Combo("Dimension", &objectAddDimension_, kDimensionItems, static_cast<int>(std::size(kDimensionItems)));

        if (objectAddDimension_ == 0) {
            ImGui::Combo("2D Type", &objectAddType2D_, kType2DItems, static_cast<int>(std::size(kType2DItems)));
            if (ImGui::Button("Add 2D Object")) {
                std::unique_ptr<Object2DBase> obj;
                switch (objectAddType2D_) {
                case 0: obj = std::make_unique<Sprite>(); break;
                case 1: obj = std::make_unique<Rect>(); break;
                case 2: obj = std::make_unique<Triangle2D>(); break;
                case 3: obj = std::make_unique<Ellipse>(); break;
                case 4: obj = std::make_unique<Line2D>(); break;
                default: break;
                }
                if (obj) {
                    if (objectAddName_[0] != '\0') {
                        obj->SetName(objectAddName_.data());
                    }
                    auto *vars = GetSceneComponent<SceneDefaultVariables>();
                    if (vars && vars->GetScreenBuffer2D()) {
                        obj->AttachToRenderer(vars->GetScreenBuffer2D(), "Object2D.DoubleSidedCulling.BlendNormal");
                    } else if (vars && vars->GetMainWindow()) {
                        obj->AttachToRenderer(vars->GetMainWindow(), "Object2D.DoubleSidedCulling.BlendNormal");
                    }
                    (void)AddObject2D(std::move(obj));
                }
            }
        } else {
            ImGui::Combo("3D Type", &objectAddType3D_, kType3DItems, static_cast<int>(std::size(kType3DItems)));
            if (objectAddType3D_ == 5) {
                ShowModelSelectorForObjectCreation(objectAddModelHandle_);
            }

            if (ImGui::Button("Add 3D Object")) {
                std::unique_ptr<Object3DBase> obj;
                switch (objectAddType3D_) {
                case 0: obj = std::make_unique<Box>(); break;
                case 1: obj = std::make_unique<Sphere>(); break;
                case 2: obj = std::make_unique<Plane3D>(); break;
                case 3: obj = std::make_unique<Triangle3D>(); break;
                case 4: obj = std::make_unique<Line3D>(1); break;
                case 5:
                    if (objectAddModelHandle_ != ModelManager::kInvalidHandle) {
                        obj = std::make_unique<Model>(objectAddModelHandle_);
                    }
                    break;
                default: break;
                }
                if (obj) {
                    if (objectAddName_[0] != '\0') {
                        obj->SetName(objectAddName_.data());
                    }
                    if (auto *vars = GetSceneComponent<SceneDefaultVariables>()) {
                        auto *screenBuffer3D = vars->GetScreenBuffer3D();
                        if (screenBuffer3D) {
                            obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
                        }
                        auto *shadowMapBuffer = vars->GetShadowMapBuffer();
                        if (shadowMapBuffer) {
                            obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
                        }
                    }
                    (void)AddObject3D(std::move(obj));
                }
            }
        }
    }

    ImGui::End();

    if (remove2D) {
        (void)RemoveObject2D(remove2D);
    }
    if (remove3D) {
        (void)RemoveObject3D(remove3D);
    }

    ImGui::Begin("Scene Components");
    if (ImGui::CollapsingHeader("Scene Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto &c : sceneComponents_) {
            if (!c) continue;
            const std::string &compType = c->GetComponentType();
            if (ImGui::TreeNode(compType.c_str())) {
                c->ShowImGui();
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}
#endif

bool SceneBase::SaveObjectsToJson(const std::string &filePath) const {
    JSON root;
    root["scene"] = name_;
    root["objects2D"] = JSON::array();
    root["objects3D"] = JSON::array();

    for (const auto &o : objects2D_) {
        if (!o) continue;
        const char *typeName = GetObject2DTypeName(o.get());
        if (!typeName) continue;

        JSON item;
        item["type"] = typeName;
        item["name"] = o->GetName();

        if (auto *tr = o->GetComponent2D<Transform2D>()) {
            item["transform"] = {
                { "translate", ToJson(tr->GetTranslate()) },
                { "rotate", ToJson(tr->GetRotate()) },
                { "scale", ToJson(tr->GetScale()) }
            };
        }

        if (auto *mat = o->GetComponent2D<Material2D>()) {
            const auto texturePath = TextureManager::GetTextureAssetPath(mat->GetTexture());
            item["material"] = {
                { "color", ToJson(mat->GetColor()) },
                { "uvTranslate", ToJson(mat->GetUVTransform().translate) },
                { "uvRotate", ToJson(mat->GetUVTransform().rotate) },
                { "uvScale", ToJson(mat->GetUVTransform().scale) },
                { "texture", texturePath },
                { "samplerHandle", static_cast<std::uint32_t>(mat->GetSampler()) }
            };
        }

        root["objects2D"].push_back(std::move(item));
    }

    for (const auto &o : objects3D_) {
        if (!o) continue;
        const char *typeName = GetObject3DTypeName(o.get());
        if (!typeName) continue;

        JSON item;
        item["type"] = typeName;
        item["name"] = o->GetName();

        if (auto *tr = o->GetComponent3D<Transform3D>()) {
            item["transform"] = {
                { "translate", ToJson(tr->GetTranslate()) },
                { "rotate", ToJson(tr->GetRotate()) },
                { "scale", ToJson(tr->GetScale()) }
            };
        }

        if (auto *mat = o->GetComponent3D<Material3D>()) {
            const auto texturePath = TextureManager::GetTextureAssetPath(mat->GetTexture());
            item["material"] = {
                { "enableLighting", mat->IsLightingEnabled() },
                { "enableShadowMapProjection", mat->IsShadowMapProjectionEnabled() },
                { "color", ToJson(mat->GetColor()) },
                { "uvTranslate", ToJson(mat->GetUVTransform().translate) },
                { "uvRotate", ToJson(mat->GetUVTransform().rotate) },
                { "uvScale", ToJson(mat->GetUVTransform().scale) },
                { "shininess", mat->GetShininess() },
                { "specularColor", ToJson(mat->GetSpecularColor()) },
                { "texture", texturePath },
                { "samplerHandle", static_cast<std::uint32_t>(mat->GetSampler()) }
            };
        }

        root["objects3D"].push_back(std::move(item));
    }

    return SaveJSON(root, filePath, 4);
}

bool SceneBase::LoadObjectsFromJson(const std::string &filePath) {
    const JSON root = LoadJSON(filePath);
    if (root.is_discarded() || !root.is_object()) return false;

    if (root.contains("objects2D") && root["objects2D"].is_array()) {
        for (const auto &item : root["objects2D"]) {
            if (!item.is_object()) continue;

            const std::string type = item.value("type", "");
            std::unique_ptr<Object2DBase> obj;

            if (type == "Sprite") obj = std::make_unique<Sprite>();
            else if (type == "Rect") obj = std::make_unique<Rect>();
            else if (type == "Triangle2D") obj = std::make_unique<Triangle2D>();
            else if (type == "Ellipse") obj = std::make_unique<Ellipse>();
            else if (type == "Line2D") obj = std::make_unique<Line2D>();
            else continue;

            obj->SetName(item.value("name", type));

            auto *vars = GetSceneComponent<SceneDefaultVariables>();
            if (vars && vars->GetScreenBuffer2D()) {
                obj->AttachToRenderer(vars->GetScreenBuffer2D(), "Object2D.DoubleSidedCulling.BlendNormal");
            } else if (vars && vars->GetMainWindow()) {
                obj->AttachToRenderer(vars->GetMainWindow(), "Object2D.DoubleSidedCulling.BlendNormal");
            }

            if (item.contains("transform") && item["transform"].is_object()) {
                const auto &trJ = item["transform"];
                if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                    tr->SetTranslate(Vector3FromJson(trJ.value("translate", JSON::object())));
                    tr->SetRotate(Vector3FromJson(trJ.value("rotate", JSON::object())));
                    tr->SetScale(Vector3FromJson(trJ.value("scale", JSON::object()), Vector3{ 1.0f, 1.0f, 1.0f }));
                }
            }

            if (item.contains("material") && item["material"].is_object()) {
                const auto &matJ = item["material"];
                if (auto *mat = obj->GetComponent2D<Material2D>()) {
                    mat->SetColor(Vector4FromJson(matJ.value("color", JSON::object()), Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));

                    Material2D::UVTransform uv{};
                    uv.translate = Vector3FromJson(matJ.value("uvTranslate", JSON::object()));
                    uv.rotate = Vector3FromJson(matJ.value("uvRotate", JSON::object()));
                    uv.scale = Vector3FromJson(matJ.value("uvScale", JSON::object()), Vector3{ 1.0f, 1.0f, 1.0f });
                    mat->SetUVTransform(uv);

                    TextureManager::TextureHandle texture = TextureManager::kInvalidHandle;
                    if (matJ.contains("texture") && matJ["texture"].is_string()) {
                        texture = TextureManager::GetTextureFromAssetPath(matJ["texture"].get<std::string>());
                    } else {
                        texture = static_cast<TextureManager::TextureHandle>(matJ.value("textureHandle", static_cast<std::uint32_t>(TextureManager::kInvalidHandle)));
                    }
                    mat->SetTexture(texture);
                    mat->SetSampler(static_cast<SamplerManager::SamplerHandle>(matJ.value("samplerHandle", static_cast<std::uint32_t>(SamplerManager::kInvalidHandle))));
                }
            }

            (void)AddObject2D(std::move(obj));
        }
    }

    if (root.contains("objects3D") && root["objects3D"].is_array()) {
        for (const auto &item : root["objects3D"]) {
            if (!item.is_object()) continue;

            const std::string type = item.value("type", "");
            std::unique_ptr<Object3DBase> obj;

            if (type == "Box") obj = std::make_unique<Box>();
            else if (type == "Sphere") obj = std::make_unique<Sphere>();
            else if (type == "Plane3D") obj = std::make_unique<Plane3D>();
            else if (type == "Triangle3D") obj = std::make_unique<Triangle3D>();
            else if (type == "Line3D") obj = std::make_unique<Line3D>(1);
            else continue;

            obj->SetName(item.value("name", type));

            if (auto *vars = GetSceneComponent<SceneDefaultVariables>(); vars && vars->GetScreenBuffer3D()) {
                obj->AttachToRenderer(vars->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
            }

            if (item.contains("transform") && item["transform"].is_object()) {
                const auto &trJ = item["transform"];
                if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(Vector3FromJson(trJ.value("translate", JSON::object())));
                    tr->SetRotate(Vector3FromJson(trJ.value("rotate", JSON::object())));
                    tr->SetScale(Vector3FromJson(trJ.value("scale", JSON::object()), Vector3{ 1.0f, 1.0f, 1.0f }));
                }
            }

            if (item.contains("material") && item["material"].is_object()) {
                const auto &matJ = item["material"];
                if (auto *mat = obj->GetComponent3D<Material3D>()) {
                    mat->SetEnableLighting(matJ.value("enableLighting", true));
                    mat->SetEnableShadowMapProjection(matJ.value("enableShadowMapProjection", true));
                    mat->SetColor(Vector4FromJson(matJ.value("color", JSON::object()), Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));

                    Material3D::UVTransform uv{};
                    uv.translate = Vector3FromJson(matJ.value("uvTranslate", JSON::object()));
                    uv.rotate = Vector3FromJson(matJ.value("uvRotate", JSON::object()));
                    uv.scale = Vector3FromJson(matJ.value("uvScale", JSON::object()), Vector3{ 1.0f, 1.0f, 1.0f });
                    mat->SetUVTransform(uv);
                    mat->SetShininess(matJ.value("shininess", 32.0f));
                    mat->SetSpecularColor(Vector4FromJson(matJ.value("specularColor", JSON::object()), Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }));

                    TextureManager::TextureHandle texture = TextureManager::kInvalidHandle;
                    if (matJ.contains("texture") && matJ["texture"].is_string()) {
                        texture = TextureManager::GetTextureFromAssetPath(matJ["texture"].get<std::string>());
                    } else {
                        texture = static_cast<TextureManager::TextureHandle>(matJ.value("textureHandle", static_cast<std::uint32_t>(TextureManager::kInvalidHandle)));
                    }
                    mat->SetTexture(texture);
                    mat->SetSampler(static_cast<SamplerManager::SamplerHandle>(matJ.value("samplerHandle", static_cast<std::uint32_t>(SamplerManager::kInvalidHandle))));
                }
            }

            (void)AddObject3D(std::move(obj));
        }
    }

    return true;
}

void SceneBase::RebuildObject2DIndices() {
    objects2DIndexByPointer_.clear();
    objects2DIndexByName_.clear();
    for (size_t i = 0; i < objects2D_.size(); ++i) {
        if (!objects2D_[i]) continue;
        objects2DIndexByPointer_.emplace(objects2D_[i].get(), i);
        objects2DIndexByName_.emplace(objects2D_[i]->GetName(), i);
    }
}

void SceneBase::RebuildObject3DIndices() {
    objects3DIndexByPointer_.clear();
    objects3DIndexByName_.clear();
    for (size_t i = 0; i < objects3D_.size(); ++i) {
        if (!objects3D_[i]) continue;
        objects3DIndexByPointer_.emplace(objects3D_[i].get(), i);
        objects3DIndexByName_.emplace(objects3D_[i]->GetName(), i);
    }
}

bool SceneBase::AddObject2D(std::unique_ptr<Object2DBase> obj) {
    if (!obj) return false;
    objects2D_.push_back(std::move(obj));

    const size_t idx = objects2D_.size() - 1;
    if (objects2D_[idx]) {
        objects2DIndexByPointer_.emplace(objects2D_[idx].get(), idx);
        objects2DIndexByName_.emplace(objects2D_[idx]->GetName(), idx);
    }
    return true;
}

bool SceneBase::AddObject3D(std::unique_ptr<Object3DBase> obj) {
    if (!obj) return false;
    objects3D_.push_back(std::move(obj));

    const size_t idx = objects3D_.size() - 1;
    if (objects3D_[idx]) {
        objects3DIndexByPointer_.emplace(objects3D_[idx].get(), idx);
        objects3DIndexByName_.emplace(objects3D_[idx]->GetName(), idx);
    }
    return true;
}

bool SceneBase::RemoveObject2D(Object2DBase *obj) {
    if (!obj) return false;

    bool found = false;
    auto range = objects2DIndexByPointer_.equal_range(obj);
    for (auto it = range.first; it != range.second; ++it) {
        const size_t idx = it->second;
        if (idx < objects2D_.size() && objects2D_[idx] && objects2D_[idx].get() == obj) {
            objects2D_.erase(objects2D_.begin() + static_cast<std::ptrdiff_t>(idx));
            found = true;
            break;
        }
    }

    if (!found) return false;
    RebuildObject2DIndices();
    return true;
}

bool SceneBase::RemoveObject3D(Object3DBase *obj) {
    if (!obj) return false;

    bool found = false;
    auto range = objects3DIndexByPointer_.equal_range(obj);
    for (auto it = range.first; it != range.second; ++it) {
        const size_t idx = it->second;
        if (idx < objects3D_.size() && objects3D_[idx] && objects3D_[idx].get() == obj) {
            objects3D_.erase(objects3D_.begin() + static_cast<std::ptrdiff_t>(idx));
            found = true;
            break;
        }
    }

    if (!found) return false;
    RebuildObject3DIndices();
    return true;
}

void SceneBase::ClearObjects2D() {
    objects2D_.clear();
    objects2DIndexByPointer_.clear();
    objects2DIndexByName_.clear();
}

void SceneBase::ClearObjects3D() {
    objects3D_.clear();
    objects3DIndexByPointer_.clear();
    objects3DIndexByName_.clear();
}

void SceneBase::ChangeToNextScene() {
    if (sceneManager_ && !nextSceneName_.empty()) {
        sceneManager_->ChangeScene(nextSceneName_);
    }
}

bool SceneBase::AddSceneComponent(std::unique_ptr<ISceneComponent> comp) {
    if (!comp) return false;

    const size_t maxCount = comp->GetMaxComponentCountPerScene();
    const size_t existingCount = HasSceneComponents(comp->GetComponentType());
    if (existingCount >= maxCount) return false;

    if (sceneContext_) {
        comp->SetOwnerContext(sceneContext_.get());
    }

    comp->Initialize();

    sceneComponents_.push_back(std::move(comp));

    const size_t idx = sceneComponents_.size() - 1;
    if (sceneComponents_[idx]) {
        sceneComponentsIndexByName_.emplace(sceneComponents_[idx]->GetComponentType(), idx);
        sceneComponentsIndexByType_.emplace(std::type_index(typeid(*sceneComponents_[idx])), idx);
    }

    return true;
}

bool SceneBase::RemoveSceneComponent(ISceneComponent *comp) {
    if (!comp) return false;
    auto it = std::find_if(sceneComponents_.begin(), sceneComponents_.end(), [&](const auto &p) { return p.get() == comp; });
    if (it == sceneComponents_.end()) return false;

    if (*it) {
        (*it)->Finalize();
    }

    sceneComponents_.erase(it);

    sceneComponentsIndexByName_.clear();
    sceneComponentsIndexByType_.clear();
    for (size_t i = 0; i < sceneComponents_.size(); ++i) {
        if (!sceneComponents_[i]) continue;
        sceneComponentsIndexByName_.emplace(sceneComponents_[i]->GetComponentType(), i);
        sceneComponentsIndexByType_.emplace(std::type_index(typeid(*sceneComponents_[i])), i);
    }

    return true;
}

void SceneBase::ClearSceneComponents() {
    for (auto &c : sceneComponents_) {
        if (!c) continue;
        c->Finalize();
    }
    sceneComponents_.clear();
    sceneComponentsIndexByName_.clear();
    sceneComponentsIndexByType_.clear();
}

void SceneBase::AddSceneVariable(const std::string &key, const std::any &value) {
    if (!sceneManager_) return;
    sceneManager_->AddSceneVariable(key, value);
}

const MyStd::AnyUnorderedMap &SceneBase::GetSceneVariables() const {
    static MyStd::AnyUnorderedMap emptyMap;
    if (!sceneManager_) return emptyMap;
    return sceneManager_->GetSceneVariables();
}

} // namespace KashipanEngine
