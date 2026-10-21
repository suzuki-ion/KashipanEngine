#include "ShaderCompiler.h"
#include "Core/ProjectPaths.h"
#include "Utilities/FileIO/ContentCache.h"
#include <atomic>
#include <stdexcept>
#include <vector>
#include <string>
#include <format>
#include <filesystem>
#include <unordered_set>
#include <d3d12shader.h>
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/Translation.h"

#pragma comment(lib, "dxcompiler.lib")

namespace KashipanEngine {
namespace {
// 要素数事前確保サイズ
static constexpr size_t kPreallocateSize = 128;
// コンパイル済みシェーダー配列
std::vector<std::unique_ptr<ShaderCompiler::ShaderCompiledInfo>> sCompiledShaders;
// 空きのシェーダーID配列
std::vector<uint32_t> sFreeShaderIDs;

#ifndef DXIL_FOURCC
#define DXIL_FOURCC(ch0, ch1, ch2, ch3) (                            \
  (uint32_t)(uint8_t)(ch0)        | (uint32_t)(uint8_t)(ch1) << 8  | \
  (uint32_t)(uint8_t)(ch2) << 16  | (uint32_t)(uint8_t)(ch3) << 24   \
)
#endif


// Delegate to DXC's original handler and record the exact bytes actually included.
class RecordingIncludeHandler final : public IDxcIncludeHandler {
public:
    RecordingIncludeHandler(IDxcIncludeHandler *delegate, std::filesystem::path sourceDirectory)
        : delegate_(delegate), sourceDirectory_(std::move(sourceDirectory)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override {
        if (!object) return E_POINTER;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IDxcIncludeHandler)) {
            *object = static_cast<IDxcIncludeHandler *>(this); AddRef(); return S_OK;
        }
        *object = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR filename, IDxcBlob **source) override {
        const HRESULT hr = delegate_->LoadSource(filename, source);
        if (SUCCEEDED(hr) && source && *source) {
            try {
                auto path = std::filesystem::path(filename);
                if (!std::filesystem::exists(path)) path = sourceDirectory_ / path;
                auto dependency = ContentCache::Dependency(path);
                if (dependency["hash"] != ContentCache::Digest((*source)->GetBufferPointer(), (*source)->GetBufferSize())) cacheable = false;
                dependencies.push_back(std::move(dependency));
            } catch (...) { cacheable = false; }
        }
        return hr;
    }
    ContentCache::Json dependencies = ContentCache::Json::array();
    bool cacheable = true;
private:
    std::atomic_ulong references_{1};
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> delegate_;
    std::filesystem::path sourceDirectory_;
};

std::string DxcIdentity() {
    static const std::string identity = [] {
        wchar_t path[32768]{};
        const auto module = GetModuleHandleW(L"dxcompiler.dll");
        const auto length = module ? GetModuleFileNameW(module, path, 32768) : 0;
        return length && length < 32768 ? ContentCache::FileDigest(path) : std::string{};
    }();
    return identity;
}

} // namespace

void ShaderCompiler::ClearAllCompiledShaders(Passkey<PipelineManager>) {
    LogScope scope;
    uint32_t count = static_cast<uint32_t>(sCompiledShaders.size() - sFreeShaderIDs.size());
    Log(Translation("engine.graphics.shadercompiler.allclear.showcount") + std::to_string(count), LogSeverity::Debug);
    sCompiledShaders.clear();
    sFreeShaderIDs.clear();
}

std::vector<uint32_t> ShaderCompiler::CaptureCompiledShaderIDs(Passkey<PipelineManager>) {
    std::vector<uint32_t> shaderIDs;
    shaderIDs.reserve(sCompiledShaders.size() - sFreeShaderIDs.size());
    for (uint32_t shaderID = 0; shaderID < sCompiledShaders.size(); ++shaderID) {
        if (sCompiledShaders[shaderID]) shaderIDs.push_back(shaderID);
    }
    return shaderIDs;
}

