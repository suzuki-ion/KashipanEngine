// Transformを持つ管理用オブジェクトへ追加。Startで生成し、Endで生成物だけを片付ける。
// 座標は管理用オブジェクトのローカル座標。既存の子オブジェクトは変更しない。
class GridGenerator : ScriptComponentBehavior {
    [SerializeField, Range(1, 256), Tooltip("X方向のマス数")]
    int cellsX = 20;
    [SerializeField, Range(1, 256), Tooltip("Z方向（XY平面ではY方向）のマス数")]
    int cellsZ = 20;
    [SerializeField, Range(0.01, 1000)]
    float spacing = 1.0f;
    [SerializeField, Range(0.001, 10)]
    float thickness = 0.02f;
    [SerializeField, Tooltip("false: XZ平面、true: XY平面")]
    bool xyPlane = false;
    [SerializeField, Tooltip("グリッド中心のローカル位置。床との重なりを避ける場合は少し浮かせる")]
    Vector3 center = Vector3(0, 0.01f, 0);
    [SerializeField, ColorPicker]
    Vector4 color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
    [SerializeField]
    string pipelineName = "Object3D.Solid.BlendNormal";
    [SerializeField, Tooltip("空欄は標準マテリアル")]
    string materialName = "";

    array<Object@>@ lines = null;

    void Start() {
        if (lines !is null) return;
        // NaNも拒否するため、許容範囲に入ることを確認する。
        if (cellsX < 1 || cellsX > 256 || cellsZ < 1 || cellsZ > 256 ||
            !(spacing >= 0.01f && spacing <= 1000.0f) ||
            !(thickness >= 0.001f && thickness <= 10.0f)) {
            LogError("GridGenerator: マス数・間隔・太さが許容範囲外です");
            return;
        }
        @lines = array<Object@>();
        uint mesh = GetModelHandleFromAssetPath("PrimitiveMesh-Box");
        float width = float(cellsX) * spacing;
        float depth = float(cellsZ) * spacing;
        for (int x = 0; x <= cellsX; ++x) {
            float offset = float(x) * spacing - width * 0.5f;
            Vector3 size = xyPlane ? Vector3(thickness, depth + thickness, thickness)
                                   : Vector3(thickness, thickness, depth + thickness);
            if (!CreateLine(mesh, center + Vector3(offset, 0, 0), size)) {
                End();
                return;
            }
        }
        for (int z = 0; z <= cellsZ; ++z) {
            float offset = float(z) * spacing - depth * 0.5f;
            Vector3 position = xyPlane ? Vector3(0, offset, 0) : Vector3(0, 0, offset);
            if (!CreateLine(mesh, center + position, Vector3(width + thickness, thickness, thickness))) {
                End();
                return;
            }
        }
    }

    bool CreateLine(uint mesh, const Vector3 &in position, const Vector3 &in size) {
        Object@ line = GetScene().CreateObject("GeneratedGridLine");
        if (line is null) return false;
        lines.insertLast(line);
        MeshFilter@ filter;
        MeshRenderer@ renderer;
        Transform@ transform = line.GetTransform();
        if (transform is null || !line.AddComponent(@filter) || !line.AddComponent(@renderer)) {
            LogError("GridGenerator: 線のコンポーネントを作成できませんでした");
            return false;
        }
        if (!transform.SetParentObject(GetOwnerObject())) return false;
        transform.SetTranslate(position);
        transform.SetScale(size);
        filter.SetMeshHandle(mesh);
        if (!filter.HasMesh()) {
            LogError("GridGenerator: PrimitiveMesh-Boxを取得できませんでした");
            return false;
        }
        renderer.SetPipelineName(pipelineName);
        if (materialName != "") renderer.SetMaterialName(materialName);
        renderer.SetInstanceColor(color);
        renderer.SetCastShadows(false);
        renderer.SetAllowInstancing(true);
        return true;
    }

    void End() {
        if (lines is null) return;
        for (uint i = 0; i < lines.length(); ++i) {
            if (IsValidObject(lines[i])) GetScene().DeleteObject(lines[i]);
        }
        @lines = null;
    }
}
