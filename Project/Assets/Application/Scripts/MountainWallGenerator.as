// 山の壁面ステージ生成サンプル（WaveFunctionCollapse 名前ベースAPIの利用例）
//
// 3D横スクロールの背景として、奥行のあるごつごつした山の壁面を生成する。
// WFCで各マスの奥行段階（手前に出っ張り / 中段 / 奥にへこみ）を抽選し、
// 「手前に出っ張った形状」の上下左右の縁に装飾（結晶・岩のトゲ）を配置する。
//
// まとまりのある凹凸になる仕組み:
//   - RockFront(出っ張り)とRockBack(へこみ)は直接隣接できず、必ずRockMid(中段)を挟む
//     接続ルールにしている。これにより1マス単位のバラバラなノイズではなく、
//     中段で縁取りされた「まとまった形」の出っ張り/へこみになる。
//   - RockFront2/RockBack2はFront/Backと同じ接続を持つ複製タイルで、Midより
//     出現しやすくするための重み付けの代わり。複製数を増やすほど塊が大きくなる。
//   - 全タイルが全方向でRockMidと接続できるため、どんな制約の組み合わせでも
//     候補にRockMidが必ず残り、Solve()は矛盾で失敗しない（リトライ不要）。
class MountainWallGenerator : ScriptComponentBehavior {
    [SerializeField]
    uint seed = 12345;
    [SerializeField]
    uint wallWidth = 24;    // X方向のタイル数
    [SerializeField]
    uint wallHeight = 8;    // Y方向のタイル数
    [SerializeField]
    float tileSize = 2.0f;  // タイル1個分のワールドサイズ
    [SerializeField]
    float stepDepth = 1.0f; // 奥行1段あたりのZオフセット量
    [SerializeField]
    float decorationChance = 0.8f; // 出っ張りの縁1辺あたりの装飾配置率(0.0～1.0)
    [SerializeField]
    string pipelineName = "Object3D.Solid.BlendNormal";

    bool generated = false;

    void Start() {
        if (generated) return;
        generated = true;
        Generate();
    }

    void Generate() {
        // 1. WFCのセットアップ（壁面はXY平面の2次元グリッドとして扱う）
        WaveFunctionCollapse@ wfc = WaveFunctionCollapse();
        wfc.SetSeed(seed);
        wfc.SetGridSize(wallWidth, wallHeight, 1);

        array<string> frontTiles = {"RockFront", "RockFront2"};
        array<string> midTiles = {"RockMid"};
        array<string> backTiles = {"RockBack", "RockBack2"};
        RegisterGroup(wfc, frontTiles);
        RegisterGroup(wfc, midTiles);
        RegisterGroup(wfc, backTiles);

        // FrontとBackを直接繋がず、必ずMidを経由させる（凹凸の縁取り＝まとまりの正体）
        ConnectGroups(wfc, frontTiles, frontTiles);
        ConnectGroups(wfc, frontTiles, midTiles);
        ConnectGroups(wfc, midTiles, midTiles);
        ConnectGroups(wfc, midTiles, backTiles);
        ConnectGroups(wfc, backTiles, backTiles);

        // 2. 崩壊を解決する
        if (!wfc.Solve()) {
            LogError("MountainWallGenerator: WaveFunctionCollapseの解決に失敗しました");
            return;
        }

        uint boxMesh = GetModelHandleFromAssetPath("PrimitiveMesh-Box");
        uint crystalMesh = GetModelHandleFromAssetPath("PrimitiveMesh-Octahedron");
        uint spikeMesh = GetModelHandleFromAssetPath("PrimitiveMesh-Cone");

        // 3. 壁ブロックを配置する（タイル名から奥行段階を決める）
        for (uint x = 0; x < wallWidth; x++) {
            for (uint y = 0; y < wallHeight; y++) {
                string tileName;
                if (!wfc.TryGetResolvedTile(x, y, 0, tileName)) continue;
                CreateBlock(boxMesh, x, y, DepthOf(tileName));
            }
        }

        // 4. 装飾を配置する（出っ張りタイルの上下左右のうち、隣が出っ張りでない縁のみ）
        int decorationCount = 0;
        for (uint x = 0; x < wallWidth; x++) {
            for (uint y = 0; y < wallHeight; y++) {
                string tileName;
                if (!wfc.TryGetResolvedTile(x, y, 0, tileName)) continue;
                if (!IsFrontTile(tileName)) continue;
                decorationCount += PlaceEdgeDecorations(wfc, crystalMesh, spikeMesh, x, y);
            }
        }

        Log("MountainWallGenerator: 生成完了 (" + (wallWidth * wallHeight) + "ブロック, 装飾" + decorationCount + "個)");
    }

    //==================================================
    // WFCタイル定義のヘルパー
    //==================================================