void ShaderCompiler::DestroyCompiledShadersExcept(
    Passkey<PipelineManager>, const std::vector<uint32_t> &keptShaderIDs) {
    const std::unordered_set<uint32_t> keptIDs(keptShaderIDs.begin(), keptShaderIDs.end());
    for (uint32_t shaderID = 0; shaderID < sCompiledShaders.size(); ++shaderID) {
        if (sCompiledShaders[shaderID] && !keptIDs.contains(shaderID)) {
            sCompiledShaders[shaderID].reset();
            sFreeShaderIDs.push_back(shaderID);
        }
    }
}

void ShaderCompiler::DestroyCompiledShaders(
    Passkey<PipelineManager>, const std::vector<uint32_t> &shaderIDs) {
    for (const uint32_t shaderID : shaderIDs) {
        if (shaderID < sCompiledShaders.size() && sCompiledShaders[shaderID]) {
            sCompiledShaders[shaderID].reset();
            sFreeShaderIDs.push_back(shaderID);
        }
    }
}

ShaderCompiler::ShaderCompiler(Passkey<PipelineManager>, ID3D12Device *device) {
    LogScope scope;
    Log(Translation("engine.graphics.shadercompiler.initialize.start"), LogSeverity::Debug);
    device_ = device;

    //==================================================
    // DXCコンパイラの初期化
    //==================================================

    HRESULT hr = DxcCreateInstance(
        CLSID_DxcCompiler,
        IID_PPV_ARGS(&dxcCompiler_)
    );
    if (FAILED(hr)) {
        Log(Translation("engine.graphics.shadercompiler.dxccompiler.create.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to create DXC Compiler instance.");
    }

    hr = DxcCreateInstance(
        CLSID_DxcLibrary,
        IID_PPV_ARGS(&dxcLibrary_)
    );
    if (FAILED(hr)) {
        Log(Translation("engine.graphics.shadercompiler.dxclibrary.create.failed"), LogSeverity::Critical);
        throw std::runtime_error("Failed to create DXC Library instance.");
    }

    //==================================================
    // 配列の事前確保
    //==================================================

    sCompiledShaders.reserve(kPreallocateSize);
    sFreeShaderIDs.reserve(kPreallocateSize);

    Log(Translation("engine.graphics.shadercompiler.initialize.end"), LogSeverity::Debug);
}

ShaderCompiler::~ShaderCompiler() {
    LogScope scope;
    Log(Translation("engine.graphics.shadercompiler.finalize.start"), LogSeverity::Debug);
    sCompiledShaders.clear();
    sFreeShaderIDs.clear();
    dxcCompiler_.Reset();
    dxcLibrary_.Reset();
    Log(Translation("engine.graphics.shadercompiler.finalize.end"), LogSeverity::Debug);
}

ShaderCompiler::ShaderCompiledInfo *ShaderCompiler::CompileShader(const CompileInfo &compileInfo) {
    LogScope scope;
    Log(Translation("engine.graphics.shadercompiler.compile.start") + (compileInfo.filePath.empty() ? "(error:empty)" : compileInfo.filePath), LogSeverity::Debug);

    ShaderCompiledInfo *compiledShader = ShaderCompile(compileInfo);
    if (compiledShader == nullptr) {
        // nullptr が返されたらとりあえずクリティカルとしておく
        throw std::runtime_error("Shader compilation failed: " + compileInfo.filePath);
    }
    // リフレクション
    ShaderReflection(compiledShader->bytecode.Get(), compiledShader->reflectionInfo);

    Log(Translation("engine.graphics.shadercompiler.compile.end") + compiledShader->name, LogSeverity::Debug);
    return compiledShader;
}

void ShaderCompiler::DestroyShader(ShaderCompiledInfo *shaderCompiledInfo) {
    LogScope scope;
    if (shaderCompiledInfo == nullptr) {
        Log(Translation("engine.graphics.shadercompiler.destroy.nullptr"), LogSeverity::Warning);
        return;
    }
    uint32_t shaderID = shaderCompiledInfo->id;
    if (shaderID >= sCompiledShaders.size() || sCompiledShaders[shaderID].get() != shaderCompiledInfo) {
        Log(Translation("engine.graphics.shadercompiler.destroy.invalid"), LogSeverity::Warning);
        return;
    }
    // シェーダー情報の削除
    std::string name = shaderCompiledInfo->name;
    sCompiledShaders[shaderID].reset();
    sFreeShaderIDs.push_back(shaderID);
    Log(Translation("engine.graphics.shadercompiler.destroy.success"), LogSeverity::Debug);
}

ShaderCompiler::ShaderCompiledInfo *ShaderCompiler::ShaderCompile(const CompileInfo &compileInfo) {
    LogScope scope;
    if (compileInfo.filePath.empty() || compileInfo.entryPoint.empty() || compileInfo.targetProfile.empty()) {
        Log(Translation("engine.graphics.shadercompiler.compile.invalidargs"), LogSeverity::Error);
        return nullptr;
    }

    // HLSL読み込み
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
    UINT codePage = 0; // auto-detect
    std::wstring wFilePath = ConvertString(compileInfo.filePath);
    HRESULT hr = dxcLibrary_->CreateBlobFromFile(wFilePath.c_str(), &codePage, &sourceBlob);
    if (FAILED(hr) || sourceBlob == nullptr) {
        Log(Translation("engine.graphics.shadercompiler.compile.hlslnotfound") + compileInfo.filePath, LogSeverity::Error);
        return nullptr;
    }

    DxcBuffer src{};
    src.Ptr = sourceBlob->GetBufferPointer();
    src.Size = sourceBlob->GetBufferSize();
    src.Encoding = DXC_CP_UTF8; // assume UTF-8

    // Include handler
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
    hr = dxcLibrary_->CreateIncludeHandler(&includeHandler);
    if (FAILED(hr)) {
        Log(Translation("engine.graphics.shadercompiler.compile.includehandler.failed"), LogSeverity::Error);
        return nullptr;
    }

    // 引数組み立て
    std::wstring wEntry = ConvertString(compileInfo.entryPoint);
    std::wstring wTarget = ConvertString(compileInfo.targetProfile);

    std::vector<std::wstring> wstrArgs;
    wstrArgs.reserve(12 + compileInfo.macros.size());
    wstrArgs.emplace_back(wFilePath);   // ソース名
    wstrArgs.emplace_back(L"-E");
    wstrArgs.emplace_back(wEntry);      // エントリーポイント
    wstrArgs.emplace_back(L"-T");
    wstrArgs.emplace_back(wTarget);     // ターゲットプロファイル
    // デバッグ情報、行優先レイアウト。最適化は無効。
    wstrArgs.emplace_back(L"-Zi");
    wstrArgs.emplace_back(L"-Qembed_debug");
    wstrArgs.emplace_back(L"-Zpr");
    wstrArgs.emplace_back(L"-Od");

    // 参照元ファイルのディレクトリをインクルードパスに追加（相対 #include 対応）
    {
        std::filesystem::path incDir = Utf8StringToPath(compileInfo.filePath).parent_path();
        if (!incDir.empty()) {
            wstrArgs.emplace_back(L"-I");
            wstrArgs.emplace_back(incDir.wstring());
        }
    }

    // マクロ定義
    for (const auto &m : compileInfo.macros) {
        std::wstring def = L"-D" + ConvertString(m.first);
        if (!m.second.empty()) {
            def += L"=" + ConvertString(m.second);
        }
        wstrArgs.emplace_back(std::move(def));
    }

    std::vector<LPCWSTR> args;
    args.reserve(wstrArgs.size());
    for (auto &s : wstrArgs) args.emplace_back(s.c_str());

    std::string cacheKey = "ShaderBytecode-v1|" + DxcIdentity();
    for (const auto &argument : wstrArgs) {
        const auto text = ConvertString(argument);
        cacheKey += "|" + std::to_string(text.size()) + ":" + text;
    }
    cacheKey += "|cwd=" + ContentCache::PathText(std::filesystem::current_path());
    static const auto cacheDirectory = ContentCache::Directory(ProjectPaths::ProjectRoot(), "Shaders");
    const auto cachePath = cacheDirectory / (ContentCache::Digest(cacheKey) + ".cache");
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    bool cacheHit = false;
    if (!DxcIdentity().empty()) {
        if (const auto cached = ContentCache::Load(cachePath, cacheKey)) {
            Microsoft::WRL::ComPtr<IDxcBlobEncoding> blob;
            hr = dxcLibrary_->CreateBlobWithEncodingOnHeapCopy(cached->data(), static_cast<UINT32>(cached->size()), 0, &blob);
            if (SUCCEEDED(hr)) { shaderBlob = blob; cacheHit = true; }
        }
    }
    if (cacheHit) {
        ++cacheHits_;
    } else {
        ++cacheMisses_;
        Microsoft::WRL::ComPtr<RecordingIncludeHandler> recordingHandler;
        recordingHandler.Attach(new RecordingIncludeHandler(includeHandler.Get(), Utf8StringToPath(compileInfo.filePath).parent_path()));
        const auto sourceDependency = ContentCache::Dependency(Utf8StringToPath(compileInfo.filePath));
        if (sourceDependency["hash"] != ContentCache::Digest(sourceBlob->GetBufferPointer(), sourceBlob->GetBufferSize())) recordingHandler->cacheable = false;
        Microsoft::WRL::ComPtr<IDxcResult> result;
        hr = dxcCompiler_->Compile(
            &src,
            args.data(),
            static_cast<UINT>(args.size()),
            recordingHandler.Get(),
            IID_PPV_ARGS(&result)
        );
        if (FAILED(hr) || result == nullptr) {
            Log(Translation("engine.graphics.shadercompiler.compile.failed") + compileInfo.filePath, LogSeverity::Error);
            return nullptr;
        }

        // ステータス取得（これで成功/失敗を判定）
        HRESULT status = S_OK;
        result->GetStatus(&status);

        // 診断出力（警告は許容、失敗時はエラー）
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0) {
            std::string msg = std::string(errors->GetStringPointer());
            Log(Translation("engine.graphics.shadercompiler.compile.error") + msg,
                FAILED(status) ? LogSeverity::Error : LogSeverity::Warning);
        }

        if (FAILED(status)) {
            return nullptr;
        }

        // バイナリ取得
        hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
        if (FAILED(hr) || shaderBlob == nullptr) {
            Log(Translation("engine.graphics.shadercompiler.compile.getobject.failed"), LogSeverity::Error);
            return nullptr;
        }

        recordingHandler->dependencies.push_back(sourceDependency);
        if (recordingHandler->cacheable && !DxcIdentity().empty()) {
            ContentCache::Store(cachePath, cacheKey, recordingHandler->dependencies,
                shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
        }
    }

    // IDの確保
    uint32_t shaderID;
    if (!sFreeShaderIDs.empty()) {
        shaderID = sFreeShaderIDs.back();
        sFreeShaderIDs.pop_back();
    } else {
        shaderID = static_cast<uint32_t>(sCompiledShaders.size());
        sCompiledShaders.emplace_back(nullptr);
    }

    auto info = std::make_unique<ShaderCompiledInfo>(Passkey<ShaderCompiler>{}, shaderID);
    info->name = compileInfo.name.empty() ? compileInfo.filePath : compileInfo.name;
    info->bytecode = shaderBlob;

    // struct Material のバイトレイアウトをHLSLソースから求める（Materialを持たないシェーダーでは空になるだけで無害）
    std::unordered_set<std::string> definedMacroNames;
    for (const auto &m : compileInfo.macros) definedMacroNames.insert(m.first);
    info->materialLayout = MaterialLayout::BuildFromHlslSource(compileInfo.filePath, definedMacroNames);

    // 保存して返す
    ShaderCompiledInfo *ret = info.get();
    sCompiledShaders[shaderID] = std::move(info);
    return ret;
}

void ShaderCompiler::ShaderReflection(IDxcBlob *shaderBlob, ShaderReflectionInfo &outReflectionInfo) {
    if (shaderBlob == nullptr) return;

    Microsoft::WRL::ComPtr<IDxcContainerReflection> container;
    HRESULT hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container));
    if (FAILED(hr) || container == nullptr) {
        Log(Translation("engine.graphics.shadercompiler.reflection.container.failed"), LogSeverity::Warning);
        return;
    }

    hr = container->Load(shaderBlob);
    if (FAILED(hr)) {
        Log(Translation("engine.graphics.shadercompiler.reflection.load.failed"), LogSeverity::Warning);
        return;
    }

    UINT32 partCount = 0;
    container->GetPartCount(&partCount);

    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> shaderRefl;
    for (UINT32 i = 0; i < partCount; ++i) {
        UINT32 kind = 0;
        if (FAILED(container->GetPartKind(i, &kind))) continue;
        if (kind == DXIL_FOURCC('D','X','I','L')) {
            container->GetPartReflection(i, __uuidof(ID3D12ShaderReflection), reinterpret_cast<void**>(shaderRefl.GetAddressOf()));
            break;
        }
    }

    if (!shaderRefl) {
        Log(Translation("engine.graphics.shadercompiler.reflection.get.failed"), LogSeverity::Warning);
        return;
    }

    D3D12_SHADER_DESC desc{};
    if (FAILED(shaderRefl->GetDesc(&desc))) return;

    outReflectionInfo.resourceBindings.clear();
    outReflectionInfo.shaderVariables.clear();

    // リソースバインディング
    for (UINT i = 0; i < desc.BoundResources; ++i) {
        D3D12_SHADER_INPUT_BIND_DESC bind{};
        if (FAILED(shaderRefl->GetResourceBindingDesc(i, &bind))) continue;

        ResourceBindingInfo info{};
        info.name = bind.Name ? bind.Name : "";
        info.type = static_cast<D3D_SHADER_INPUT_TYPE>(bind.Type);
        info.bindPoint = bind.BindPoint;
        info.bindCount = bind.BindCount;
        info.numSamples = bind.NumSamples;
        info.space = bind.Space;
        info.flags = bind.uFlags;

        ShaderVariable resourceVariable{};
        FillVariableBindingInfo(resourceVariable, info.name, info.type, info.bindPoint, info.bindCount, info.space);

        if (info.type == D3D_SIT_CBUFFER || info.type == D3D_SIT_TBUFFER) {
            ID3D12ShaderReflectionConstantBuffer *constantBuffer = shaderRefl->GetConstantBufferByName(info.name.c_str());
            D3D12_SHADER_BUFFER_DESC bufferDesc{};
            if (constantBuffer && SUCCEEDED(constantBuffer->GetDesc(&bufferDesc))) {
                resourceVariable.byteSize = bufferDesc.Size;
                for (UINT variableIndex = 0; variableIndex < bufferDesc.Variables; ++variableIndex) {
                    ID3D12ShaderReflectionVariable *reflectedVariable = constantBuffer->GetVariableByIndex(variableIndex);
                    if (!reflectedVariable) continue;

                    D3D12_SHADER_VARIABLE_DESC variableDesc{};
                    if (FAILED(reflectedVariable->GetDesc(&variableDesc))) continue;

                    const std::string variableName = variableDesc.Name ? variableDesc.Name : "";
                    ShaderVariable memberVariable = BuildShaderVariableFromType(
                        reflectedVariable->GetType(),
                        variableName,
                        variableDesc.StartOffset,
                        variableDesc.Size
                    );
                    resourceVariable.memberVariables[memberVariable.variableName] = std::move(memberVariable);
                }
            }
        } else {
            ID3D12ShaderReflectionVariable *reflectedVariable = shaderRefl->GetVariableByName(info.name.c_str());
            D3D12_SHADER_VARIABLE_DESC variableDesc{};
            if (reflectedVariable && SUCCEEDED(reflectedVariable->GetDesc(&variableDesc))) {
                resourceVariable = BuildShaderVariableFromType(
                    reflectedVariable->GetType(),
                    info.name,
                    variableDesc.StartOffset,
                    variableDesc.Size
                );
                FillVariableBindingInfo(resourceVariable, info.name, info.type, info.bindPoint, info.bindCount, info.space);
            }
        }

        outReflectionInfo.shaderVariables[info.name] = std::move(resourceVariable);

        // key を name + space にする場合のコード
        // std::string key = info.name + "#s" + std::to_string(info.space);
        // outReflectionInfo.resourceBindings[key] = std::move(info);
        outReflectionInfo.resourceBindings[info.name] = std::move(info);
    }

    // 入力パラメータ（VS等）
    outReflectionInfo.inputParameters.clear();
    outReflectionInfo.inputParameters.reserve(desc.InputParameters);
    for (UINT i = 0; i < desc.InputParameters; ++i) {
        D3D12_SIGNATURE_PARAMETER_DESC sig{};
        if (FAILED(shaderRefl->GetInputParameterDesc(i, &sig))) continue;
        InputParameterInfo param{};
        param.semanticName = sig.SemanticName ? sig.SemanticName : "";
        param.semanticIndex = sig.SemanticIndex;
        param.usageMask = sig.Mask;
        param.componentType = static_cast<D3D_REGISTER_COMPONENT_TYPE>(sig.ComponentType);
        outReflectionInfo.inputParameters.emplace_back(std::move(param));
    }

    // スレッドグループサイズ（CSのみ）
    UINT tgx = 0, tgy = 0, tgz = 0;
    if (SUCCEEDED(shaderRefl->GetThreadGroupSize(&tgx, &tgy, &tgz))) {
        outReflectionInfo.threadGroupSize.x = tgx;
        outReflectionInfo.threadGroupSize.y = tgy;
        outReflectionInfo.threadGroupSize.z = tgz;
    }
}


