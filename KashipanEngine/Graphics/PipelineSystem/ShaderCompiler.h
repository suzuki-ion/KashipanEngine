#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <memory>
#include <unordered_map>

namespace KashipanEngine {

class PipelineManager;

/// @brief シェーダーコンパイラクラス
class ShaderCompiler final {
    /// @brief リフレクションのリソースバインディング情報構造体
    struct ResourceBindingInfo {
        const std::string &Name() const { return name; }
        D3D_SHADER_INPUT_TYPE Type() const { return type; }
        UINT BindPoint() const { return bindPoint; }
        UINT BindCount() const { return bindCount; }
        UINT NumSamples() const { return numSamples; }
    private:
        friend class ShaderCompiler;
        std::string name;           ///< リソース名
        D3D_SHADER_INPUT_TYPE type; ///< リソースタイプ
        UINT bindPoint = 0;         ///< バインドポイント
        UINT bindCount = 0;         ///< バインド数
        UINT numSamples = 0;        ///< サンプル数
    };

    /// @brief リフレクションのインプットパラメーター情報構造体
    struct InputParameterInfo {
        const std::string &SemanticName() const { return semanticName; }
        UINT SemanticIndex() const { return semanticIndex; }
        BYTE UsageMask() const { return usageMask; }
        D3D_REGISTER_COMPONENT_TYPE ComponentType() const { return componentType; }
    private:
        friend class ShaderCompiler;
        std::string semanticName;   ///< セマンティック名
        UINT semanticIndex = 0;     ///< セマンティックインデックス
        BYTE usageMask = 0;         ///< 使用マスク
        D3D_REGISTER_COMPONENT_TYPE componentType; ///< コンポーネントタイプ
    };

    /// @brief スレッドグループサイズ構造体
    struct ThreadGroupSize {
        UINT X() const { return x; }
        UINT Y() const { return y; }
        UINT Z() const { return z; }
    private:
        friend class ShaderCompiler;
        UINT x = 0; ///< X方向スレッドグループサイズ
        UINT y = 0; ///< Y方向スレッドグループサイズ
        UINT z = 0; ///< Z方向スレッドグループサイズ
    };

    /// @brief リフレクション情報構造体
    struct ShaderReflectionInfo {
        const ResourceBindingInfo *GetResourceBindingInfo(const std::string &name) const {
            auto it = resourceBindings.find(name);
            if (it != resourceBindings.end()) {
                return &it->second;
            }
            return nullptr;
        }
        const InputParameterInfo *GetInputParameterInfo(const std::string &semanticName, UINT semanticIndex) const {
            for (const auto &param : inputParameters) {
                if (param.semanticName == semanticName && param.semanticIndex == semanticIndex) {
                    return &param;
                }
            }
            return nullptr;
        }
        const std::unordered_map<std::string, ResourceBindingInfo> &ResourceBindings() const { return resourceBindings; }
        const std::vector<InputParameterInfo> &InputParameters() const { return inputParameters; }
        const ThreadGroupSize &GetThreadGroupSize() const { return threadGroupSize; }
    private:
        friend class ShaderCompiler;
        std::unordered_map<std::string, ResourceBindingInfo> resourceBindings;  ///< リソースバインディング情報マップ
        std::vector<InputParameterInfo> inputParameters;                        ///< インプットパラメーター情報マップ
        ThreadGroupSize threadGroupSize;                                        ///< スレッドグループサイズ（コンピュートシェーダー用）
    };

public:
    /// @brief コンパイル済みシェーダー情報のクリア
    static void ClearAllCompiledShaders(Passkey<PipelineManager>);

    /// @brief シェーダーコンパイル結果情報構造体
    struct ShaderCompiledInfo {
        ShaderCompiledInfo(Passkey<ShaderCompiler>, uint32_t shaderID) : id(shaderID) {}
        const ShaderReflectionInfo &GetReflectionInfo() const { return reflectionInfo; }
    private:
        friend class ShaderCompiler;
        const uint32_t id;                          ///< シェーダーID
        std::string name;                           ///< シェーダー名
        Microsoft::WRL::ComPtr<IDxcBlob> bytecode;  ///< シェーダーバイトコード
        ShaderReflectionInfo reflectionInfo;        ///< シェーダーリフレクション情報
    };

    /// @brief シェーダーコンパイル情報構造体
    struct CompileInfo {
        std::string name;           ///< シェーダー名
        std::string filePath;       ///< シェーダーファイルパス
        std::string entryPoint;     ///< エントリーポイント名
        std::string targetProfile;  ///< ターゲットプロファイル
        std::vector<std::pair<std::string, std::string>> macros; ///< マクロ定義リスト
    };

    /// @brief コンストラクタ
    /// @param device D3D12デバイス
    ShaderCompiler(Passkey<PipelineManager>, ID3D12Device *device);
    ~ShaderCompiler();

    /// @brief シェーダーのコンパイル
    /// @param compileInfo コンパイル情報
    /// @return シェーダーコンパイル情報
    ShaderCompiledInfo *CompileShader(const CompileInfo &compileInfo);

    /// @brief シェーダーコンパイル情報の破棄
    void DestroyShader(ShaderCompiledInfo *shaderCompiledInfo);

private:
    /// @brief シェーダーのコンパイル内部処理
    /// @param compileInfo コンパイル情報
    ShaderCompiledInfo *ShaderCompile(const CompileInfo &compileInfo);
    /// @brief シェーダーリフレクション取得内部処理
    /// @param shaderBlob コンパイル済みシェーダーブロブ
    /// @param outReflectionInfo 出力先リフレクション情報
    void ShaderReflection(IDxcBlob *shaderBlob, ShaderReflectionInfo &outReflectionInfo);

    ID3D12Device *device_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcLibrary> dxcLibrary_;
};

} // namespace KashipanEngine