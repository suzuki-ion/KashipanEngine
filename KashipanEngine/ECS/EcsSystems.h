#pragma once
#include "Utilities/EntityComponentSystem/SystemManager.h"
#include "Utilities/EntityComponentSystem/ComponentStrage.h"
#include "EcsComponents.h"

namespace KashipanEngine {

class TransformSystem final : public ComponentSystem<TransformComponent> {
protected:
    void UpdateEntity(const Entity &entity, ComponentStorage &componentStorage, float /*deltaTime*/) override {
        auto *transform = componentStorage.GetComponent<TransformComponent>(entity);
        if (!transform) return;

        if (!transform->dirty) return;

        Matrix4x4 local = Matrix4x4::Identity();
        local.MakeAffine(transform->scale, transform->rotate, transform->translate);

        if (transform->parent != Entity(-1)) {
            auto *parent = componentStorage.GetComponent<TransformComponent>(transform->parent);
            if (parent) {
                if (parent->dirty) {
                    UpdateEntity(transform->parent, componentStorage, 0.0f);
                }
                transform->world = local * parent->world;
            } else {
                transform->world = local;
            }
        } else {
            transform->world = local;
        }

        transform->dirty = false;
        ++transform->version;
    }
};

class CameraSystem final : public ComponentSystem<TransformComponent, CameraComponent> {
protected:
    void UpdateEntity(const Entity &entity, ComponentStorage &componentStorage, float /*deltaTime*/) override {
        auto *transform = componentStorage.GetComponent<TransformComponent>(entity);
        auto *camera = componentStorage.GetComponent<CameraComponent>(entity);
        if (!transform || !camera) return;

        Vector3 forward = Vector3{0.0f, 0.0f, 1.0f};
        Matrix4x4 rot = Matrix4x4::Identity();
        rot.MakeRotate(transform->rotate);
        forward = forward.Transform(rot);

        Vector3 up{0.0f, 1.0f, 0.0f};
        Vector3 eye = transform->translate;
        Vector3 target = eye + forward;

        camera->buffer.view.MakeViewMatrix(eye, target, up);
        camera->buffer.eyePosition = Vector4{eye.x, eye.y, eye.z, 1.0f};
        camera->buffer.fov = camera->fovY;

        if (camera->type == CameraComponent::CameraType::Perspective) {
            camera->buffer.projection.MakePerspectiveFovMatrix(camera->fovY, camera->aspectRatio, camera->nearClip, camera->farClip);
        } else {
            camera->buffer.projection.MakeOrthographicMatrix(camera->orthoLeft, camera->orthoTop, camera->orthoRight, camera->orthoBottom, camera->nearClip, camera->farClip);
        }

        camera->buffer.viewProjection = camera->buffer.view * camera->buffer.projection;
    }
};

} // namespace KashipanEngine