ShaderCompiler::ShaderVariable ShaderCompiler::BuildShaderVariableFromType(
    ID3D12ShaderReflectionType *type,
    const std::string &name,
    UINT byteOffset,
    UINT byteSize
) {
    ShaderCompiler::ShaderVariable variable{};
    variable.variableName = name;
    variable.byteOffset = byteOffset;
    variable.byteSize = byteSize;

    if (!type) return variable;

    D3D12_SHADER_TYPE_DESC typeDesc{};
    if (FAILED(type->GetDesc(&typeDesc))) return variable;

    variable.typeName = typeDesc.Name ? typeDesc.Name : "";
    variable.rows = typeDesc.Rows;
    variable.columns = typeDesc.Columns;
    variable.elements = typeDesc.Elements;
    variable.members = typeDesc.Members;
    if (variable.byteOffset == 0) variable.byteOffset = typeDesc.Offset;

    for (UINT i = 0; i < typeDesc.Members; ++i) {
        ID3D12ShaderReflectionType *memberType = type->GetMemberTypeByIndex(i);
        if (!memberType) continue;

        const char *memberTypeName = type->GetMemberTypeName(i);
        std::string memberName = memberTypeName ? memberTypeName : ("member" + std::to_string(i));

        D3D12_SHADER_TYPE_DESC memberDesc{};
        if (SUCCEEDED(memberType->GetDesc(&memberDesc))) {
            variable.memberVariables[memberName] = BuildShaderVariableFromType(
                memberType,
                memberName,
                memberDesc.Offset,
                0
            );
        } else {
            variable.memberVariables[memberName] = BuildShaderVariableFromType(memberType, memberName, 0, 0);
        }
    }

    return variable;
}

void ShaderCompiler::FillVariableBindingInfo(
    ShaderCompiler::ShaderVariable &variable,
    const std::string &name,
    D3D_SHADER_INPUT_TYPE type,
    UINT bindPoint,
    UINT bindCount,
    UINT space
) {
    variable.variableName = name;
    variable.resourceType = type;
    variable.bindPoint = bindPoint;
    variable.bindCount = bindCount;
    variable.space = space;
    variable.isResource = true;
}

} // namespace KashipanEngine
