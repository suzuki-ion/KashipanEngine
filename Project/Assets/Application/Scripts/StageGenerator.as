// StageGraphGenerator（部屋グラフ生成）→ StageGridBuilder（グリッド展開）→
// WaveFunctionCollapse（装飾解決）→ オブジェクト配置、という一連の流れのデモスクリプト。
//
// タイルの接続ルールはこの例では説明のためにあえて単純化しており、
// 「登録した全タイルが全方向で互いに接続可能」としている（矛盾が起きないことを優先した設定）。
// 実際のゲームで使う場合は、壁と床が正しく噛み合うようにタイルごとの接続ルールを作り込むこと。
class StageGenerator : ScriptComponentBehavior {
    [SerializeField]
    uint32 seed = 12345;

    // 抽象部屋グリッドのサイズ（メインルートの長さ・上下スロット数・奥行スロット数）
    [SerializeField]
    uint32 mainLength = 6;
    [SerializeField]
    uint32 verticalSlots = 3;
    [SerializeField]
    uint32 depthSlots = 2;
    [SerializeField]
    float branchProbability = 0.35f;

    // 部屋1つ分のタイルサイズ・部屋同士の隙間・通路の太さ
    [SerializeField]
    uint32 roomSizeX = 3;
    [SerializeField]
    uint32 roomSizeY = 2;
    [SerializeField]
    uint32 roomSizeZ = 3;
    [SerializeField]
    uint32 roomSpacing = 1;
    [SerializeField]
    uint32 corridorWidth = 1;
    [SerializeField]
    float tileWorldSize = 2.0f;

    [SerializeField]
    string pipelineName = "Object3D.Solid.BlendNormal";

    bool generated = false;

    // タイルID（この例専用の割り当て。実際のゲームではタイルセットに応じて設計し直すこと）
    uint32 kRoomFloor = 0;
    uint32 kBuildingFloor = 1;
    uint32 kGimmickFloor = 2;
    uint32 kTreasureFloor = 3;
    uint32 kCorridorFloor = 4;
    uint32 kWall = 5;
    uint32 kEmpty = 6;
    uint32 kTileCount = 7;

    void Start() {
        if (generated) return;
        generated = true;
        GenerateStage();
    }

