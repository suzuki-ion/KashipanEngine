#include "Model.h"
#include "Objects/Components/3D/Material3D.h"
#include "Assets/TextureManager.h"
#include <algorithm>
#include <cstring>

namespace KashipanEngine {

Model::Model(const ModelData &modelData)
    : Object3DBase(
        modelData.GetAssetRelativePath(),
        modelData.HasSkinning() ? sizeof(VertexSkinning) : sizeof(VertexNormal),
        sizeof(Index),
        modelData.GetVertexCount(),
        modelData.GetIndexCount()) {
    hasSkinning_ = modelData.HasSkinning();
    SetRenderType(hasSkinning_ ? RenderType::Standard : RenderType::Instancing);
    InitializeGeometry(modelData);
    InitializeSkinning(modelData);
}

Model::Model(const std::string &relativePath)
    : Model(sModelManager->GetModelDataFromAssetPath(relativePath)) {
}

Model::Model(ModelManager::ModelHandle handle)
    : Model(sModelManager->GetModelData(handle)) {
}

void Model::InitializeGeometry(const ModelData& modelData) {
    auto dstI = GetIndexSpan<Index>();

    if (dstI.size() != modelData.indices_.size()) return;

    if (hasSkinning_) {
        auto dstV = GetVertexSpan<VertexSkinning>();
        if (dstV.size() != modelData.vertices_.size()) return;

        for (size_t i = 0; i < dstV.size(); ++i) {
            const auto& src = modelData.vertices_[i];
            dstV[i].position = Vector4(src.px, src.py, src.pz, 1.0f);
            dstV[i].normal = Vector3(src.nx, src.ny, src.nz);
            dstV[i].texcoord = Vector2(src.u, src.v);
            std::copy(std::begin(src.boneIndices), std::end(src.boneIndices), std::begin(dstV[i].boneIndices));
            std::copy(std::begin(src.boneWeights), std::end(src.boneWeights), std::begin(dstV[i].boneWeights));
        }
    } else {
        auto dstV = GetVertexSpan<VertexNormal>();
        if (dstV.size() != modelData.vertices_.size()) return;

        for (size_t i = 0; i < dstV.size(); ++i) {
            const auto& src = modelData.vertices_[i];
            dstV[i].position = Vector4(src.px, src.py, src.pz, 1.0f);
            dstV[i].normal = Vector3(src.nx, src.ny, src.nz);
            dstV[i].texcoord = Vector2(src.u, src.v);
            dstV[i].enableSkinning = 0;
        }
    }

    for (size_t i = 0; i < dstI.size(); ++i) {
        dstI[i] = static_cast<Index>(modelData.indices_[i]);
    }

    Material3D *material = GetComponent3D<Material3D>();
    if (material && modelData.GetMaterialCount() > 0) {
        const auto* matData = modelData.GetMaterial(0);
        if (matData) {
            material->SetColor(Vector4(
                matData->baseColor[0],
                matData->baseColor[1],
                matData->baseColor[2],
                matData->baseColor[3]
            ));
            if (!matData->diffuseTexturePath.empty()) {
                auto textureHandle = TextureManager::GetTextureFromAssetPath(matData->diffuseTexturePath);
                material->SetTexture(textureHandle);
            }
        }
    }
}

void Model::InitializeSkinning(const ModelData &modelData) {
    if (!hasSkinning_) return;

    skinClusterNames_ = modelData.GetSkinClusterNames();
    inverseBindPoseMatrices_.clear();
    inverseBindPoseMatrices_.reserve(modelData.GetSkinClusters().size());
    for (const auto &[name, cluster] : modelData.GetSkinClusters()) {
        inverseBindPoseMatrices_[name] = cluster.inverseBindPoseMatrix;
    }

    skeletonHandle_ = SkeletonManager::GetSkeletonHandleFromAssetPath(modelData.GetAssetRelativePath());

    SetConstantBufferRequirements({
        { "Vertex:gSkinningMatrices", sizeof(SkinningPalette) }
    });
    SetUpdateConstantBuffersFunction([this](void *constantBufferMaps, std::uint32_t instanceCount) -> bool {
        return UpdateSkinningPalette(constantBufferMaps, instanceCount);
    });
}

bool Model::UpdateSkinningPalette(void *constantBufferMaps, std::uint32_t instanceCount) {
    (void)instanceCount;
    if (!hasSkinning_) return true;
    if (!constantBufferMaps) return false;

    auto **maps = static_cast<void **>(constantBufferMaps);
    auto *palette = static_cast<SkinningPalette *>(maps[0]);
    if (!palette) return false;

    for (auto &matrix : palette->matrices) {
        matrix = Matrix4x4::Identity();
    }

    if (skeletonHandle_ == SkeletonManager::kInvalidHandle) {
        palette->enableSkinning = 0;
        return true;
    }
    palette->enableSkinning = 1;

    const auto &skeletonData = SkeletonManager::GetSkeletonData(skeletonHandle_);
    const auto &skeleton = skeletonData.GetSkeleton();
    if (skeleton.joints.empty()) return true;

    SkeletonManager::UpdateSkeletonJointTransforms(skeletonHandle_);

    const size_t matrixCount = std::min<size_t>(skinClusterNames_.size(), kMaxSkinningMatrices);
    for (size_t i = 0; i < matrixCount; ++i) {
        const std::string &jointName = skinClusterNames_[i];
        const auto jointIt = skeleton.jointNameToIndexMap.find(jointName);
        const auto bindIt = inverseBindPoseMatrices_.find(jointName);
        if (jointIt == skeleton.jointNameToIndexMap.end() || bindIt == inverseBindPoseMatrices_.end()) {
            continue;
        }

        const int32_t jointIndex = jointIt->second;
        if (jointIndex < 0 || static_cast<size_t>(jointIndex) >= skeleton.joints.size()) {
            continue;
        }

        auto *jointTransform = skeleton.joints[static_cast<size_t>(jointIndex)].skeletonSpaceTransform.get();
        if (!jointTransform) continue;

        palette->matrices[i] = bindIt->second * jointTransform->GetWorldMatrix();
        palette->normalMatrices[i] = palette->matrices[i].Inverse().Transpose();
    }

    return true;
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
