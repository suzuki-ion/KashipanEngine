#include "Objects/Components/ScriptComponent.h"

#include <cstdint>
#include <functional>

#include <angelscript.h>
#include <add_on/scriptbuilder/scriptbuilder.h>
#include <add_on/scripthelper/scripthelper.h>

#include "Debug/Logger.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Scene/Components/Script/SceneScriptEngine.h"
#include "Scene/Components/Script/ScriptBindings.h"

namespace KashipanEngine {

namespace {

constexpr const char *kSerializeFieldMetadata = "SerializeField";
constexpr const char *kBehaviorInterfaceName = "ScriptComponentBehavior";

/// @brief メタデータ文字列の前後空白を取り除く
std::string TrimMetadata(const std::string &metadata) {
    const auto first = metadata.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = metadata.find_last_not_of(" \t");
    return metadata.substr(first, last - first + 1);
}

/// @brief メタデータのリストに SerializeField が含まれるかを確認する
bool HasSerializeFieldMetadata(const std::vector<std::string> &metadataList) {
    for (const auto &metadata : metadataList) {
        if (TrimMetadata(metadata) == kSerializeFieldMetadata) return true;
    }
    return false;
}

/// @brief `#include "path.as"` を解決するコールバック
/// @details includeで指定されたパスが相対パスの場合、includeディレクティブを書いたファイル（from）
///          と同じディレクトリからの相対パスとして解決する。絶対パス指定の場合はそのまま使用する
int ResolveIncludePath(const char *include, const char *from, CScriptBuilder *builder, void *) {
    if (!include || !builder) return -1;

    std::string includePath = include;
    const bool isAbsolute = includePath.size() >= 2 &&
        (includePath[1] == ':' || includePath[0] == '/' || includePath[0] == '\\');

    if (!isAbsolute && from) {
        const std::string fromPath = from;
        const auto slashPos = fromPath.find_last_of("/\\");
        if (slashPos != std::string::npos) {
            includePath = fromPath.substr(0, slashPos + 1) + includePath;
        }
    }
    return builder->AddSectionFromFile(includePath.c_str());
}

} // namespace

/// @brief コライダーへ設定した衝突コールバックのフック情報
struct ScriptComponent::ColliderHooks {
    struct Entry {
        ICollider *collider = nullptr;
        std::function<void(const HitInfo3D &)> prevEnter3D;
        std::function<void(const HitInfo3D &)> prevStay3D;
        std::function<void(const HitInfo3D &)> prevExit3D;
        std::function<void(const HitInfo2D &)> prevEnter2D;
        std::function<void(const HitInfo2D &)> prevStay2D;
        std::function<void(const HitInfo2D &)> prevExit2D;
    };
    std::vector<Entry> entries;
};

ScriptComponent::~ScriptComponent() {
    // コライダー側のコールバックはaliveToken_の失効により無効化されるため、ここでの解除は不要
    ReleaseScript();
}

std::unique_ptr<IObjectComponent> ScriptComponent::Clone() const {
    auto ptr = std::make_unique<ScriptComponent>();
    ptr->scriptPath_ = scriptPath_;
    ptr->pendingFieldValues_ = CaptureFieldValuesToJson();
    return ptr;
}

SceneScriptEngine *ScriptComponent::GetSceneScriptEngine() const {
    auto *sceneContext = GetOwnerSceneContext();
    return sceneContext ? sceneContext->GetComponent<SceneScriptEngine>() : nullptr;
}

SceneScriptEngine *ScriptComponent::GetOrAddSceneScriptEngine() const {
    auto *sceneContext = GetOwnerSceneContext();
    if (!sceneContext) return nullptr;
    auto *scriptEngine = sceneContext->GetComponent<SceneScriptEngine>();
    if (!scriptEngine) {
        scriptEngine = sceneContext->AddComponent<SceneScriptEngine>();
    }
    return scriptEngine;
}

void ScriptComponent::ReleaseScript() {
    if (behaviorObject_) {
        behaviorObject_->Release();
        behaviorObject_ = nullptr;
    }
    behaviorType_ = nullptr;
    startMethod_ = nullptr;
    updateMethod_ = nullptr;
    endMethod_ = nullptr;
    onCollisionEnterMethod_ = nullptr;
    onCollisionStayMethod_ = nullptr;
    onCollisionExitMethod_ = nullptr;
    serializedFields_.clear();

    if (context_) {
        context_->Release();
        context_ = nullptr;
    }

    if (!moduleName_.empty()) {
        auto *scriptEngine = GetSceneScriptEngine();
        asIScriptEngine *engine = scriptEngine ? scriptEngine->GetEngine() : nullptr;
        if (engine) {
            engine->DiscardModule(moduleName_.c_str());
        }
        moduleName_.clear();
    }
}

bool ScriptComponent::Reload() {
    // リロード後も編集済みの値を維持するため、破棄前に現在値を退避する
    pendingFieldValues_ = CaptureFieldValuesToJson();

    // 既にBehaviorが動作している場合は終了処理を呼んでから破棄する
    if (behaviorObject_) {
        CallMethod(endMethod_);
    }
    UnhookColliders();
    ReleaseScript();
    lastError_.clear();
    buildErrorMessages_.clear();

    if (scriptPath_.empty()) return false;

    auto *scriptEngine = GetOrAddSceneScriptEngine();
    asIScriptEngine *engine = scriptEngine ? scriptEngine->GetEngine() : nullptr;
    if (!engine) {
        lastError_ = "スクリプトエンジンが初期化されていません";
        return false;
    }

    const std::string moduleName = "ScriptComponent_" + std::to_string(reinterpret_cast<uintptr_t>(this));

    CScriptBuilder builder;
    if (builder.StartNewModule(engine, moduleName.c_str()) < 0) {
        lastError_ = "モジュールの作成に失敗しました";
        return false;
    }
    moduleName_ = moduleName;
    builder.SetIncludeCallback(ResolveIncludePath, nullptr);

    // ビルド中のコンパイルメッセージを収集し、失敗時にインスペクターへ表示できるようにする
    scriptEngine->BeginMessageCapture();
    const bool isSectionLoaded = builder.AddSectionFromFile(scriptPath_.c_str()) >= 0;
    const bool isBuilt = isSectionLoaded && builder.BuildModule() >= 0;
    auto buildMessages = scriptEngine->EndMessageCapture();

    if (!isBuilt) {
        lastError_ = isSectionLoaded
            ? "スクリプトのビルドに失敗しました: " + scriptPath_
            : "スクリプトファイルの読み込みに失敗しました: " + scriptPath_;
        buildErrorMessages_ = std::move(buildMessages);
        engine->DiscardModule(moduleName_.c_str());
        moduleName_.clear();
        return false;
    }

    stringTypeId_ = engine->GetTypeIdByDecl("string");
    vector2TypeId_ = engine->GetTypeIdByDecl("Vector2");
    vector3TypeId_ = engine->GetTypeIdByDecl("Vector3");
    vector4TypeId_ = engine->GetTypeIdByDecl("Vector4");
    quaternionTypeId_ = engine->GetTypeIdByDecl("Quaternion");

    context_ = engine->CreateContext();

    if (!CreateBehaviorInstance(engine, builder)) {
        ReleaseScript();
        return false;
    }

    CollectSerializedFields(builder);
    ApplyFieldValuesFromJson(pendingFieldValues_);
    return true;
}

bool ScriptComponent::CreateBehaviorInstance(asIScriptEngine *engine, CScriptBuilder &builder) {
    asIScriptModule *module = builder.GetModule();
    asITypeInfo *interfaceType = engine->GetTypeInfoByDecl(kBehaviorInterfaceName);
    if (!module || !interfaceType) {
        lastError_ = "スクリプトモジュールの取得に失敗しました";
        return false;
    }

    // ScriptComponentBehaviorを実装した最初のクラスを探す
    behaviorType_ = nullptr;
    const asUINT typeCount = module->GetObjectTypeCount();
    for (asUINT i = 0; i < typeCount; ++i) {
        asITypeInfo *type = module->GetObjectTypeByIndex(i);
        if (type && type->Implements(interfaceType)) {
            behaviorType_ = type;
            break;
        }
    }
    if (!behaviorType_) {
        lastError_ = std::string(kBehaviorInterfaceName) + " を実装したクラスが見つかりません: " + scriptPath_;
        return false;
    }

    const std::string factoryDecl = std::string(behaviorType_->GetName()) + " @" + behaviorType_->GetName() + "()";
    asIScriptFunction *factory = behaviorType_->GetFactoryByDecl(factoryDecl.c_str());
    if (!factory) {
        lastError_ = std::string("クラス ") + behaviorType_->GetName() + " のデフォルトコンストラクタが見つかりません";
        return false;
    }

    if (context_->Prepare(factory) < 0) {
        lastError_ = "コンストラクタの準備に失敗しました";
        return false;
    }
    int r;
    {
        ScriptExecutionScope scope(GetOwnerObjectContext(), GetOwnerSceneContext());
        r = context_->Execute();
    }
    if (r != asEXECUTION_FINISHED) {
        lastError_ = GetExceptionInfo(context_);
        Log("AngelScript: " + lastError_, LogSeverity::Error);
        return false;
    }

    behaviorObject_ = *static_cast<asIScriptObject **>(context_->GetAddressOfReturnValue());
    if (!behaviorObject_) {
        lastError_ = "Behaviorクラスのインスタンス生成に失敗しました";
        return false;
    }
    behaviorObject_->AddRef();

    startMethod_ = behaviorType_->GetMethodByDecl("void Start()");
    updateMethod_ = behaviorType_->GetMethodByDecl("void Update()");
    endMethod_ = behaviorType_->GetMethodByDecl("void End()");
    onCollisionEnterMethod_ = behaviorType_->GetMethodByDecl("void OnCollisionEnter(const HitInfo &in)");
    onCollisionStayMethod_ = behaviorType_->GetMethodByDecl("void OnCollisionStay(const HitInfo &in)");
    onCollisionExitMethod_ = behaviorType_->GetMethodByDecl("void OnCollisionExit(const HitInfo &in)");
    return true;
}

void ScriptComponent::CallMethod(asIScriptFunction *method) {
    if (!method || !context_ || !behaviorObject_) return;

    if (context_->Prepare(method) < 0) {
        lastError_ = "関数の準備に失敗しました";
        return;
    }
    context_->SetObject(behaviorObject_);
    ScriptExecutionScope scope(GetOwnerObjectContext(), GetOwnerSceneContext());
    const int r = context_->Execute();
    if (r != asEXECUTION_FINISHED) {
        lastError_ = GetExceptionInfo(context_);
        Log("AngelScript: " + lastError_, LogSeverity::Error);
    }
}

void ScriptComponent::CallCollisionMethod(asIScriptFunction *method, const Vector3 &normal, float penetration,
    EmptyObject *selfObject, EmptyObject *otherObject) {
    if (!method || !context_ || !behaviorObject_) return;

    ScriptHitInfo hitInfo;
    hitInfo.normal = normal;
    hitInfo.penetration = penetration;
    hitInfo.selfObject = selfObject;
    hitInfo.otherObject = otherObject;

    if (context_->Prepare(method) < 0) return;
    context_->SetObject(behaviorObject_);
    context_->SetArgObject(0, &hitInfo);
    ScriptExecutionScope scope(GetOwnerObjectContext(), GetOwnerSceneContext());
    const int r = context_->Execute();
    if (r != asEXECUTION_FINISHED) {
        lastError_ = GetExceptionInfo(context_);
        Log("AngelScript: " + lastError_, LogSeverity::Error);
    }
}

size_t ScriptComponent::CountColliders() const {
    auto *objectContext = GetOwnerObjectContext();
    if (!objectContext) return 0;
    size_t count = 0;
    for (const auto &pair : objectContext->GetAllComponents()) {
        if (dynamic_cast<ICollider *>(pair.first.get())) ++count;
    }
    return count;
}

void ScriptComponent::HookColliders() {
    UnhookColliders();
    auto *objectContext = GetOwnerObjectContext();
    if (!objectContext) return;
    if (!colliderHooks_) colliderHooks_ = std::make_shared<ColliderHooks>();

    // コールバックのラムダはコンポーネント破棄後に呼ばれる可能性があるため、
    // 生ポインタではなくaliveToken_経由で生存確認をしてから呼び出す
    const std::weak_ptr<ScriptComponent *> weakSelf(aliveToken_);
    const auto makeCallback3D = [weakSelf](std::function<void(const HitInfo3D &)> prev, asIScriptFunction *ScriptComponent::*methodMember) {
        return [weakSelf, prev = std::move(prev), methodMember](const HitInfo3D &hit) {
            if (prev) prev(hit);
            if (auto alive = weakSelf.lock()) {
                ScriptComponent *self = *alive;
                self->CallCollisionMethod(self->*methodMember, hit.normal, hit.penetration, hit.selfObject, hit.otherObject);
            }
        };
    };
    const auto makeCallback2D = [weakSelf](std::function<void(const HitInfo2D &)> prev, asIScriptFunction *ScriptComponent::*methodMember) {
        return [weakSelf, prev = std::move(prev), methodMember](const HitInfo2D &hit) {
            if (prev) prev(hit);
            if (auto alive = weakSelf.lock()) {
                ScriptComponent *self = *alive;
                self->CallCollisionMethod(self->*methodMember, hit.normal, hit.penetration, hit.selfObject, hit.otherObject);
            }
        };
    };

    for (const auto &pair : objectContext->GetAllComponents()) {
        auto *collider = dynamic_cast<ICollider *>(pair.first.get());
        if (!collider) continue;

        ColliderHooks::Entry entry;
        entry.collider = collider;
        entry.prevEnter3D = collider->GetOnCollisionEnter3D();
        entry.prevStay3D = collider->GetOnCollisionStay3D();
        entry.prevExit3D = collider->GetOnCollisionExit3D();
        entry.prevEnter2D = collider->GetOnCollisionEnter2D();
        entry.prevStay2D = collider->GetOnCollisionStay2D();
        entry.prevExit2D = collider->GetOnCollisionExit2D();

        collider->SetOnCollisionEnter3D(makeCallback3D(entry.prevEnter3D, &ScriptComponent::onCollisionEnterMethod_));
        collider->SetOnCollisionStay3D(makeCallback3D(entry.prevStay3D, &ScriptComponent::onCollisionStayMethod_));
        collider->SetOnCollisionExit3D(makeCallback3D(entry.prevExit3D, &ScriptComponent::onCollisionExitMethod_));
        collider->SetOnCollisionEnter2D(makeCallback2D(entry.prevEnter2D, &ScriptComponent::onCollisionEnterMethod_));
        collider->SetOnCollisionStay2D(makeCallback2D(entry.prevStay2D, &ScriptComponent::onCollisionStayMethod_));
        collider->SetOnCollisionExit2D(makeCallback2D(entry.prevExit2D, &ScriptComponent::onCollisionExitMethod_));

        colliderHooks_->entries.push_back(std::move(entry));
    }
}

void ScriptComponent::UnhookColliders() {
    if (!colliderHooks_) return;
    auto *objectContext = GetOwnerObjectContext();
    for (auto &entry : colliderHooks_->entries) {
        // コライダーが既に削除されている場合はポインタが無効なため触らない
        if (!objectContext || !objectContext->GetComponent(entry.collider)) continue;
        entry.collider->SetOnCollisionEnter3D(entry.prevEnter3D);
        entry.collider->SetOnCollisionStay3D(entry.prevStay3D);
        entry.collider->SetOnCollisionExit3D(entry.prevExit3D);
        entry.collider->SetOnCollisionEnter2D(entry.prevEnter2D);
        entry.collider->SetOnCollisionStay2D(entry.prevStay2D);
        entry.collider->SetOnCollisionExit2D(entry.prevExit2D);
    }
    colliderHooks_->entries.clear();
}

void ScriptComponent::Initialize() {
    if (Reload()) {
        HookColliders();
        CallMethod(startMethod_);
    }
}

void ScriptComponent::Finalize() {
    CallMethod(endMethod_);
    UnhookColliders();
    ReleaseScript();
}

void ScriptComponent::Update() {
    // 実行中に追加/削除されたコライダーへ追従するため、数が変わったらフックし直す
    const size_t hookedCount = colliderHooks_ ? colliderHooks_->entries.size() : 0;
    if (behaviorObject_ && CountColliders() != hookedCount) {
        HookColliders();
    }
    CallMethod(updateMethod_);
}

void ScriptComponent::CollectSerializedFields(CScriptBuilder &builder) {
    serializedFields_.clear();
    asIScriptModule *module = builder.GetModule();
    if (!module) return;

    // グローバル変数
    const asUINT varCount = module->GetGlobalVarCount();
    for (asUINT i = 0; i < varCount; ++i) {
        if (!HasSerializeFieldMetadata(builder.GetMetadataForVar(static_cast<int>(i)))) continue;

        const char *name = nullptr;
        int typeId = 0;
        if (module->GetGlobalVar(i, &name, nullptr, &typeId) < 0 || !name) continue;

        void *address = module->GetAddressOfGlobalVar(i);
        if (!address) continue;

        serializedFields_.push_back({ name, typeId, address });
    }

    // Behaviorクラスのメンバ変数
    if (behaviorType_ && behaviorObject_) {
        const int behaviorTypeId = behaviorType_->GetTypeId();
        const asUINT propertyCount = behaviorType_->GetPropertyCount();
        for (asUINT i = 0; i < propertyCount; ++i) {
            if (!HasSerializeFieldMetadata(builder.GetMetadataForTypeProperty(behaviorTypeId, static_cast<int>(i)))) continue;

            const char *name = nullptr;
            int typeId = 0;
            if (behaviorType_->GetProperty(i, &name, &typeId) < 0 || !name) continue;

            void *address = behaviorObject_->GetAddressOfProperty(i);
            if (!address) continue;

            serializedFields_.push_back({ name, typeId, address });
        }
    }
}

JSON ScriptComponent::CaptureFieldValuesToJson() const {
    if (serializedFields_.empty()) return pendingFieldValues_;

    JSON json = JSON::object();
    for (const auto &field : serializedFields_) {
        if (field.typeId == asTYPEID_BOOL) {
            json[field.name] = *static_cast<bool *>(field.address);
        } else if (field.typeId == asTYPEID_INT32) {
            json[field.name] = *static_cast<int32_t *>(field.address);
        } else if (field.typeId == asTYPEID_UINT32) {
            json[field.name] = *static_cast<uint32_t *>(field.address);
        } else if (field.typeId == asTYPEID_FLOAT) {
            json[field.name] = *static_cast<float *>(field.address);
        } else if (field.typeId == asTYPEID_DOUBLE) {
            json[field.name] = *static_cast<double *>(field.address);
        } else if (field.typeId == stringTypeId_) {
            json[field.name] = *static_cast<std::string *>(field.address);
        } else if (field.typeId == vector2TypeId_) {
            json[field.name] = ToJSON(*static_cast<Vector2 *>(field.address));
        } else if (field.typeId == vector3TypeId_) {
            json[field.name] = ToJSON(*static_cast<Vector3 *>(field.address));
        } else if (field.typeId == vector4TypeId_) {
            json[field.name] = ToJSON(*static_cast<Vector4 *>(field.address));
        } else if (field.typeId == quaternionTypeId_) {
            json[field.name] = ToJSON(*static_cast<Quaternion *>(field.address));
        }
    }
    return json;
}

void ScriptComponent::ApplyFieldValuesFromJson(const JSON &json) {
    if (!json.is_object()) return;

    for (auto &field : serializedFields_) {
        if (!json.contains(field.name)) continue;
        const JSON &value = json[field.name];
        try {
            if (field.typeId == asTYPEID_BOOL) {
                *static_cast<bool *>(field.address) = value.get<bool>();
            } else if (field.typeId == asTYPEID_INT32) {
                *static_cast<int32_t *>(field.address) = value.get<int32_t>();
            } else if (field.typeId == asTYPEID_UINT32) {
                *static_cast<uint32_t *>(field.address) = value.get<uint32_t>();
            } else if (field.typeId == asTYPEID_FLOAT) {
                *static_cast<float *>(field.address) = value.get<float>();
            } else if (field.typeId == asTYPEID_DOUBLE) {
                *static_cast<double *>(field.address) = value.get<double>();
            } else if (field.typeId == stringTypeId_) {
                *static_cast<std::string *>(field.address) = value.get<std::string>();
            } else if (field.typeId == vector2TypeId_) {
                *static_cast<Vector2 *>(field.address) = FromJSON<Vector2>(value);
            } else if (field.typeId == vector3TypeId_) {
                *static_cast<Vector3 *>(field.address) = FromJSON<Vector3>(value);
            } else if (field.typeId == vector4TypeId_) {
                *static_cast<Vector4 *>(field.address) = FromJSON<Vector4>(value);
            } else if (field.typeId == quaternionTypeId_) {
                *static_cast<Quaternion *>(field.address) = FromJSON<Quaternion>(value);
            }
        } catch (const JSON::exception &) {
            // 型が合わない保存値（スクリプト側の型変更後など）は無視して既定値のままにする
        }
    }
}

#if defined(USE_IMGUI)
void ScriptComponent::ShowImGui() {
    ImGuiCustom::EditValue("Script Path", scriptPath_);
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        if (Reload()) {
            HookColliders();
            CallMethod(startMethod_);
        }
    }

