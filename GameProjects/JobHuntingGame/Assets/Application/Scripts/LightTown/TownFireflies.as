#include "TownPresentation.as"
#include "TownState.as"

int TownFireflyCount(float energy) {
    // 所持する光1につき1灯。減少時はプールを休止し、再増加時に再利用する。
    return int(TownClamp(energy, 0, TownEnergyLimit()));
}
class TownFireflies {
    Scene@ scene; Object@ camera;
    array<Object@>@ points = array<Object@>();
    array<Object@>@ halos = array<Object@>();
    array<Object@>@ cores = array<Object@>();
    float clock = 0, extent = 10;
    void Initialize(Scene@ owner, float range) {
        @scene = owner; @camera = scene.GetObject("TownCamera"); extent = range;
    }
    Object@ Marker(const string &in name, float size, const Vector4 &in color) {
        Object@ marker = TownMesh(scene, name, "PrimitiveMesh-Circle-XY", Vector3(), Vector3(size, size, size), color);
        MeshRenderer@ renderer; marker.GetComponent(@renderer);
        renderer.SetMaterialName("TownFirefly"); renderer.SetPipelineName("Town.Firefly"); renderer.SetCastShadows(false);
        return marker;
    }
    Vector3 Position(uint i) {
        float phase = float(i) * 2.399963f;
        float speed = 0.11f + float(i % 5) * 0.013f;
        // 個数が変わっても既存の灯りを飛ばさず、任意の個数で街全体へ均等に分散する。
        float u=float(i)*.618033989f+.12f;u-=Floor(u);
        float radius = extent * Sqrt(u);
        return Vector3(Cos(phase + clock * speed) * radius + Sin(clock * 0.37f + phase) * 0.7f,
            0.6f + float(i % 4) * 0.35f + (Sin(clock * 0.8f + phase) + 1) * 0.35f,
            Sin(phase + clock * speed) * radius + Cos(clock * 0.29f + phase) * 0.7f);
    }
    void Update(float dt, float energy) {
        clock += dt; int count = TownFireflyCount(energy);
        while (points.length() < uint(count)) {
            Object@ point = scene.CreateObject("TownFirefly");
            Light@ light; LightRenderer@ renderer; point.AddComponent(@light); point.AddComponent(@renderer);
            light.SetType(LightType::Point); light.SetColor(Vector4(1, 0.8f, 0.2f, 1));
            light.SetRadius(2.2f); light.SetDecay(2); light.SetIntensity(0.036f); light.SetCastShadows(false);
            renderer.SetTargetObject(scene.GetObject("TownScreen"));
            points.insertLast(point);
            halos.insertLast(Marker("TownFireflyHalo", 0.4f, Vector4(1, 0.75f, 0.12f, 0.14f)));
            cores.insertLast(Marker("TownFireflyCore", 0.1f, Vector4(1, 0.95f, 0.5f, 0.9f)));
        }
        Quaternion facing = camera.GetTransform().GetRotateQuaternion();
        for (uint i = 0; i < points.length(); ++i) {
            bool active = i < uint(count);
            points[i].SetActive(active); halos[i].SetActive(active); cores[i].SetActive(active);
            if (!active) continue;
            Vector3 position = Position(i);
            points[i].GetTransform().SetTranslate(position);
            halos[i].GetTransform().SetTranslate(position); cores[i].GetTransform().SetTranslate(position);
            // スクリーンに平行な円盤。両面描画でどちら側から見ても同じ表示。
            halos[i].GetTransform().SetRotateQuaternion(facing); cores[i].GetTransform().SetRotateQuaternion(facing);
            float pulse = 0.65f + 0.35f * Sin(clock * 2 + float(i) * 1.7f);
            Light@ light; points[i].GetComponent(@light); light.SetIntensity(0.024f + pulse * 0.024f);
            MeshRenderer@ glow; halos[i].GetComponent(@glow); glow.SetInstanceColor(Vector4(1, 0.75f, 0.12f, 0.1f + pulse * 0.12f));
        }
    }
}
