#include "Objects/Components/ScriptComponent.h"

#include <algorithm>
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
#include "Utilities/FileIO/Directory.h"

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

/// @brief 属性メタデータのトークン（属性名 + 引数リスト）
struct AttributeToken {
    std::string name;
    std::vector<std::string> args;
};

/// @brief 属性の引数文字列から前後の空白と引用符を取り除く
std::string UnquoteAttributeArg(const std::string &arg) {
    std::string s = TrimMetadata(arg);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

/// @brief 1つのメタデータブロック文字列を属性トークンへ分解する
/// @details `[SerializeField, Range(1, 10)]` のように1つのブロックへ複数の属性を
///          カンマ区切りで書けるため、括弧・引用符の外にあるカンマでのみ分割する
void ParseMetadataString(const std::string &metadata, std::vector<AttributeToken> &out) {
    std::vector<std::string> parts;
    std::string current;
    int parenDepth = 0;
    bool inQuote = false;
    char quoteChar = '"';
    for (const char c : metadata) {
        if (inQuote) {
            if (c == quoteChar) inQuote = false;
            current += c;
            continue;
        }
        if (c == '"' || c == '\'') { inQuote = true; quoteChar = c; current += c; continue; }
        if (c == '(') { ++parenDepth; current += c; continue; }
        if (c == ')') { if (parenDepth > 0) --parenDepth; current += c; continue; }
        if (c == ',' && parenDepth == 0) { parts.push_back(current); current.clear(); continue; }
        current += c;
    }
    parts.push_back(current);

    for (const auto &part : parts) {
        const std::string token = TrimMetadata(part);
        if (token.empty()) continue;

        AttributeToken attr;
        const auto open = token.find('(');
        if (open == std::string::npos) {
            attr.name = token;
        } else {
            attr.name = TrimMetadata(token.substr(0, open));
            const auto close = token.rfind(')');
            const auto argsEnd = (close != std::string::npos && close > open) ? close : token.size();
            const std::string argsStr = token.substr(open + 1, argsEnd - open - 1);

            // 引数を引用符の外にあるカンマで分割する
            if (!TrimMetadata(argsStr).empty()) {
                std::string arg;
                bool argInQuote = false;
                char argQuote = '"';
                for (const char c : argsStr) {
                    if (argInQuote) {
                        if (c == argQuote) argInQuote = false;
                        arg += c;
                        continue;
                    }
                    if (c == '"' || c == '\'') { argInQuote = true; argQuote = c; arg += c; continue; }
                    if (c == ',') { attr.args.push_back(UnquoteAttributeArg(arg)); arg.clear(); continue; }
                    arg += c;
                }
                attr.args.push_back(UnquoteAttributeArg(arg));
            }
        }
        out.push_back(std::move(attr));
    }
}

/// @brief メタデータのリストを属性トークンのリストへ変換する
/// @details `[SerializeField][Range(1, 10)]` のように別ブロックで書いた場合も
///          リストの各要素として渡ってくるため、全ブロックをまとめて解析する
std::vector<AttributeToken> ParseAttributeTokens(const std::vector<std::string> &metadataList) {
    std::vector<AttributeToken> tokens;
    for (const auto &metadata : metadataList) {
        ParseMetadataString(metadata, tokens);
    }
    return tokens;
}

/// @brief 属性の引数をfloatとして取得する（無い・解釈できない場合は既定値）
float ParseFloatArg(const std::vector<std::string> &args, size_t index, float defaultValue) {
    if (index >= args.size()) return defaultValue;
    try {
        return std::stof(args[index]);
    } catch (...) {
        return defaultValue;
    }
}

/// @brief 属性の引数をintとして取得する（無い・解釈できない場合は既定値）
int ParseIntArg(const std::vector<std::string> &args, size_t index, int defaultValue) {
    if (index >= args.size()) return defaultValue;
    try {
        return std::stoi(args[index]);
    } catch (...) {
        return defaultValue;
    }
}

/// @brief メタデータのリストを解析してフィールド属性へ変換する
ScriptComponent::FieldAttributes ParseFieldAttributes(const std::vector<std::string> &metadataList) {
    ScriptComponent::FieldAttributes attrs;
    for (const auto &token : ParseAttributeTokens(metadataList)) {
        if (token.name == kSerializeFieldMetadata) {
            attrs.serializeField = true;
        } else if (token.name == "Range") {
            attrs.hasRange = true;
            attrs.rangeMin = ParseFloatArg(token.args, 0, 0.0f);
            attrs.rangeMax = ParseFloatArg(token.args, 1, 1.0f);
            if (attrs.rangeMax < attrs.rangeMin) std::swap(attrs.rangeMin, attrs.rangeMax);
        } else if (token.name == "TextArea") {
            // Unityと同様、内容の行数に応じて minLines〜maxLines の範囲で高さが変わる
            attrs.multiline = true;
            attrs.minLines = std::max(1, ParseIntArg(token.args, 0, 3));
            attrs.maxLines = std::max(attrs.minLines, ParseIntArg(token.args, 1, attrs.minLines));
        } else if (token.name == "Multiline") {
            // Unityと同様、固定行数（既定3行）のテキストエリアになる
            attrs.multiline = true;
            attrs.minLines = attrs.maxLines = std::max(1, ParseIntArg(token.args, 0, 3));
        } else if (token.name == "ColorPicker") {
            attrs.colorPicker = true;
        } else if (token.name == "Header") {
            if (!token.args.empty()) attrs.header = token.args[0];
        } else if (token.name == "Space") {
            attrs.hasSpace = true;
            attrs.space = ParseFloatArg(token.args, 0, 8.0f);  // Unityの[Space]の既定値と同じ8px
        } else if (token.name == "Tooltip") {
            if (!token.args.empty()) attrs.tooltip = token.args[0];
        }
    }
    return attrs;
}

/// @brief 型メタデータに [System.Serializable] が含まれるかを確認する
bool HasSerializableMetadata(const std::vector<std::string> &metadataList) {
    for (const auto &token : ParseAttributeTokens(metadataList)) {
        if (token.name == "System.Serializable" || token.name == "Serializable") return true;
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

#if defined(USE_IMGUI)
/// @brief Assetsフォルダ以下に存在する全ての .as ファイルの一覧（キャッシュ）
/// @details ファイルシステムを毎フレーム走査しないよう、一度取得した結果を保持し、
///          明示的な更新（RefreshAvailableScriptPaths）があるまで使い回す
std::vector<std::string> gAvailableScriptPaths;
bool gAvailableScriptPathsValid = false;

/// @brief Assetsフォルダ以下の .as ファイル一覧を再取得する
void RefreshAvailableScriptPaths() {
    gAvailableScriptPaths.clear();

    const auto dir = GetDirectoryData("Assets", true, true);
    const auto filtered = GetDirectoryDataByExtension(dir, { ".as" });
    std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &d) {
        for (const auto &file : d.files) gAvailableScriptPaths.push_back(file);
        for (const auto &subdir : d.subdirectories) flatten(subdir);
    };
    flatten(filtered);

    std::sort(gAvailableScriptPaths.begin(), gAvailableScriptPaths.end());
    gAvailableScriptPathsValid = true;
}

/// @brief Assetsフォルダ以下の .as ファイル一覧を取得する（未取得の場合は取得してからキャッシュする）
const std::vector<std::string> &GetAvailableScriptPaths() {
    if (!gAvailableScriptPathsValid) RefreshAvailableScriptPaths();
    return gAvailableScriptPaths;
}
#endif // USE_IMGUI

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

bool ScriptComponent::IsSupportedFieldType(int typeId) const {
    return typeId == asTYPEID_BOOL || typeId == asTYPEID_INT32 || typeId == asTYPEID_UINT32 ||
           typeId == asTYPEID_FLOAT || typeId == asTYPEID_DOUBLE ||
           typeId == stringTypeId_ || typeId == vector2TypeId_ || typeId == vector3TypeId_ ||
           typeId == vector4TypeId_ || typeId == quaternionTypeId_;
}

void ScriptComponent::CollectSerializedFields(CScriptBuilder &builder) {
    serializedFields_.clear();
    asIScriptModule *module = builder.GetModule();
    if (!module) return;
    asIScriptEngine *engine = module->GetEngine();

    // グローバル変数
    const asUINT varCount = module->GetGlobalVarCount();
    for (asUINT i = 0; i < varCount; ++i) {
        FieldAttributes attrs = ParseFieldAttributes(builder.GetMetadataForVar(static_cast<int>(i)));
        if (!attrs.serializeField) continue;

        const char *name = nullptr;
        int typeId = 0;
        if (module->GetGlobalVar(i, &name, nullptr, &typeId) < 0 || !name) continue;

        void *address = module->GetAddressOfGlobalVar(i);
        if (!address) continue;

        SerializedField field;
        field.name = name;
        field.typeId = typeId;
        field.address = address;
        field.attributes = std::move(attrs);
        CollectSerializableChildren(field, builder, engine, 0);
        serializedFields_.push_back(std::move(field));
    }

    // Behaviorクラスのメンバ変数
    if (behaviorType_ && behaviorObject_) {
        const int behaviorTypeId = behaviorType_->GetTypeId();
        const asUINT propertyCount = behaviorType_->GetPropertyCount();
        for (asUINT i = 0; i < propertyCount; ++i) {
            FieldAttributes attrs = ParseFieldAttributes(builder.GetMetadataForTypeProperty(behaviorTypeId, static_cast<int>(i)));
            if (!attrs.serializeField) continue;

            const char *name = nullptr;
            int typeId = 0;
            if (behaviorType_->GetProperty(i, &name, &typeId) < 0 || !name) continue;

            void *address = behaviorObject_->GetAddressOfProperty(i);
            if (!address) continue;

            SerializedField field;
            field.name = name;
            field.typeId = typeId;
            field.address = address;
            field.attributes = std::move(attrs);
            CollectSerializableChildren(field, builder, engine, 0);
            serializedFields_.push_back(std::move(field));
        }
    }
}

void ScriptComponent::CollectSerializableChildren(SerializedField &field, CScriptBuilder &builder, asIScriptEngine *engine, int depth) {
    // 自己参照型（Serializableクラスが自身の型のメンバを持つ場合など）による無限再帰を防ぐ
    constexpr int kMaxSerializableDepth = 8;
    if (!engine || depth >= kMaxSerializableDepth) return;
    if (IsSupportedFieldType(field.typeId)) return;

    asITypeInfo *type = engine->GetTypeInfoById(field.typeId);
    if (!type || !(type->GetFlags() & asOBJ_SCRIPT_OBJECT)) return;

    // [System.Serializable] が付いたスクリプトクラスのみ展開の対象にする
    const int classTypeId = type->GetTypeId();
    if (!HasSerializableMetadata(builder.GetMetadataForType(classTypeId))) return;

    field.isScriptObject = true;
    const asUINT propertyCount = type->GetPropertyCount();
    for (asUINT i = 0; i < propertyCount; ++i) {
        const char *name = nullptr;
        int propTypeId = 0;
        bool isPrivate = false;
        bool isProtected = false;
        if (type->GetProperty(i, &name, &propTypeId, &isPrivate, &isProtected) < 0 || !name) continue;

        FieldAttributes attrs = ParseFieldAttributes(builder.GetMetadataForTypeProperty(classTypeId, static_cast<int>(i)));
        // Unityと同様、publicメンバは自動で対象になり、private/protectedは [SerializeField] が必要
        if ((isPrivate || isProtected) && !attrs.serializeField) continue;

        SerializedField child;
        child.name = name;
        child.typeId = propTypeId;
        child.propertyIndex = i;
        child.attributes = std::move(attrs);
        CollectSerializableChildren(child, builder, engine, depth + 1);
        if (!child.isScriptObject && !IsSupportedFieldType(child.typeId)) continue;
        field.children.push_back(std::move(child));
    }
}

JSON ScriptComponent::CaptureField(const SerializedField &field, void *address) const {
    if (!address) return JSON();

    if (field.isScriptObject) {
        // Serializableクラスはハンドルとして格納されているため、参照先を解決してから
        // 子フィールドをネストしたJSONオブジェクトへ書き出す
        asIScriptObject *object = *static_cast<asIScriptObject **>(address);
        if (!object) return JSON();
        JSON json = JSON::object();
        for (const auto &child : field.children) {
            void *childAddress = object->GetAddressOfProperty(child.propertyIndex);
            if (!childAddress) continue;
            JSON value = CaptureField(child, childAddress);
            if (!value.is_null()) json[child.name] = std::move(value);
        }
        return json;
    }

    if (field.typeId == asTYPEID_BOOL) {
        return *static_cast<bool *>(address);
    } else if (field.typeId == asTYPEID_INT32) {
        return *static_cast<int32_t *>(address);
    } else if (field.typeId == asTYPEID_UINT32) {
        return *static_cast<uint32_t *>(address);
    } else if (field.typeId == asTYPEID_FLOAT) {
        return *static_cast<float *>(address);
    } else if (field.typeId == asTYPEID_DOUBLE) {
        return *static_cast<double *>(address);
    } else if (field.typeId == stringTypeId_) {
        return *static_cast<std::string *>(address);
    } else if (field.typeId == vector2TypeId_) {
        return ToJSON(*static_cast<Vector2 *>(address));
    } else if (field.typeId == vector3TypeId_) {
        return ToJSON(*static_cast<Vector3 *>(address));
    } else if (field.typeId == vector4TypeId_) {
        return ToJSON(*static_cast<Vector4 *>(address));
    } else if (field.typeId == quaternionTypeId_) {
        return ToJSON(*static_cast<Quaternion *>(address));
    }
    return JSON();
}

void ScriptComponent::ApplyField(const SerializedField &field, void *address, const JSON &value) {
    if (!address) return;

    if (field.isScriptObject) {
        if (!value.is_object()) return;
        asIScriptObject *object = *static_cast<asIScriptObject **>(address);
        if (!object) return;
        for (const auto &child : field.children) {
            if (!value.contains(child.name)) continue;
            void *childAddress = object->GetAddressOfProperty(child.propertyIndex);
            if (childAddress) ApplyField(child, childAddress, value[child.name]);
        }
        return;
    }

    try {
        if (field.typeId == asTYPEID_BOOL) {
            *static_cast<bool *>(address) = value.get<bool>();
        } else if (field.typeId == asTYPEID_INT32) {
            *static_cast<int32_t *>(address) = value.get<int32_t>();
        } else if (field.typeId == asTYPEID_UINT32) {
            *static_cast<uint32_t *>(address) = value.get<uint32_t>();
        } else if (field.typeId == asTYPEID_FLOAT) {
            *static_cast<float *>(address) = value.get<float>();
        } else if (field.typeId == asTYPEID_DOUBLE) {
            *static_cast<double *>(address) = value.get<double>();
        } else if (field.typeId == stringTypeId_) {
            *static_cast<std::string *>(address) = value.get<std::string>();
        } else if (field.typeId == vector2TypeId_) {
            *static_cast<Vector2 *>(address) = FromJSON<Vector2>(value);
        } else if (field.typeId == vector3TypeId_) {
            *static_cast<Vector3 *>(address) = FromJSON<Vector3>(value);
        } else if (field.typeId == vector4TypeId_) {
            *static_cast<Vector4 *>(address) = FromJSON<Vector4>(value);
        } else if (field.typeId == quaternionTypeId_) {
            *static_cast<Quaternion *>(address) = FromJSON<Quaternion>(value);
        }
    } catch (const JSON::exception &) {
        // 型が合わない保存値（スクリプト側の型変更後など）は無視して既定値のままにする
    }
}

JSON ScriptComponent::CaptureFieldValuesToJson() const {
    if (serializedFields_.empty()) return pendingFieldValues_;

    JSON json = JSON::object();
    for (const auto &field : serializedFields_) {
        JSON value = CaptureField(field, field.address);
        if (!value.is_null()) json[field.name] = std::move(value);
    }
    return json;
}

void ScriptComponent::ApplyFieldValuesFromJson(const JSON &json) {
    if (!json.is_object()) return;

    for (auto &field : serializedFields_) {
        if (!json.contains(field.name)) continue;
        ApplyField(field, field.address, json[field.name]);
    }
}

#if defined(USE_IMGUI)
void ScriptComponent::ShowImGui() {
    ImGuiCustom::SelectString("Script Path", scriptPath_, GetAvailableScriptPaths(), true);
    ImGui::SameLine();
    if (ImGui::Button("Refresh List")) {
        RefreshAvailableScriptPaths();
    }
    // コンボの右に並べると画面外へはみ出して押しづらいため、Reloadは下の行に配置する
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
        ImGui::PushID(field.name.c_str());
        DrawFieldImGui(field, field.address);
        ImGui::PopID();
    }
}

void ScriptComponent::DrawFieldImGui(SerializedField &field, void *address) {
    const FieldAttributes &attrs = field.attributes;

    // 装飾系の属性（Header/Space）はフィールド本体の上に描画する
    if (!attrs.header.empty()) ImGui::SeparatorText(attrs.header.c_str());
    if (attrs.hasSpace) ImGui::Dummy(ImVec2(0.0f, attrs.space));

    if (!address) return;
    const char *label = field.name.c_str();

    if (field.isScriptObject) {
        // [System.Serializable] クラスはツリーで子フィールドを表示する
        asIScriptObject *object = *static_cast<asIScriptObject **>(address);
        if (!object) {
            ImGui::Text("%s: (null)", label);
            return;
        }
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen);
        if (!attrs.tooltip.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", attrs.tooltip.c_str());
        if (isOpen) {
            for (auto &child : field.children) {
                ImGui::PushID(child.name.c_str());
                DrawFieldImGui(child, object->GetAddressOfProperty(child.propertyIndex));
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        return;
    }

    if (field.typeId == asTYPEID_BOOL) {
        ImGui::Checkbox(label, static_cast<bool *>(address));
    } else if (field.typeId == asTYPEID_INT32) {
        if (attrs.hasRange) {
            ImGui::SliderInt(label, static_cast<int32_t *>(address),
                static_cast<int>(attrs.rangeMin), static_cast<int>(attrs.rangeMax));
        } else {
            ImGui::DragInt(label, static_cast<int32_t *>(address));
        }
    } else if (field.typeId == asTYPEID_UINT32) {
        if (attrs.hasRange) {
            const uint32_t minValue = static_cast<uint32_t>(std::max(0.0f, attrs.rangeMin));
            const uint32_t maxValue = static_cast<uint32_t>(std::max(0.0f, attrs.rangeMax));
            ImGui::SliderScalar(label, ImGuiDataType_U32, address, &minValue, &maxValue);
        } else {
            ImGui::DragScalar(label, ImGuiDataType_U32, address);
        }
    } else if (field.typeId == asTYPEID_FLOAT) {
        if (attrs.hasRange) {
            ImGui::SliderFloat(label, static_cast<float *>(address), attrs.rangeMin, attrs.rangeMax);
        } else {
            ImGui::DragFloat(label, static_cast<float *>(address), 0.01f);
        }
    } else if (field.typeId == asTYPEID_DOUBLE) {
        if (attrs.hasRange) {
            const double minValue = static_cast<double>(attrs.rangeMin);
            const double maxValue = static_cast<double>(attrs.rangeMax);
            ImGui::SliderScalar(label, ImGuiDataType_Double, address, &minValue, &maxValue);
        } else {
            ImGuiCustom::EditValue(label, *static_cast<double *>(address), { .vSpeed = 0.01f });
        }
    } else if (field.typeId == stringTypeId_) {
        auto *str = static_cast<std::string *>(address);
        if (attrs.multiline) {
            // 内容の行数に応じて minLines〜maxLines の範囲で高さを調整する（UnityのTextArea相当）
            int lineCount = 1 + static_cast<int>(std::count(str->begin(), str->end(), '\n'));
            lineCount = std::clamp(lineCount, attrs.minLines, attrs.maxLines);
            const float height = ImGui::GetTextLineHeight() * static_cast<float>(lineCount)
                + ImGui::GetStyle().FramePadding.y * 2.0f;
            ImGui::InputTextMultiline(label, str, ImVec2(0.0f, height));
        } else {
            ImGuiCustom::EditValue(label, *str);
        }
    } else if (field.typeId == vector2TypeId_) {
        ImGuiCustom::EditValue(label, *static_cast<Vector2 *>(address), { .vSpeed = 0.01f });
    } else if (field.typeId == vector3TypeId_) {
        ImGuiCustom::EditValue(label, *static_cast<Vector3 *>(address), { .vSpeed = 0.01f });
    } else if (field.typeId == vector4TypeId_) {
        if (attrs.colorPicker) {
            ImGui::ColorEdit4(label, &static_cast<Vector4 *>(address)->x);
        } else {
            ImGuiCustom::EditValue(label, *static_cast<Vector4 *>(address), { .vSpeed = 0.01f });
        }
    } else if (field.typeId == quaternionTypeId_) {
        ImGuiCustom::EditValue(label, *static_cast<Quaternion *>(address), { .vSpeed = 0.01f });
    } else {
        ImGui::Text("%s: (unsupported type)", label);
    }

    if (!attrs.tooltip.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", attrs.tooltip.c_str());
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