    ImGui::Text("Behavior: %s", behaviorType_ ? behaviorType_->GetName() : "(None)");
    if (behaviorObject_) {
        ImGui::Text("Start: %s / Update: %s / End: %s",
            startMethod_ ? "o" : "-", updateMethod_ ? "o" : "-", endMethod_ ? "o" : "-");
        ImGui::Text("OnCollision Enter: %s / Stay: %s / Exit: %s",
            onCollisionEnterMethod_ ? "o" : "-", onCollisionStayMethod_ ? "o" : "-", onCollisionExitMethod_ ? "o" : "-");
    }
    if (!lastError_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", lastError_.c_str());
        // ビルド失敗時はコンパイラの出力したメッセージを失敗表示の下に出す
        if (!buildErrorMessages_.empty()) {
            ImGui::Indent();
            for (const auto &message : buildErrorMessages_) {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.5f, 1.0f), "%s", message.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::Unindent();
        }
    }

    if (serializedFields_.empty()) return;

    ImGui::SeparatorText("Serialize Fields");
    for (auto &field : serializedFields_) {
        const char *label = field.name.c_str();
        if (field.typeId == asTYPEID_BOOL) {
            ImGui::Checkbox(label, static_cast<bool *>(field.address));
        } else if (field.typeId == asTYPEID_INT32) {
            ImGui::DragInt(label, static_cast<int32_t *>(field.address));
        } else if (field.typeId == asTYPEID_UINT32) {
            ImGui::DragScalar(label, ImGuiDataType_U32, field.address);
        } else if (field.typeId == asTYPEID_FLOAT) {
            ImGui::DragFloat(label, static_cast<float *>(field.address), 0.01f);
        } else if (field.typeId == asTYPEID_DOUBLE) {
            ImGuiCustom::EditValue(label, *static_cast<double *>(field.address), { .vSpeed = 0.01f });
        } else if (field.typeId == stringTypeId_) {
            ImGuiCustom::EditValue(label, *static_cast<std::string *>(field.address));
        } else if (field.typeId == vector2TypeId_) {
            ImGuiCustom::EditValue(label, *static_cast<Vector2 *>(field.address), { .vSpeed = 0.01f });
        } else if (field.typeId == vector3TypeId_) {
            ImGuiCustom::EditValue(label, *static_cast<Vector3 *>(field.address), { .vSpeed = 0.01f });
        } else if (field.typeId == vector4TypeId_) {
            ImGuiCustom::EditValue(label, *static_cast<Vector4 *>(field.address), { .vSpeed = 0.01f });
        } else if (field.typeId == quaternionTypeId_) {
            ImGuiCustom::EditValue(label, *static_cast<Quaternion *>(field.address), { .vSpeed = 0.01f });
        } else {
            ImGui::Text("%s: (unsupported type)", label);
        }
    }
}
#endif

JSON ScriptComponent::SaveToJson() const {
    JSON json = JSON::object();
    json["scriptPath"] = scriptPath_;
    json["fields"] = CaptureFieldValuesToJson();
    return json;
}

bool ScriptComponent::LoadFromJson(const JSON &json) {
    scriptPath_ = json.value("scriptPath", std::string{});
    pendingFieldValues_ = json.value("fields", JSON::object());

    // シーン読み込み時はコンポーネント追加時点でInitialize()が空のscriptPath_で
    // 呼ばれてしまっているため、ここで読み込んだパスを使って改めてリロードする。
    // 非アクティブな場合はSetActive(true)時のInitialize()に任せる
    if (IsActive()) {
        if (Reload()) {
            HookColliders();
            CallMethod(startMethod_);
        }
    } else if (!serializedFields_.empty()) {
        // 既にビルド済み（Initialize後に読み込まれた）の場合はその場で反映する
        ApplyFieldValuesFromJson(pendingFieldValues_);
    }
    return true;
}

} // namespace KashipanEngine