    void GenerateStage() {
        // 1. 抽象部屋グラフを生成する（スタート→ゴールのメインルートは必ず繋がる）
        StageGraphGenerator@ graph = StageGraphGenerator();
        graph.SetSeed(seed);
        graph.SetGridSize(mainLength, verticalSlots, depthSlots);
        graph.SetBranchProbability(branchProbability);
        graph.AddSideRoomType(RoomType::Building, 1.0f);
        graph.AddSideRoomType(RoomType::GimmickDepth, 1.0f);
        graph.AddSideRoomType(RoomType::Treasure, 1.5f);
        graph.Generate();

        // 2. WaveFunctionCollapseへタイルを登録する
        WaveFunctionCollapse@ wfc = WaveFunctionCollapse();
        wfc.SetSeed(seed);
        for (uint32 i = 0; i < kTileCount; ++i) {
            wfc.RegisterTile(i);
        }
        for (uint32 a = 0; a < kTileCount; ++a) {
            for (uint32 b = 0; b < kTileCount; ++b) {
                wfc.AddTileConnection(a, WFCDirection::Up, b);
                wfc.AddTileConnection(a, WFCDirection::Down, b);
                wfc.AddTileConnection(a, WFCDirection::Left, b);
                wfc.AddTileConnection(a, WFCDirection::Right, b);
                wfc.AddTileConnection(a, WFCDirection::Front, b);
                wfc.AddTileConnection(a, WFCDirection::Back, b);
            }
        }

        // 3. 部屋グラフを実際のタイルグリッドへ展開する（部屋・通路タイルを固定する）
        StageGridBuilder@ builder = StageGridBuilder();
        builder.SetRoomSize(roomSizeX, roomSizeY, roomSizeZ);
        builder.SetRoomSpacing(roomSpacing);
        builder.SetCorridorWidth(corridorWidth);
        builder.SetTileWorldSize(tileWorldSize);
        builder.SetDefaultRoomTileID(kRoomFloor);
        builder.SetRoomTileID(RoomType::Building, kBuildingFloor);
        builder.SetRoomTileID(RoomType::GimmickDepth, kGimmickFloor);
        builder.SetRoomTileID(RoomType::Treasure, kTreasureFloor);
        builder.SetCorridorTileID(kCorridorFloor);

        if (!builder.Build(graph, wfc)) {
            LogError("StageGenerator: グリッド展開に失敗しました");
            return;
        }

        // 4. 部屋・通路以外（隙間）の装飾をWaveFunctionCollapseで解決する
        if (!wfc.Solve()) {
            LogError("StageGenerator: WaveFunctionCollapseの解決に失敗しました");
            return;
        }

        // 5. 解決結果を元に、タイル1つにつき1オブジェクトを配置する
        //    （タイル数が多い大きなステージでは、インスタンシング等でのバッチ描画に置き換えることを推奨）
        uint32 meshHandle = GetModelHandleFromAssetPath("PrimitiveMesh-Box");
        uint32 gw = wfc.GetGridWidth();
        uint32 gh = wfc.GetGridHeight();
        uint32 gd = wfc.GetGridDepth();
        for (uint32 x = 0; x < gw; ++x) {
            for (uint32 y = 0; y < gh; ++y) {
                for (uint32 z = 0; z < gd; ++z) {
                    uint32 tileID;
                    if (!wfc.TryGetResolvedTile(x, y, z, tileID)) continue;
                    if (tileID == kEmpty) continue; // 何も置かない

                    Object@ tileObject = GetScene().CreateObject("StageTile");
                    MeshFilter@ meshFilter;
                    MeshRenderer@ meshRenderer;
                    tileObject.AddComponent(@meshFilter);
                    tileObject.AddComponent(@meshRenderer);
                    meshFilter.SetMeshHandle(meshHandle);
                    meshRenderer.SetPipelineName(pipelineName);

                    Transform@ tr = tileObject.GetTransform();
                    tr.SetTranslate(Vector3(float(x) * tileWorldSize, float(y) * tileWorldSize, float(z) * tileWorldSize));
                }
            }
        }

        // 6. 部屋種別ごとに目印となるオブジェクトを配置する
        //    （実際のゲームでは、Buildingを別マップへの入り口に、GimmickDepthを固有ギミックに、
        //      Treasureを宝箱の取得ロジックに、それぞれ差し替えて使う想定）
        uint32 roomCount = graph.GetRoomCount();
        for (uint32 i = 0; i < roomCount; ++i) {
            RoomType roomType = graph.GetRoomType(i);
            string markerMeshPath;
            if (roomType == RoomType::Treasure) {
                markerMeshPath = "PrimitiveMesh-Octahedron";
            } else if (roomType == RoomType::Building) {
                markerMeshPath = "PrimitiveMesh-Cone";
            } else if (roomType == RoomType::GimmickDepth) {
                markerMeshPath = "PrimitiveMesh-UVSphere";
            } else {
                continue;
            }

            uint32 roomID = graph.GetRoomID(i);
            Vector3 worldPos;
            if (!builder.TryGetRoomWorldCenter(graph, roomID, worldPos)) continue;
            worldPos.y += tileWorldSize; // 床の上に乗るよう少し持ち上げる

            Object@ markerObject = GetScene().CreateObject("StageMarker");
            MeshFilter@ markerMeshFilter;
            MeshRenderer@ markerMeshRenderer;
            markerObject.AddComponent(@markerMeshFilter);
            markerObject.AddComponent(@markerMeshRenderer);
            markerMeshFilter.SetMeshHandle(GetModelHandleFromAssetPath(markerMeshPath));
            markerMeshRenderer.SetPipelineName(pipelineName);

            Transform@ tr = markerObject.GetTransform();
            tr.SetTranslate(worldPos);
        }

        Log("StageGenerator: ステージ生成が完了しました（部屋数=" + roomCount + "）");
    }
}
