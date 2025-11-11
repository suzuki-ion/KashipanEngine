#include "ShaderCompiler.h"
#include <stdexcept>
#include <vector>
#include <string>
#include <format>
#include <d3d12shader.h>
#include "Utilities/Conversion/ConvertString.h"

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
} // namespace

void ShaderCompiler::ClearAllCompiledShaders(Passkey<PipelineManager>) {
    LogScope scope;
    uint32_t count = static_cast<uint32_t>(sCompiledShaders.size() - sFreeShaderIDs.size());
    Log(Translation("engine.graphics.shadercompiler.allclear.showcount") + std::to_string(count), LogSeverity::Debug);
    sCompiledShaders.clear();
    sFreeShaderIDs.clear();
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
    wstrArgs.reserve(8 + compileInfo.macros.size());
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

    Microsoft::WRL::ComPtr<IDxcResult> result;
    hr = dxcCompiler_->Compile(
        &src,
        args.data(),
        static_cast<UINT>(args.size()),
        includeHandler.Get(),
        IID_PPV_ARGS(&result)
    );
    if (FAILED(hr) || result == nullptr) {
        Log(Translation("engine.graphics.shadercompiler.compile.failed") + compileInfo.filePath, LogSeverity::Error);
        return nullptr;
    }

    // コンパイルエラーの確認
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0) {
        Log(Translation("engine.graphics.shadercompiler.compile.error") + std::string(errors->GetStringPointer()), LogSeverity::Error);
        return nullptr;
    }

    // バイナリ取得
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    if (FAILED(hr) || shaderBlob == nullptr) {
        Log(Translation("engine.graphics.shadercompiler.compile.getobject.failed"), LogSeverity::Error);
        return nullptr;
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

} // namespace KashipanEngine