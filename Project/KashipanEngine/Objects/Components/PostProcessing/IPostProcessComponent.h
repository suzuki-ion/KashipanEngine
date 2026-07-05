#pragma once
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"

namespace KashipanEngine {

class Renderer;

/// @brief ポストエフェクト用オブジェクトコンポーネントの基底クラス
/// @details ScreenBufferObject コンポーネントが付与されたオブジェクトへ付与することで、
///          そのスクリーンバッファに対してポストエフェクトがかけられる。
///          オブジェクトへは IPostProcessComponent として登録されるように、
///          コンポーネントIDは IPostProcessComponent から生成したIDで固定にしている。
class IPostProcessComponent : public IObjectComponent {
public:
    struct PassInfo {
        struct ConstantBufferRequirement {
            std::string variableName;
            size_t byteSize = 0;
            void *dataPtr = nullptr;
        };
        struct InstanceBufferRequirement {
            std::string variableName;
            size_t elementStride = 0;
            void *dataPtr = nullptr;
        };
        std::string pipelineName;
        std::vector<ConstantBufferRequirement> constantBufferRequirements;
        std::vector<InstanceBufferRequirement> instanceBufferRequirements;
    };
    virtual ~IPostProcessComponent() = default;

    /// @brief ポストエフェクトパス情報の構築（Renderer から呼ばれる）
    std::vector<PassInfo> BuildPassesInterface(Passkey<Renderer>) { return BuildPasses(); }

protected:
    IPostProcessComponent(const std::string &componentType, size_t maxComponentCountPerBuffer = 0xFF)
        : IObjectComponent(componentType, maxComponentCountPerBuffer, GetComponentTypeID<IPostProcessComponent>()) {}

    /// @brief ポストエフェクトパス情報を構築する（派生クラスで実装）
    /// @return 実行するパスのリスト（1コンポーネントで複数パスを返してもよい）
    virtual std::vector<PassInfo> BuildPasses() = 0;
};

} // namespace KashipanEngine
