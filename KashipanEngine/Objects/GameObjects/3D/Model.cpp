#include "Model.h"

namespace KashipanEngine {

Model::Model(const ModelData &modelData)
    : Object3DBase(
        modelData.GetAssetRelativePath(),
        sizeof(Vertex),
        sizeof(Index),
        modelData.GetVertexCount(),
        modelData.GetIndexCount()) {
    SetRenderType(RenderType::Instancing);
    InitializeGeometry(modelData);
}

Model::Model(const std::string &relativePath)
    : Model(sModelManager->GetModelDataFromAssetPath(relativePath)) {
}

Model::Model(ModelManager::ModelHandle handle)
    : Model(sModelManager->GetModelData(handle)) {
}

void Model::InitializeGeometry(const ModelData& modelData) {
    auto dstV = GetVertexSpan<Vertex>();
    auto dstI = GetIndexSpan<Index>();

    if (dstV.size() != modelData.vertices_.size()) return;
    if (dstI.size() != modelData.indices_.size()) return;

    for (size_t i = 0; i < dstV.size(); ++i) {
        const auto& src = modelData.vertices_[i];
        dstV[i].position = Vector4(src.px, src.py, src.pz, 1.0f);
        dstV[i].normal = Vector3(src.nx, src.ny, src.nz);
        dstV[i].texcoord = Vector2(src.u, src.v);
    }

    for (size_t i = 0; i < dstI.size(); ++i) {
        dstI[i] = static_cast<Index>(modelData.indices_[i]);
    }
}

bool Model::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents3D("Transform3D") == 0 ||
        HasComponents3D("Material3D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Model::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
