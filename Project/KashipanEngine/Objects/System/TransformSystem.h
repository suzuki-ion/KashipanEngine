#pragma once
#include "Utilities/EntityComponentSystem.h"
#include "Objects/Components/Transform.h"

namespace KashipanEngine {

class TransformSystem : public ComponentSystem<Transform, TransformWorldMatrix> {
    bool ResolveMatrix(const Entity &entity, const EntityManager &entityManager, ComponentStorage &componentStorage) {
        bool isDirty = false;
        Matrix4x4 parentWorldMatrix = Matrix4x4::Identity();

        // 自分のTransformVersionとWorldMatrixのバージョンを比較して、どちらかが更新されている場合はDirtyとする
        auto *transformVersion = componentStorage.GetComponent<TransformVersion>(entity);
        auto *worldMatrix = componentStorage.GetComponent<TransformWorldMatrix>(entity);
        if (transformVersion && worldMatrix) {
            if (transformVersion->version != worldMatrix->lastKnownVersion) {
                isDirty = true;
                worldMatrix->lastKnownVersion = transformVersion->version;
            }
        }

        // 親がいる場合は親のワールド行列を再帰的に確定させる
        if (auto *parent = componentStorage.GetComponent<TransformParent>(entity)) {
            if (entityManager.IsEntityAlive(parent->parentEntity)) {
                ResolveMatrix(parent->parentEntity, entityManager, componentStorage);
                auto *parentWorld = componentStorage.GetComponent<TransformWorldMatrix>(parent->parentEntity);
                // 子が知っている親のバージョンと親のワールド行列のバージョンが異なる場合はDirtyとする
                if (parentWorld && parent->lastKnownVersion != parentWorld->lastKnownVersion) {
                    isDirty = true;
                    parent->lastKnownVersion = parentWorld->lastKnownVersion;
                    parentWorldMatrix = parentWorld->matrix;
                }
            }
        }

        // 自分自身または親がDirtyの場合はワールド行列を再計算
        if (isDirty) {
            auto *transform = componentStorage.GetComponent<Transform>(entity);
            if (transform && worldMatrix) {

                Matrix4x4 scaleMatrix;
                scaleMatrix.MakeScale(transform->scale);
                Quaternion rotation = transform->rotation;
                Matrix4x4 rotationMatrix = rotation.MakeRotateMatrix();
                Matrix4x4 translationMatrix;
                translationMatrix.MakeTranslate(transform->position);

                worldMatrix->matrix = parentWorldMatrix * (translationMatrix * rotationMatrix * scaleMatrix);
            }
        }

        return isDirty;
    }

public:
    TransformSystem() = default;
    ~TransformSystem() override = default;

protected:
    void UpdateEntity(const Entity &entity, const EntityManager &entityManager, ComponentStorage &componentStorage, float deltaTime) override {
        ResolveMatrix(entity, entityManager, componentStorage);
    }
};
} // namespace KashipanEngine