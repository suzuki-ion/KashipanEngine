#pragma once
#include "Objects/Object3DBase.h"
#include "Objects/GameObjects/3D/VertexData3D.h"
#include "Assets/ModelManager.h"
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

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData3D;
    using Index = uint32_t;

    void InitializeGeometry(const ModelData& modelData);
};

} // namespace KashipanEngine
