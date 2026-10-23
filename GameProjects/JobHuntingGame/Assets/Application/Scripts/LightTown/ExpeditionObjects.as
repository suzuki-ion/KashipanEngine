#include "TownPresentation.as"

Object@ ExpeditionFloor(Scene@ scene, const string &in name, const Vector3 &in pos, const Vector3 &in scale,
                        const Vector4 &in color, float angle = 0) {
    Object@ obj = scene.CloneObject(scene.GetObject("ExpeditionGroundTemplate"), name);
    if (obj is null) { LogError("ExpeditionGroundTemplateがありません。"); return null; }
    obj.GetTransform().SetTranslate(pos); obj.GetTransform().SetScale(scale);
    obj.GetTransform().SetRotate(Vector3(0, 0, angle)); obj.SetActive(true);
    // 旧Stage1/GameSceneの地面と同じ白いDefault材質。素材の色分けは収集物側で行う。
    MeshRenderer@ mesh; obj.GetComponent(@mesh); mesh.SetInstanceColor(Vector4(1, 1, 1, 1));
    return obj;
}
// 複製した階層内の部品だけを検索し、別インスタンスへの参照を防ぐ。
Object@ ExpeditionPart(Scene@ scene, Object@ root, const string &in name) {
    array<Object@>@ candidates = scene.GetObjects(name);
    for (uint i = 0; i < candidates.length(); ++i) {
        Object@ parent = candidates[i].GetTransform().GetParentObject();
        while (parent !is null) {
            if (root !is null && parent.GetUUID() == root.GetUUID()) return candidates[i];
            @parent = parent.GetTransform().GetParentObject();
        }
    }
    return null;
}
Object@ ExpeditionPrefab(Scene@ scene, const string &in type, const string &in name, const Vector3 &in pos) {
    Object@ obj = scene.CloneObject(scene.GetObject("Expedition" + type + "Prefab"), name, true);
    if (obj is null) { LogError("生成用プレハブがありません: " + type); return null; }
    obj.GetTransform().SetTranslate(pos); obj.SetActive(true);
    return obj;
}
void ExpeditionMovingFloor(Scene@ scene, const Vector3 &in pos, float travel = 5, float scale = 1) {
    Object@ root = ExpeditionPrefab(scene, "Moving", "GeneratedMovingFloor", pos);
    if(root is null)return;root.GetTransform().SetScale(Vector3(scale,scale,scale));
    Object@ obj = ExpeditionPart(scene, root, "PrefabMovingPart1");
    ScriptComponent@ script; if (obj is null || !obj.GetComponent(@script)) return;
    script.SetVariable("moveRangeMin", Vector3(0, 0, 0));
    script.SetVariable("moveRangeMax", Vector3(0, travel, 0));
    script.SetVariable("moveSpeed", Vector3(0, 1.8f, 0));
    script.SetVariable("moveStopDuration", 0.6f);
}
void ExpeditionRotatingFloor(Scene@ scene, const Vector3 &in pos, float scale = 1) {
    Object@ root=ExpeditionPrefab(scene, "Rotating", "GeneratedRotatingFloor", pos);
    if(root !is null)root.GetTransform().SetScale(Vector3(scale,scale,scale));
}
void ExpeditionCheckpoint(Scene@ scene, const Vector3 &in pos, int priority) {
    Object@ obj = ExpeditionPrefab(scene, "Checkpoint", "GeneratedCheckpoint", pos);
    if (obj is null) return;
    ScriptComponent@ script; obj.GetComponent(@script);
    Object@ rotating = ExpeditionPart(scene, obj, "PrefabCheckpointPart1");
    Object@ light = ExpeditionPart(scene, obj, "PrefabCheckpointPart2");
    script.SetVariable("priority", priority);
    script.SetVariable("rotatingObject", @rotating); script.SetVariable("lightObject", @light);
}
void ExpeditionTeleport(Scene@ scene, const Vector3 &in pos, const Vector3 &in destination) {
    Object@ root = ExpeditionPrefab(scene, "Teleport", "GeneratedTeleporter", pos);
    Object@ obj = ExpeditionPart(scene, root, "PrefabTeleportPart1");
    Object@ exit = ExpeditionPart(scene, root, "PrefabTeleportPart3");
    if (obj is null || exit is null) return;
    exit.GetTransform().SetTranslate(destination - pos);
    ScriptComponent@ script; obj.GetComponent(@script);
    script.SetVariable("targetObject", @exit); script.SetVariable("offset", Vector3(0, 0, 0));
    script.SetVariable("requiredObjectTag", "Player");
    script.SetVariable("deferTeleport", true);
    CapsuleCollider@ trigger; if (obj.GetComponent(@trigger)) trigger.SetTrigger(true);
}
void ExpeditionEnemy(Scene@ scene, const Vector3 &in pos) {
    ExpeditionPrefab(scene, "Enemy", "GeneratedEnemy", pos);
}
void ExpeditionStreetlight(Scene@ scene, const Vector3 &in pos, bool tall) {
    Object@ root = scene.CloneObject(scene.GetObject(tall ? "HistoricalStreetlight1Template" : "HistoricalStreetlight2Template"), "GeneratedStreetlight", true);
    if (root is null) return;
    root.GetTransform().SetTranslate(pos); root.SetActive(true);
}
void ExpeditionFactoryLight(Scene@ scene, const Vector3 &in pos, bool fluorescent) {
    Object@ root = scene.CloneObject(scene.GetObject(fluorescent ? "HistoricalFluorescentTemplate" : "HistoricalWallLampTemplate"),
        fluorescent ? "GeneratedFluorescent" : "GeneratedWallLamp", true);
    if (root is null) { LogError("旧GameSceneの照明テンプレートがありません。"); return; }
    root.GetTransform().SetTranslate(pos); root.SetActive(true);
}
