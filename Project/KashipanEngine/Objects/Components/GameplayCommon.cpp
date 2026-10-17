#include "Objects/Components/GameplayComponents.h"
#include <cmath>
#include <set>
#include "Objects/EmptyObject.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Scene/Components/GameEventRouter.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Scene/SceneContext.h"
#include "Utilities/ShortTextProgram.h"

namespace KashipanEngine::Gameplay {

const JSON *Fields::Get(const char *key) const {
    return in_ && in_->is_object() && in_->contains(key) ? &(*in_)[key] : nullptr;
}
bool Fields::Float(const char *key, float &value, float low, float high) {
    if (out_) { (*out_)[key] = value; return true; }
    const auto *v = Get(key);
    if (!v) return true;
    if (!v->is_number()) return false;
    const double n = v->get<double>();
    if (!std::isfinite(n) || n < low || n > high) return false;
    value = static_cast<float>(n); return true;
}
bool Fields::Int(const char *key, int &value, int low, int high) {
    if (out_) { (*out_)[key] = value; return true; }
    const auto *v = Get(key);
    if (!v) return true;
    if (!v->is_number_integer()) return false;
    const auto n = v->get<long long>();
    if (n < low || n > high) return false;
    value = static_cast<int>(n); return true;
}
bool Fields::Bool(const char *key, bool &value) {
    if (out_) { (*out_)[key] = value; return true; }
    const auto *v = Get(key);
    if (!v) return true;
    if (!v->is_boolean()) return false;
    value = v->get<bool>(); return true;
}
bool Fields::Text(const char *key, std::string &value, size_t maxBytes) {
    if (out_) { (*out_)[key] = value; return true; }
    const auto *v = Get(key);
    if (!v) return true;
    if (!v->is_string() || v->get<std::string>().size() > maxBytes) return false;
    for (unsigned char c : v->get<std::string>()) if ((c < 32 && c != '\n' && c != '\t') || c == 127) return false;
    value = v->get<std::string>(); return true;
}
bool Fields::EventName(const char *key, std::string &value) {
    std::string next = value;
    if (!Text(key, next, GameEvents::MaxNameBytes)) return false;
    if (!next.empty() && !GameEvents::IsValidName(next)) return false;
    value = next; return true;
}
bool Fields::Vec3(const char *key, Vector3 &value) {
    if (out_) { (*out_)[key] = JSON::array({value.x, value.y, value.z}); return true; }
    const auto *v = Get(key);
    if (!v) return true;
    if (!v->is_array() || v->size() != 3) return false;
    float c[3];
    for (int i = 0; i < 3; ++i) {
        if (!(*v)[i].is_number()) return false;
        const double n = (*v)[i].get<double>();
        if (!std::isfinite(n) || std::abs(n) > 1.0e6) return false;
        c[i] = static_cast<float>(n);
    }
    value = Vector3(c[0], c[1], c[2]); return true;
}
bool Fields::KeyCode(const char *key, Key &value) {
    if (out_) { (*out_)[key] = ShortText::KeyName(value); return true; }
    const auto *v = Get(key);
    if (!v) return true;
    if (!v->is_string()) return false;
    const Key parsed = ShortText::ParseKey(v->get<std::string>());
    if (parsed == Key::Unknown) return false;
    value = parsed; return true;
}
bool Fields::Raw(const char *key, JSON &value) {
    if (out_) { (*out_)[key] = value; return true; }
    const auto *v = Get(key);
    if (v) value = *v;
    return true;
}

bool IsPlaying(SceneContext *scene) { return scene && scene->IsPlaying(); }

std::vector<EmptyObject *> QueryContacts(const EmptyObject *owner, SceneContext *scene) {
    std::vector<EmptyObject *> result;
    if (!owner || !scene) return result;
    auto *collisions = scene->GetComponent<SceneObjectCollider>();
    if (!collisions) return result;
    std::set<EmptyObject *> seen;
    auto collect = [&](const auto &hits) {
        for (const auto &hit : hits) {
            auto *other = hit.otherObject;
            if (!other || other == owner || result.size() >= 32 || !seen.insert(other).second) continue;
            result.push_back(other);
        }
    };
    for (const auto &[component, addedID] : owner->GetAllComponents()) {
        (void)addedID;
        auto *collider = dynamic_cast<ICollider *>(component);
        if (!collider || !collider->IsActive()) continue;
        if (collider->Is2D()) {
            if (auto shape = collider->BuildColliderInfo2D()) { shape->sourceCollider = collider; collect(collisions->Query2D(*shape, true, owner)); }
        } else if (auto shape = collider->BuildColliderInfo3D()) {
            shape->sourceCollider = collider;
            collect(collisions->Query3D(*shape, {}, Quaternion::Identity(), true, owner));
        }
    }
    return result;
}

EmptyObject *ResolveObject(SceneContext *scene, const std::string &reference, std::string *error) {
    if (!scene || reference.empty()) return nullptr;
    const UUID128 id(reference);
    if (id.IsValid()) {
        if (auto *object = scene->GetSceneObject(id)) return object;
        if (error) *error = "ID " + reference + " のオブジェクトがありません。";
        return nullptr;
    }
    const auto matches = scene->GetSceneObjects(reference);
    if (matches.size() == 1) return matches.front();
    if (error) *error = matches.empty() ? "オブジェクト「" + reference + "」がありません。" : "オブジェクト「" + reference + "」が複数あります。UUIDで指定してください。";
    return nullptr;
}

Vector3 WorldPosition(EmptyObject *object) {
    auto *transform = object ? object->GetComponent<Transform>() : nullptr;
    return transform ? transform->GetWorldPosition() : Vector3(0, 0, 0);
}

void SetWorldPosition(EmptyObject *object, const Vector3 &world) {
    auto *transform = object ? object->GetComponent<Transform>() : nullptr;
    if (!transform) return;
    Vector3 local = world;
    if (auto *parent = transform->GetParentObject()) if (auto *parentTransform = parent->GetComponent<Transform>())
        local = world.Transform(parentTransform->GetWorldMatrix().Inverse());
    if (std::isfinite(local.x) && std::isfinite(local.y) && std::isfinite(local.z)) transform->SetTranslate(local);
}

std::string IdOf(const EmptyObject *object) { return object ? object->GetObjectID().ToString() : std::string{}; }

bool EmitEvent(SceneContext *scene, const EmptyObject *sender, const std::string &eventName, const GameEvents::Value &payload, std::string *error) {
    if (eventName.empty()) return true;
    auto *router = GameEventRouter::Find(scene);
    if (!router) { if (error) *error = "シーンに GameEventRouter がありません（イベント「" + eventName + "」を送れません）。"; return false; }
    GameEvents::Value full = payload;
    if (!full.contains("sender")) full["sender"] = IdOf(sender);
    return router->Emit(eventName, full, IdOf(sender), GameEvents::TargetMode::Broadcast, "", error, true);
}

void RemoveOwner(SceneContext *scene, EmptyObject *owner, int mode) {
    if (!scene || !owner) return;
    if (mode == 1) owner->SetActive(false);
    else if (mode == 2) scene->DeleteObject(owner);
}
} // namespace KashipanEngine::Gameplay
