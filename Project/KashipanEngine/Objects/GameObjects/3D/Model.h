#pragma once
#include "Objects/Object3DBase.h"
#include "Objects/GameObjects/3D/VertexData3D.h"
#include "Objects/GameObjects/3D/VertexDataSkinning.h"
#include "Assets/ModelManager.h"
#include "Assets/SkeletonManager.h"
#include <array>
#include <unordered_map>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace KashipanEngine {

class GameEngine;

class Model : public Object3DBase {
    static inline ModelManager *sModelManager;
public:
    static void SetModelManager(Passkey<GameEngine>, ModelManager* modelManager) { sModelManager = modelManager; }

    /// @brief コンストラクタ
    /// @param modelData モデルデータ
    Model(const ModelData &modelData);
    /// @brief コンストラクタ
    /// @param relativePath Assetsフォルダからの相対パス
    Model(const std::string &relativePath);
    /// @brief コンストラクタ
    /// @param handle モデルハンドル
    Model(ModelManager::ModelHandle handle);
    ~Model() override = default;

    void SetSkeletonHandle(SkeletonManager::SkeletonHandle handle) noexcept { skeletonHandle_ = handle; }
    SkeletonManager::SkeletonHandle GetSkeletonHandle() const noexcept { return skeletonHandle_; }

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using VertexNormal = VertexData3D;
    using VertexSkinning = VertexDataSkinning;
    using Index = uint32_t;
    static constexpr uint32_t kMaxSkinningMatrices = 256;

    struct SkinningPalette {
        Matrix4x4 matrices[kMaxSkinningMatrices];
        Matrix4x4 normalMatrices[kMaxSkinningMatrices];
        uint32_t enableSkinning = 0;
    };

    void InitializeGeometry(const ModelData& modelData);
    void InitializeSkinning(const ModelData& modelData);
    bool UpdateSkinningPalette(void *constantBufferMaps, std::uint32_t instanceCount);

    bool hasSkinning_ = false;
    bool enableSkinning_ = false;
    SkeletonManager::SkeletonHandle skeletonHandle_ = SkeletonManager::kInvalidHandle;
    std::vector<std::string> skinClusterNames_;
    std::unordered_map<std::string, Matrix4x4> inverseBindPoseMatrices_;
};

} // namespace KashipanEngine