    void RegisterGroup(WaveFunctionCollapse@ wfc, const array<string> &in names) {
        for (uint i = 0; i < names.length(); i++) {
            wfc.RegisterTile(names[i]);
        }
    }

    /// 2つのグループ間の全ペアを、上下左右の4方向で相互に接続する
    void ConnectGroups(WaveFunctionCollapse@ wfc, const array<string> &in groupA, const array<string> &in groupB) {
        for (uint i = 0; i < groupA.length(); i++) {
            for (uint j = 0; j < groupB.length(); j++) {
                ConnectMutual(wfc, groupA[i], groupB[j]);
            }
        }
    }

    /// aとbを上下左右の4方向で双方向に接続する（接続は片方向宣言のため両側へ追加する）
    void ConnectMutual(WaveFunctionCollapse@ wfc, const string &in a, const string &in b) {
        wfc.AddTileConnection(a, WFCDirection::Up, b);
        wfc.AddTileConnection(a, WFCDirection::Down, b);
        wfc.AddTileConnection(a, WFCDirection::Left, b);
        wfc.AddTileConnection(a, WFCDirection::Right, b);
        wfc.AddTileConnection(b, WFCDirection::Up, a);
        wfc.AddTileConnection(b, WFCDirection::Down, a);
        wfc.AddTileConnection(b, WFCDirection::Left, a);
        wfc.AddTileConnection(b, WFCDirection::Right, a);
    }

    bool IsFrontTile(const string &in name) {
        return name.findFirst("RockFront") == 0;
    }

    /// タイル名から奥行のZオフセットを決める（手前=0、奥ほど+Z）
    float DepthOf(const string &in name) {
        if (IsFrontTile(name)) return 0.0f;                    // 手前に出っ張り
        if (name.findFirst("RockMid") == 0) return stepDepth;  // 中段
        return stepDepth * 2.0f;                               // 奥にへこみ
    }

    //==================================================
    // オブジェクト配置
    //==================================================

    /// (x,y)の出っ張りタイルについて、上下左右の縁を調べて装飾を置く
    int PlaceEdgeDecorations(WaveFunctionCollapse@ wfc, uint crystalMesh, uint spikeMesh, uint x, uint y) {
        int placed = 0;
        array<int> dx = {0, 0, -1, 1};
        array<int> dy = {1, -1, 0, 0};
        for (int i = 0; i < 4; i++) {
            int nx = int(x) + dx[i];
            int ny = int(y) + dy[i];

            // 隣も出っ張りなら「形状の内側の辺」なので装飾しない（縁だけに置く）
            bool neighborIsFront = false;
            if (nx >= 0 && ny >= 0 && nx < int(wallWidth) && ny < int(wallHeight)) {
                string neighborName;
                if (wfc.TryGetResolvedTile(uint(nx), uint(ny), 0, neighborName)) {
                    neighborIsFront = IsFrontTile(neighborName);
                }
            }
            if (neighborIsFront) continue;
            if (!Random::Bool(decorationChance)) continue;

            // 出っ張りブロックのこの方向の縁に、少し手前へ浮かせて配置する
            Vector3 position = Vector3(
                float(x) * tileSize + float(dx[i]) * tileSize * 0.5f,
                float(y) * tileSize + float(dy[i]) * tileSize * 0.5f,
                -tileSize * 0.25f);
            uint mesh = Random::Bool(0.5f) ? crystalMesh : spikeMesh;
            float scale = tileSize * Random::Float(0.15f, 0.3f);
            CreateDecoration(mesh, position, scale);
            placed++;
        }
        return placed;
    }

    void CreateBlock(uint meshHandle, uint x, uint y, float depth) {
        Object@ obj = GetScene().CreateObject("WallBlock");
        MeshFilter@ filter;
        MeshRenderer@ renderer;
        obj.AddComponent(@filter);
        obj.AddComponent(@renderer);
        filter.SetMeshHandle(meshHandle);
        renderer.SetPipelineName(pipelineName);

        Transform@ tr = obj.GetTransform();
        tr.SetTranslate(Vector3(float(x) * tileSize, float(y) * tileSize, depth));
        tr.SetScale(Vector3(tileSize, tileSize, tileSize));
    }

    void CreateDecoration(uint meshHandle, const Vector3 &in position, float scale) {
        Object@ obj = GetScene().CreateObject("WallDecoration");
        MeshFilter@ filter;
        MeshRenderer@ renderer;
        obj.AddComponent(@filter);
        obj.AddComponent(@renderer);
        filter.SetMeshHandle(meshHandle);
        renderer.SetPipelineName(pipelineName);

        Transform@ tr = obj.GetTransform();
        tr.SetTranslate(position);
        tr.SetScale(Vector3(scale, scale, scale));
        tr.SetRotate(Vector3(0.0f, ToRadians(Random::Float(0.0f, 360.0f)), 0.0f));
    }
}
