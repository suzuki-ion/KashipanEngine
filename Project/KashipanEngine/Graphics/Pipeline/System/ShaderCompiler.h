#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>

#include "Debug/Logger.h"
#include "Graphics/Pipeline/System/MaterialLayout.h"

class ID3D12ShaderReflectionType;
namespace KashipanEngine {

class PipelineManager;

/// @brief シェーダーコンパイラクラス
class ShaderCompiler final {
public:
    /// @brief シェーダー変数の反射情報
    struct ShaderVariable {
        const std::string &VariableName() const { return variableName; }
        const std::string &TypeName() const { return typeName; }
        D3D_SHADER_INPUT_TYPE ResourceType() const { return resourceType; }
        UINT BindPoint() const { return bindPoint; }
        UINT BindCount() const { return bindCount; }
        UINT Space() const { return space; }
        UINT ByteOffset() const { return byteOffset; }
        UINT ByteSize() const { return byteSize; }
        UINT Rows() const { return rows; }
        UINT Columns() const { return columns; }
        UINT Elements() const { return elements; }
        UINT Members() const { return members; }
        bool IsResource() const { return isResource; }
        const std::unordered_map<std::string, ShaderVariable> &MemberVariables() const { return memberVariables; }
    private:
        friend class ShaderCompiler;
        std::string variableName;
        std::string typeName;
        D3D_SHADER_INPUT_TYPE resourceType = D3D_SIT_CBUFFER;
        UINT bindPoint = 0;
        UINT bindCount = 0;
        UINT space = 0;
        UINT byteOffset = 0;
        UINT byteSize = 0;
        UINT rows = 0;
        UINT columns = 0;
        UINT elements = 0;
        UINT members = 0;
        bool isResource = false;
        std::unordered_map<std::string, ShaderVariable> memberVariables;
    };

private:
    /// @brief リフレクションのリソースバインディング情報構造体
    struct ResourceBindingInfo {
        const std::string &Name() const { return name; }
        D3D_SHADER_INPUT_TYPE Type() const { return type; }
        UINT BindPoint() const { return bindPoint; }
        UINT BindCount() const { return bindCount; }
        UINT NumSamples() const { return numSamples; }
        UINT Space() const { return space; }
        UINT Flags() const { return flags; }
    private:
        friend class ShaderCompiler;
        std::string name;
        D3D_SHADER_INPUT_TYPE type = D3D_SIT_CBUFFER;
        UINT bindPoint = 0;
        UINT bindCount = 0;
        UINT numSamples = 0;
        UINT space = 0; // register space
        UINT flags = 0; // D3D12_SHADER_INPUT_BIND_DESC::uFlags
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
            LogScope scope;
            auto it = resourceBindings.find(name);
            if (it != resourceBindings.end()) {
                return &it->second;
            }
            return nullptr;
        }
        const InputParameterInfo *GetInputParameterInfo(const std::string &semanticName, UINT semanticIndex) const {
            LogScope scope;
            for (const auto &param : inputParameters) {
                if (param.semanticName == semanticName && param.semanticIndex == semanticIndex) {
                    return &param;
                }
            }
            return nullptr;
        }
        const std::unordered_map<std::string, ResourceBindingInfo> &ResourceBindings() const { return resourceBindings; }
        const std::unordered_map<std::string, ShaderVariable> &ShaderVariables() const { return shaderVariables; }
        const std::vector<InputParameterInfo> &InputParameters() const { return inputParameters; }
        const ThreadGroupSize &GetThreadGroupSize() const { return threadGroupSize; }
    private:
        friend class ShaderCompiler;
        std::unordered_map<std::string, ResourceBindingInfo> resourceBindings;  ///< リソースバインディング情報マップ
        std::unordered_map<std::string, ShaderVariable> shaderVariables;        ///< CPU側からバインドする変数とメンバ変数
        std::vector<InputParameterInfo> inputParameters;                        ///< インプットパラメーター情報マップ
        ThreadGroupSize threadGroupSize;                                        ///< スレッドグループサイズ（コンピュートシェーダー用）
    };

public:
    /// @brief コンパイル済みシェーダー情報のクリア
    static void ClearAllCompiledShaders(Passkey<PipelineManager>);

    /// @brief シェーダーコンパイル結果情報構造体
    struct ShaderCompiledInfo {
        ShaderCompiledInfo(Passkey<ShaderCompiler>, uint32_t shaderID) : id(shaderID) {}
        /// @brief シェーダー名取得
        const std::string &Name() const { return name; }
        /// @brief バイトコード取得
        const void *GetBytecodePtr() const { return bytecode ? bytecode->GetBufferPointer() : nullptr; }
        /// @brief バイトコードサイズ取得
        size_t GetBytecodeSize() const { return bytecode ? bytecode->GetBufferSize() : 0; }
        /// @brief リフレクション情報
        const ShaderReflectionInfo &GetReflectionInfo() const { return reflectionInfo; }
        /// @brief struct Material のバイトレイアウト（Material を持たないシェーダーでは空）
        const MaterialLayout &GetMaterialLayout() const { return materialLayout; }
    private:
        friend class ShaderCompiler;
        const uint32_t id;                          ///< シェーダーID
        std::string name;                           ///< シェーダー名
        Microsoft::WRL::ComPtr<IDxcBlob> bytecode;  ///< シェーダーバイトコード
        ShaderReflectionInfo reflectionInfo;        ///< シェーダーリフレクション情報
        MaterialLayout materialLayout;              ///< struct Material のバイトレイアウト（HLSLソース走査で取得）
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

    /// @brief 型情報からシェーダー変数情報を構築
    ShaderVariable BuildShaderVariableFromType(
        ID3D12ShaderReflectionType *type,
        const std::string &name,
        UINT byteOffset,
        UINT byteSize
    );
    /// @brief シェーダー変数のバインディング情報を埋める
    /// @param variable シェーダー変数情報
    /// @param name 変数名
    /// @param type シェーダー入力タイプ
    /// @param bindPoint バインドポイント
    /// @param bindCount バインドカウント
    /// @param space レジスタスペース
    void FillVariableBindingInfo(
        ShaderCompiler::ShaderVariable &variable,
        const std::string &name,
        D3D_SHADER_INPUT_TYPE type,
        UINT bindPoint,
        UINT bindCount,
        UINT space
    );

    ID3D12Device *device_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcLibrary> dxcLibrary_;
};

} // namespace KashipanEngine
