// 横スクロール用の背景（前景: 浮遊モノリス / 中景: 工場 / 遠景: 工場シルエット）を
// プリミティブメッシュだけで自動生成するエディターツール。
//
// メニューバーの Tools > Background Generator からウィンドウを開き、"Generate" で生成する。
//
// - 生成物は全て Root Name（既定 "BG_Root"）のオブジェクト配下へまとめて作られる。
//   "Generate" / "Clear" は同名のルートを子孫ごと削除してから処理するため、何度でも作り直せる
// - 同じ Seed・同じパラメータなら毎回同じ背景になる（レイヤーごとに乱数系列を分けているため、
//   あるレイヤーのパラメータを変えても他のレイヤーの配置は変わらない）
// - アニメーションは KeyFrameAnimator コンポーネントで行う。使用するキーフレームjson
//   （浮遊・回転・点滅）は生成時に Animation Folder へ書き出される
//   （Keyframe Animation Editor で開いて調整することもできる。ただし再生成で上書きされる）
// - アニメーションはゲームループの再生中のみ動く（playOnStart）
// - パラメータは "Save Settings" / "Load Settings" で Settings Path のjsonへ保存/読み込みできる
//   （最初にウィンドウを開いたとき、ファイルがあれば自動で読み込む）

/// @brief シード付きの擬似乱数（xorshift32）。Random:: はシード指定できないため自前で持つ
class BGRandom {
    uint state = 0x9E3779B9;

    BGRandom() {}
    BGRandom(uint seed) { Reset(seed); }

    void Reset(uint seed) {
        state = (seed == 0) ? 0x9E3779B9 : seed;
        // 近いシード同士でも系列がばらけるよう、最初の数回を捨てる
        for (int i = 0; i < 8; i++) Next();
    }

    uint Next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    /// @brief [0, 1) の一様乱数
    float Float01() { return float(Next() >> 8) / 16777216.0f; }
    /// @brief [minValue, maxValue) の一様乱数
    float Range(float minValue, float maxValue) { return minValue + (maxValue - minValue) * Float01(); }
    /// @brief [minValue, maxValue] の整数乱数
    int RangeInt(int minValue, int maxValue) {
        if (maxValue <= minValue) return minValue;
        return minValue + int(Next() % uint(maxValue - minValue + 1));
    }
    /// @brief probabilityの確率でtrue
    bool Chance(float probability) { return Float01() < probability; }
    /// @brief 1 または -1
    float Sign() { return Chance(0.5f) ? 1.0f : -1.0f; }
}

[EditorWindow("Background Generator")]
[MenuItem("MenuBar/Tools/Background Generator", "open_background_generator")]
class BackgroundGenerator : EditorTool {
    //==================================================
    // 共通パラメータ
    //==================================================

    string settingsPath = "Assets/Application/Background/BackgroundGenerator.json";
    string animationFolder = "Assets/Application/Keyframe/Background";
    string rootName = "BG_Root";
    string renderTargetName = "MainScreen";
    string materialName = "Default";
    int seed = 12345;
    float rangeStartX = -30.0f;
    float rangeEndX = 250.0f;

    //==================================================
    // 前景: 浮遊モノリス
    //==================================================

    bool fgEnabled = true;
    float fgMarginX = 10.0f;
    float fgZMin = 3.0f;
    float fgZMax = 9.0f;
    float fgYMin = -6.0f;
    float fgYMax = 16.0f;
    bool fgAvoidBand = true;
    float fgAvoidYMin = -1.0f;
    float fgAvoidYMax = 6.0f;
    float fgSpacingMin = 5.0f;
    float fgSpacingMax = 12.0f;
    float fgSizeMin = 0.8f;
    float fgSizeMax = 2.5f;
    float fgFloatAmplitudeMin = 0.3f;
    float fgFloatAmplitudeMax = 1.0f;
    float fgFloatPeriodMin = 3.0f;
    float fgFloatPeriodMax = 6.0f;
    float fgSpinSpeedMin = 0.02f; // 回転/秒
    float fgSpinSpeedMax = 0.08f;
    float fgAccentRatio = 0.2f;
    bool fgCastShadows = false;
    Vector4 fgColor = Vector4(0.12f, 0.13f, 0.18f, 1.0f);
    Vector4 fgAccentColor = Vector4(0.3f, 0.9f, 1.0f, 1.0f);

    //==================================================
    // 中景: 工場
    //==================================================

    bool midEnabled = true;
    float midMarginX = 50.0f;
    float midZ = 30.0f;
    float midGroundY = -6.0f;
    bool midGroundStrip = true;
    float midGapMin = 0.5f;
    float midGapMax = 4.0f;
    float midBuildingWidthMin = 5.0f;
    float midBuildingWidthMax = 12.0f;
    float midBuildingHeightMin = 4.0f;
    float midBuildingHeightMax = 12.0f;
    float midChimneyChance = 0.35f;
    float midTankChance = 0.2f;
    float midGearChance = 0.4f;
    float midPipeChance = 0.5f;
    float midGearSpinSpeedMin = 0.05f; // 回転/秒
    float midGearSpinSpeedMax = 0.2f;
    bool midCastShadows = true;
    Vector4 midColor = Vector4(0.32f, 0.34f, 0.40f, 1.0f);
    Vector4 midDetailColor = Vector4(0.22f, 0.23f, 0.28f, 1.0f);
    Vector4 midStripeColor = Vector4(0.80f, 0.25f, 0.20f, 1.0f);
    Vector4 midWindowColor = Vector4(0.90f, 0.80f, 0.50f, 1.0f);
    Vector4 midLightColor = Vector4(1.00f, 0.55f, 0.20f, 1.0f);

    //==================================================
    // 遠景: 工場シルエット
    //==================================================

    bool farEnabled = true;
    float farMarginX = 100.0f;
    float farZ = 80.0f;
    float farGroundY = -12.0f;
    float farGapMin = 0.0f;
    float farGapMax = 6.0f;
    float farWidthMin = 6.0f;
    float farWidthMax = 16.0f;
    float farHeightMin = 12.0f;
    float farHeightMax = 40.0f;
    float farChimneyChance = 0.4f;
    Vector4 farColor = Vector4(0.55f, 0.60f, 0.70f, 1.0f);

    //==================================================
    // 実行時状態
    //==================================================

    string statusMessage = "";
    bool settingsLoadedOnce = false;
    int createdObjectCount = 0;
    Scene@ scene;
    Object@ renderTarget;
    uint meshBox = 0;
    uint meshCylinder = 0;
    uint meshSphere = 0;
    uint meshTetrahedron = 0;
    uint meshOctahedron = 0;
    uint meshIcosahedron = 0;

    // 書き出すキーフレームjsonのファイル名
    string kFloatFile = "BG_Float.json";
    string kSpinFile = "BG_Spin.json";
    string kBlinkFile = "BG_Blink.json";

    void InitializeOnLoad() {
        Log("BackgroundGenerator: 読み込まれました (Tools > Background Generator)");
    }

    void OnItemSelected(const string &in tag) {
        if (tag == "open_background_generator") {
            OpenEditorWindow("Background Generator");
        }
    }

    void OnWindowEnable(const string &in windowName) {
        // 初回に開いたときだけ読み込む（開き直すたびに読み込むと未保存の編集が巻き戻るため）
        if (settingsLoadedOnce) return;
        settingsLoadedOnce = true;
        LoadSettings(false);
    }

    void OnWindowDisable(const string &in windowName) {}

    void Update() {
        if (!IsEditorWindowOpen("Background Generator")) return;

        ShowActionSection();
        ImGui::Separator();
        ShowCommonSection();
        ShowForegroundSection();
        ShowMidSection();
        ShowFarSection();
        ShowSettingsFileSection();
    }

    //==================================================
    // UI
    //==================================================

    void ShowActionSection() {
        if (ImGui::Button("Generate", 120, 0)) {
            Generate();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("既存の生成物（Root Name のオブジェクト）を削除してから背景を生成する");
        }
        ImGui::SameLine();
        if (ImGui::Button("Random Seed")) {
            seed = Random::Int(1, 999999);
            Generate();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            @scene = GetScene();
            int removed = ClearGenerated();
            statusMessage = "削除しました: " + removed + " 個のルート";
        }
        if (statusMessage != "") {
            ImGui::TextDisabled(statusMessage);
        }
    }

    void ShowCommonSection() {
        if (!ImGui::CollapsingHeader("Common")) return;
        ImGui::InputText("Root Name", rootName);
        ImGui::InputText("Render Target", renderTargetName);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("MeshRendererの描画先オブジェクト名（ScreenBufferを持つオブジェクト）");
        }
        ImGui::InputText("Material", materialName);
        ImGui::InputText("Animation Folder", animationFolder);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("生成時にキーフレームjson（" + kFloatFile + " / " + kSpinFile + " / " + kBlinkFile + "）を書き出すフォルダ");
        }
        ImGui::DragInt("Seed", seed, 1.0f, 0, 999999);
        ImGui::DragFloat("Range Start X", rangeStartX, 1.0f);
        ImGui::DragFloat("Range End X", rangeEndX, 1.0f);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ステージのX範囲。各レイヤーはここへ Margin X を左右に足した範囲へ配置される\n（奥のレイヤーほど画面に映る横幅が広いため、Marginを大きくする）");
        }
    }

    void ShowForegroundSection() {
        if (!ImGui::CollapsingHeader("Foreground: Floating Monoliths")) return;
        ImGui::PushID("fg");
        ImGui::Checkbox("Enabled", fgEnabled);
        ImGui::DragFloat("Margin X", fgMarginX, 0.5f, 0.0f, 1000.0f);
        DragRange("Z", fgZMin, fgZMax, 0.1f);
        DragRange("Y", fgYMin, fgYMax, 0.1f);
        ImGui::Checkbox("Avoid Play Band", fgAvoidBand);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("プレイ面の高さ帯（Avoid Y）にはモノリスを置かない");
        }
        ImGui::BeginDisabled(!fgAvoidBand);
        DragRange("Avoid Y", fgAvoidYMin, fgAvoidYMax, 0.1f);
        ImGui::EndDisabled();
        DragRange("Spacing X", fgSpacingMin, fgSpacingMax, 0.1f);
        DragRange("Size", fgSizeMin, fgSizeMax, 0.05f);
        DragRange("Float Amplitude", fgFloatAmplitudeMin, fgFloatAmplitudeMax, 0.05f);
        DragRange("Float Period (s)", fgFloatPeriodMin, fgFloatPeriodMax, 0.1f);
        DragRange("Spin (rev/s)", fgSpinSpeedMin, fgSpinSpeedMax, 0.005f);
        ImGui::SliderFloat("Accent Ratio", fgAccentRatio, 0.0f, 1.0f);
        ImGui::Checkbox("Cast Shadows", fgCastShadows);
        ImGui::ColorEdit4("Color", fgColor);
        ImGui::ColorEdit4("Accent Color", fgAccentColor);
        ImGui::PopID();
    }

    void ShowMidSection() {
        if (!ImGui::CollapsingHeader("Middle: Factory")) return;
        ImGui::PushID("mid");
        ImGui::Checkbox("Enabled", midEnabled);
        ImGui::DragFloat("Margin X", midMarginX, 0.5f, 0.0f, 1000.0f);
        ImGui::DragFloat("Z", midZ, 0.5f);
        ImGui::DragFloat("Ground Y", midGroundY, 0.1f);
        ImGui::Checkbox("Ground Strip", midGroundStrip);
        DragRange("Gap X", midGapMin, midGapMax, 0.1f);
        DragRange("Building Width", midBuildingWidthMin, midBuildingWidthMax, 0.1f);
        DragRange("Building Height", midBuildingHeightMin, midBuildingHeightMax, 0.1f);
        ImGui::SliderFloat("Chimney Chance", midChimneyChance, 0.0f, 1.0f);
        ImGui::SliderFloat("Tank Chance", midTankChance, 0.0f, 1.0f);
        ImGui::SliderFloat("Gear Chance", midGearChance, 0.0f, 1.0f);
        ImGui::SliderFloat("Pipe Chance", midPipeChance, 0.0f, 1.0f);
        DragRange("Gear Spin (rev/s)", midGearSpinSpeedMin, midGearSpinSpeedMax, 0.005f);
        ImGui::Checkbox("Cast Shadows", midCastShadows);
        ImGui::ColorEdit4("Color", midColor);
        ImGui::ColorEdit4("Detail Color", midDetailColor);
        ImGui::ColorEdit4("Stripe Color", midStripeColor);
        ImGui::ColorEdit4("Window Color", midWindowColor);
        ImGui::ColorEdit4("Light Color", midLightColor);
        ImGui::PopID();
    }

    void ShowFarSection() {
        if (!ImGui::CollapsingHeader("Far: Factory Silhouette")) return;
        ImGui::PushID("far");
        ImGui::Checkbox("Enabled", farEnabled);
        ImGui::DragFloat("Margin X", farMarginX, 0.5f, 0.0f, 1000.0f);
        ImGui::DragFloat("Z", farZ, 0.5f);
        ImGui::DragFloat("Ground Y", farGroundY, 0.1f);
        DragRange("Gap X", farGapMin, farGapMax, 0.1f);
        DragRange("Width", farWidthMin, farWidthMax, 0.1f);
        DragRange("Height", farHeightMin, farHeightMax, 0.1f);
        ImGui::SliderFloat("Chimney Chance", farChimneyChance, 0.0f, 1.0f);
        ImGui::ColorEdit4("Color", farColor);
        ImGui::PopID();
    }

    void ShowSettingsFileSection() {
        if (!ImGui::CollapsingHeader("Settings File")) return;
        ImGui::InputText("Settings Path", settingsPath);
        if (ImGui::Button("Save Settings")) {
            SaveSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Settings")) {
            LoadSettings(true);
        }
    }

    /// @brief 最小/最大のペアを1行で編集する（最小 > 最大にならないよう補正する）
    void DragRange(const string &in label, float &inout minValue, float &inout maxValue, float speed) {
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("##min", minValue, speed);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("##max", maxValue, speed);
        ImGui::SameLine();
        ImGui::Text(label);
        ImGui::PopID();
        if (minValue > maxValue) maxValue = minValue;
    }

    //==================================================
    // 生成
    //==================================================

    void Generate() {
        @scene = GetScene();
        if (scene is null) {
            statusMessage = "シーンが開かれていません";
            return;
        }
        if (!PrepareMeshes()) {
            statusMessage = "プリミティブメッシュの取得に失敗しました（ログを確認してください）";
            return;
        }
        if (!WriteAnimationFiles()) {
            statusMessage = "キーフレームjsonの書き出しに失敗しました: " + animationFolder;
            return;
        }

        @renderTarget = scene.GetObject(renderTargetName);
        if (renderTarget is null) {
            LogWarning("BackgroundGenerator: 描画先オブジェクトが見つかりません: " + renderTargetName);
        }

        ClearGenerated();
        createdObjectCount = 0;

        Object@ root = CreateNode(rootName, null, Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(1, 1, 1));
        if (fgEnabled) GenerateForeground(root);
        if (midEnabled) GenerateMid(root);
        if (farEnabled) GenerateFar(root);

        statusMessage = "生成しました: " + createdObjectCount + " オブジェクト (Seed " + seed + ")";
        if (renderTarget is null) {
            statusMessage += " ※描画先 '" + renderTargetName + "' が見つからないため描画されません";
        }
        Log("BackgroundGenerator: " + statusMessage);
    }

    /// @brief 生成済みのルートを子孫ごと削除する
    /// @return 削除したルートの数
    int ClearGenerated() {
        if (scene is null) return 0;
        array<Object@>@ roots = scene.GetObjects(rootName);
        if (roots is null) return 0;
        int removed = 0;
        for (uint i = 0; i < roots.length(); i++) {
            if (roots[i] is null) continue;
            if (scene.DeleteObject(roots[i])) removed++;
        }
        return removed;
    }

    bool PrepareMeshes() {
        meshBox = GetModelHandleFromAssetPath("PrimitiveMesh-Box");
        meshCylinder = GetModelHandleFromAssetPath("PrimitiveMesh-Cylinder-Closed");
        meshSphere = GetModelHandleFromAssetPath("PrimitiveMesh-UVSphere");
        meshTetrahedron = GetModelHandleFromAssetPath("PrimitiveMesh-Tetrahedron");
        meshOctahedron = GetModelHandleFromAssetPath("PrimitiveMesh-Octahedron");
        meshIcosahedron = GetModelHandleFromAssetPath("PrimitiveMesh-Icosahedron");
        return meshBox != 0 && meshCylinder != 0 && meshSphere != 0
            && meshTetrahedron != 0 && meshOctahedron != 0 && meshIcosahedron != 0;
    }

    /// @brief 層ごとに独立した乱数系列を作る（あるレイヤーの設定変更が他のレイヤーへ波及しないように）
    BGRandom@ MakeLayerRandom(uint layerSalt) {
        return BGRandom(uint(seed) * 0x9E3779B1 + layerSalt);
    }

    //--------------------------------------------------
    // 前景: 浮遊モノリス
    //--------------------------------------------------

    void GenerateForeground(Object@ root) {
        BGRandom@ rng = MakeLayerRandom(0x1000);
        Object@ layer = CreateNode("BG_Foreground", root, Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(1, 1, 1));

        float x = rangeStartX - fgMarginX + rng.Range(0.0f, fgSpacingMax);
        const float endX = rangeEndX + fgMarginX;
        int index = 0;
        while (x <= endX) {
            float y = PickForegroundY(rng);
            float z = rng.Range(fgZMin, fgZMax);
            CreateMonolith(layer, rng, Vector3(x, y, z), index);
            index++;
            x += rng.Range(fgSpacingMin, fgSpacingMax);
        }
    }

    float PickForegroundY(BGRandom@ rng) {
        if (!fgAvoidBand) return rng.Range(fgYMin, fgYMax);
        // 回避帯の上下それぞれの区間長に比例して振り分ける
        float below = Max(0.0f, Min(fgAvoidYMin, fgYMax) - fgYMin);
        float above = Max(0.0f, fgYMax - Max(fgAvoidYMax, fgYMin));
        if (below + above <= 0.0f) return rng.Range(fgYMin, fgYMax);
        float t = rng.Range(0.0f, below + above);
        if (t < below) return fgYMin + t;
        return Max(fgAvoidYMax, fgYMin) + (t - below);
    }

    void CreateMonolith(Object@ layer, BGRandom@ rng, const Vector3 &in position, int index) {
        // 親（配置位置）→ Body（浮遊・回転アニメーション）→ 形状 の3段構成。
        // KeyFrameAnimatorはBodyのローカル値を書き換えるため、配置位置は親側に持たせる
        Object@ anchor = CreateNode("Monolith", layer, position, Vector3(0, 0, 0), Vector3(1, 1, 1));
        Object@ body = CreateNode("Body", anchor, Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(1, 1, 1));

        float size = rng.Range(fgSizeMin, fgSizeMax);
        bool accent = rng.Chance(fgAccentRatio);
        Vector4 color = accent ? fgAccentColor : fgColor;
        Vector3 tilt(rng.Range(-0.4f, 0.4f), 0.0f, rng.Range(-0.4f, 0.4f));

        int shape = rng.RangeInt(0, 4);
        if (shape <= 1) {
            // 縦長の石板（出現率を高めにする）
            Vector3 scale(size * rng.Range(0.5f, 0.8f), size * rng.Range(2.0f, 3.5f), size * rng.Range(0.2f, 0.4f));
            CreateMesh("Slab", body, meshBox, Vector3(0, 0, 0), tilt, scale, color, fgCastShadows);
        } else if (shape == 2) {
            CreateMesh("Octahedron", body, meshOctahedron, Vector3(0, 0, 0), tilt,
                Vector3(size * 1.2f, size * 1.8f, size * 1.2f), color, fgCastShadows);
        } else if (shape == 3) {
            CreateMesh("Icosahedron", body, meshIcosahedron, Vector3(0, 0, 0), tilt,
                Vector3(size * 1.4f, size * 1.4f, size * 1.4f), color, fgCastShadows);
        } else {
            CreateMesh("Tetrahedron", body, meshTetrahedron, Vector3(0, 0, 0), tilt,
                Vector3(size * 1.6f, size * 1.6f, size * 1.6f), color, fgCastShadows);
        }

        // 石板には小さな衛星を付けることがある（Bodyと一緒に浮遊・回転する）
        if (shape <= 1 && rng.Chance(0.4f)) {
            float s = size * rng.Range(0.2f, 0.35f);
            CreateMesh("Satellite", body, meshOctahedron, Vector3(size * rng.Range(0.8f, 1.3f) * rng.Sign(), size * rng.Range(-0.5f, 1.0f), 0.0f),
                Vector3(0, 0, 0), Vector3(s, s * 1.5f, s), fgAccentColor, fgCastShadows);
        }

        KeyFrameAnimator@ animator;
        if (!body.AddComponent(@animator)) return;

        // 浮遊: Float.json は周期2秒・値域[-1, 1]。振幅をValueScale、周期を再生速度で変える
        float period = rng.Range(fgFloatPeriodMin, fgFloatPeriodMax);
        AddLoopAnimation(animator, "Float", kFloatFile, 2.0f / Max(period, 0.01f));
        animator.SetValueScale("Float", rng.Range(fgFloatAmplitudeMin, fgFloatAmplitudeMax));
        animator.SetTimeOffset("Float", rng.Range(0.0f, 2.0f));
        animator.AddBinding("Float", "Transform", "translate_", 1);

        // 回転: Spin.json は1秒で1回転（0→2π）。回転/秒をそのまま再生速度にする（負なら逆回転）
        float spinSpeed = rng.Range(fgSpinSpeedMin, fgSpinSpeedMax) * rng.Sign();
        AddLoopAnimation(animator, "Spin", kSpinFile, spinSpeed);
        animator.SetTimeOffset("Spin", rng.Range(0.0f, 1.0f));
        animator.AddBinding("Spin", "Transform", "rotate_", 1);
    }

    //--------------------------------------------------
    // 中景: 工場
    //--------------------------------------------------

    void GenerateMid(Object@ root) {
        BGRandom@ rng = MakeLayerRandom(0x2000);
        Object@ layer = CreateNode("BG_FactoryMid", root, Vector3(0, midGroundY, midZ), Vector3(0, 0, 0), Vector3(1, 1, 1));

        const float startX = rangeStartX - midMarginX;
        const float endX = rangeEndX + midMarginX;

        if (midGroundStrip) {
            float length = endX - startX + 20.0f;
            CreateMesh("Ground", layer, meshBox, Vector3((startX + endX) * 0.5f, -0.5f, 0.0f), Vector3(0, 0, 0),
                Vector3(length, 1.0f, 14.0f), midDetailColor, midCastShadows);
        }

        float x = startX;
        float previousRight = 0.0f;
        float previousHeight = 0.0f;
        bool hasPrevious = false;
        while (x <= endX) {
            float width = 0.0f;
            float height = 0.0f;
            float zJitter = rng.Range(-2.0f, 2.0f);
            float roll = rng.Float01();

            if (roll < midTankChance) {
                float radius = rng.Range(1.5f, 3.5f);
                width = radius * 2.0f;
                height = CreateTank(layer, rng, Vector3(x + radius, 0.0f, zJitter), radius);
            } else if (roll < midTankChance + midChimneyChance * 0.3f) {
                // 単独の煙突（建屋の上に乗せる分とは別に、たまに地面から立てる）
                float radius = rng.Range(0.6f, 1.2f);
                width = radius * 2.0f + 1.0f;
                height = CreateChimney(layer, rng, Vector3(x + width * 0.5f, 0.0f, zJitter), radius, rng.Range(14.0f, 24.0f));
            } else {
                width = rng.Range(midBuildingWidthMin, midBuildingWidthMax);
                height = CreateBuilding(layer, rng, Vector3(x + width * 0.5f, 0.0f, zJitter), width);
            }

            // 前のパーツとの間をパイプで繋ぐ
            if (hasPrevious && rng.Chance(midPipeChance)) {
                float top = Min(previousHeight, height);
                if (top > 2.0f) {
                    float pipeY = rng.Range(1.5f, top - 0.5f);
                    float pipeStart = previousRight - 0.5f;
                    float pipeEnd = x + 0.5f;
                    float pipeLength = Max(pipeEnd - pipeStart, 1.0f);
                    float pipeRadius = rng.Range(0.2f, 0.45f);
                    CreateMesh("Pipe", layer, meshCylinder, Vector3((pipeStart + pipeEnd) * 0.5f, pipeY, -3.5f),
                        Vector3(0, 0, GetPI() * 0.5f), Vector3(pipeRadius * 2.0f, pipeLength, pipeRadius * 2.0f),
                        midDetailColor, midCastShadows);
                }
            }

            hasPrevious = true;
            previousRight = x + width;
            previousHeight = height;
            x += width + rng.Range(midGapMin, midGapMax);
        }
    }

    /// @brief 建屋（本体＋屋根＋窓帯＋任意の煙突/歯車）を生成する
    /// @return 建屋の高さ
    float CreateBuilding(Object@ layer, BGRandom@ rng, const Vector3 &in basePosition, float width) {
        float height = rng.Range(midBuildingHeightMin, midBuildingHeightMax);
        float depth = rng.Range(4.0f, 8.0f);
        Object@ building = CreateNode("Building", layer, basePosition, Vector3(0, 0, 0), Vector3(1, 1, 1));

        CreateMesh("Body", building, meshBox, Vector3(0, height * 0.5f, 0), Vector3(0, 0, 0),
            Vector3(width, height, depth), midColor, midCastShadows);

        // 屋根: のこぎり屋根 / 屋上設備
        if (rng.Chance(0.5f)) {
            int teeth = Max(2, int(width / 2.5f));
            float toothWidth = width / float(teeth);
            float angle = 0.5f;
            float slabLength = toothWidth / Cos(angle);
            float rise = toothWidth * Tan(angle);
            for (int i = 0; i < teeth; i++) {
                float cx = -width * 0.5f + toothWidth * (float(i) + 0.5f);
                CreateMesh("Roof", building, meshBox, Vector3(cx, height + rise * 0.5f, 0), Vector3(0, 0, angle),
                    Vector3(slabLength, 0.2f, depth), midDetailColor, midCastShadows);
                // 垂直面（明かり取り）
                CreateMesh("RoofWindow", building, meshBox, Vector3(cx + toothWidth * 0.5f - 0.05f, height + rise * 0.5f, 0), Vector3(0, 0, 0),
                    Vector3(0.1f, rise, depth * 0.98f), midWindowColor, false);
            }
        } else {
            int units = rng.RangeInt(0, 3);
            for (int i = 0; i < units; i++) {
                float s = rng.Range(0.8f, 1.6f);
                CreateMesh("RoofUnit", building, meshBox, Vector3(rng.Range(-width * 0.4f, width * 0.4f), height + s * 0.5f, rng.Range(-depth * 0.3f, depth * 0.3f)),
                    Vector3(0, 0, 0), Vector3(s * 1.5f, s, s), midDetailColor, midCastShadows);
            }
        }

        // 正面の窓帯
        int windowRows = rng.RangeInt(0, int(height / 3.5f));
        for (int i = 0; i < windowRows; i++) {
            float wy = height * (float(i) + 1.0f) / float(windowRows + 1);
            CreateMesh("Window", building, meshBox, Vector3(0, wy, -depth * 0.5f - 0.05f), Vector3(0, 0, 0),
                Vector3(width * rng.Range(0.5f, 0.85f), 0.35f, 0.1f), midWindowColor, false);
        }

        // 屋上の煙突
        if (rng.Chance(midChimneyChance)) {
            float radius = rng.Range(0.4f, 0.9f);
            CreateChimney(building, rng, Vector3(rng.Range(-width * 0.35f, width * 0.35f), height, rng.Range(-depth * 0.2f, depth * 0.2f)),
                radius, rng.Range(5.0f, 12.0f));
        }

        // 正面の歯車
        float maxGearRadius = Min(width, height) * 0.3f;
        if (maxGearRadius >= 0.8f && rng.Chance(midGearChance)) {
            float radius = rng.Range(0.8f, maxGearRadius);
            CreateGear(building, rng, Vector3(rng.Range(-width * 0.5f + radius, width * 0.5f - radius), rng.Range(radius + 0.3f, height - radius), -depth * 0.5f - 0.3f), radius);
        }
        return height;
    }

    /// @brief 煙突（本体＋紅白の帯＋先端の点滅灯）を生成する
    /// @return 煙突の先端の高さ（basePosition.y基準ではなくレイヤー内の高さ）
    float CreateChimney(Object@ parent, BGRandom@ rng, const Vector3 &in basePosition, float radius, float height) {
        Object@ chimney = CreateNode("Chimney", parent, basePosition, Vector3(0, 0, 0), Vector3(1, 1, 1));
        CreateMesh("Stack", chimney, meshCylinder, Vector3(0, height * 0.5f, 0), Vector3(0, 0, 0),
            Vector3(radius * 2.0f, height, radius * 2.0f), midColor, midCastShadows);
        for (int i = 0; i < 2; i++) {
            float by = height * (0.72f + 0.14f * float(i));
            CreateMesh("Stripe", chimney, meshCylinder, Vector3(0, by, 0), Vector3(0, 0, 0),
                Vector3(radius * 2.1f, height * 0.06f, radius * 2.1f), midStripeColor, false);
        }

        // 点滅灯: Blink.json（値域[0.2, 1]）をスケールの3成分へ適用する
        float lightSize = Max(radius * 0.6f, 0.3f);
        Object@ lightObject = CreateMesh("Light", chimney, meshSphere, Vector3(0, height + lightSize * 0.5f, 0), Vector3(0, 0, 0),
            Vector3(lightSize, lightSize, lightSize), midLightColor, false);
        KeyFrameAnimator@ animator;
        if (lightObject.AddComponent(@animator)) {
            AddLoopAnimation(animator, "Blink", kBlinkFile, rng.Range(0.7f, 1.3f));
            animator.SetValueScale("Blink", lightSize);
            animator.SetTimeOffset("Blink", rng.Range(0.0f, 1.2f));
            animator.AddBinding("Blink", "Transform", "scale_", 0);
            animator.AddBinding("Blink", "Transform", "scale_", 1);
            animator.AddBinding("Blink", "Transform", "scale_", 2);
        }
        return basePosition.y + height;
    }

    /// @brief タンク（円柱＋半球の蓋）を生成する
    /// @return タンクの高さ
    float CreateTank(Object@ layer, BGRandom@ rng, const Vector3 &in basePosition, float radius) {
        float height = rng.Range(2.0f, 5.0f);
        Object@ tank = CreateNode("Tank", layer, basePosition, Vector3(0, 0, 0), Vector3(1, 1, 1));
        CreateMesh("Body", tank, meshCylinder, Vector3(0, height * 0.5f, 0), Vector3(0, 0, 0),
            Vector3(radius * 2.0f, height, radius * 2.0f), midColor, midCastShadows);
        CreateMesh("Dome", tank, meshSphere, Vector3(0, height, 0), Vector3(0, 0, 0),
            Vector3(radius * 2.0f, radius * 0.9f, radius * 2.0f), midColor, midCastShadows);
        CreateMesh("Band", tank, meshCylinder, Vector3(0, height * 0.5f, 0), Vector3(0, 0, 0),
            Vector3(radius * 2.05f, 0.3f, radius * 2.05f), midStripeColor, false);
        return height + radius * 0.45f;
    }

    /// @brief カメラ側（-Z）を向いた歯車を生成し、Z軸回転のアニメーションを付ける
    void CreateGear(Object@ parent, BGRandom@ rng, const Vector3 &in position, float radius) {
        // 回転させるのはこのノード（rotate_.z）。円盤・歯は子として回転に追従する
        Object@ gear = CreateNode("Gear", parent, position, Vector3(0, 0, 0), Vector3(1, 1, 1));
        const float thickness = 0.4f;
        CreateMesh("Disc", gear, meshCylinder, Vector3(0, 0, 0), Vector3(GetPI() * 0.5f, 0, 0),
            Vector3(radius * 2.0f, thickness, radius * 2.0f), midDetailColor, midCastShadows);
        CreateMesh("Hub", gear, meshCylinder, Vector3(0, 0, -thickness * 0.5f), Vector3(GetPI() * 0.5f, 0, 0),
            Vector3(radius * 0.6f, thickness * 0.6f, radius * 0.6f), midStripeColor, false);

        int teeth = int(Clamp(radius * 6.0f, 6.0f, 20.0f));
        float toothLength = Max(radius * 0.3f, 0.25f);
        float toothWidth = 2.0f * GetPI() * radius / float(teeth) * 0.5f;
        for (int i = 0; i < teeth; i++) {
            float a = 2.0f * GetPI() * float(i) / float(teeth);
            float r = radius + toothLength * 0.3f;
            CreateMesh("Tooth", gear, meshBox, Vector3(Cos(a) * r, Sin(a) * r, 0), Vector3(0, 0, a),
                Vector3(toothLength, toothWidth, thickness), midDetailColor, midCastShadows);
        }

        KeyFrameAnimator@ animator;
        if (!gear.AddComponent(@animator)) return;
        float spinSpeed = rng.Range(midGearSpinSpeedMin, midGearSpinSpeedMax) * rng.Sign();
        AddLoopAnimation(animator, "Spin", kSpinFile, spinSpeed);
        animator.SetTimeOffset("Spin", rng.Range(0.0f, 1.0f));
        animator.AddBinding("Spin", "Transform", "rotate_", 2);
    }

    //--------------------------------------------------
    // 遠景: 工場シルエット
    //--------------------------------------------------

    void GenerateFar(Object@ root) {
        BGRandom@ rng = MakeLayerRandom(0x3000);
        Object@ layer = CreateNode("BG_FactoryFar", root, Vector3(0, farGroundY, farZ), Vector3(0, 0, 0), Vector3(1, 1, 1));

        float x = rangeStartX - farMarginX;
        const float endX = rangeEndX + farMarginX;
        while (x <= endX) {
            float width = rng.Range(farWidthMin, farWidthMax);
            float height = rng.Range(farHeightMin, farHeightMax);
            float z = rng.Range(-4.0f, 4.0f);
            CreateMesh("Tower", layer, meshBox, Vector3(x + width * 0.5f, height * 0.5f, z), Vector3(0, 0, 0),
                Vector3(width, height, rng.Range(6.0f, 12.0f)), farColor, false);

            // 段差を付けて単調な箱の並びにならないようにする
            if (rng.Chance(0.5f)) {
                float stepWidth = width * rng.Range(0.3f, 0.6f);
                float stepHeight = height * rng.Range(0.15f, 0.35f);
                CreateMesh("Step", layer, meshBox, Vector3(x + rng.Range(stepWidth * 0.5f, width - stepWidth * 0.5f), height + stepHeight * 0.5f, z),
                    Vector3(0, 0, 0), Vector3(stepWidth, stepHeight, 6.0f), farColor, false);
            }
            if (rng.Chance(farChimneyChance)) {
                float radius = rng.Range(0.8f, 1.8f);
                float chimneyHeight = height + rng.Range(8.0f, 20.0f);
                CreateMesh("Chimney", layer, meshCylinder, Vector3(x + rng.Range(radius, width - radius), chimneyHeight * 0.5f, z - 2.0f),
                    Vector3(0, 0, 0), Vector3(radius * 2.0f, chimneyHeight, radius * 2.0f), farColor, false);
            }
            x += width + rng.Range(farGapMin, farGapMax);
        }
    }

    //==================================================
    // オブジェクト生成ヘルパー
    //==================================================

    /// @brief Transformのみの空オブジェクトを作り、親子付けとローカル姿勢を設定する
    Object@ CreateNode(const string &in name, Object@ parent, const Vector3 &in translate, const Vector3 &in rotate, const Vector3 &in scale) {
        Object@ obj = scene.CreateObject(name);
        Transform@ transform = obj.GetTransform();
        if (parent !is null) {
            transform.SetParentObject(parent);
        }
        transform.SetTranslate(translate);
        transform.SetRotate(rotate);
        transform.SetScale(scale);
        createdObjectCount++;
        return obj;
    }

    /// @brief プリミティブメッシュを描画するオブジェクトを作る
    Object@ CreateMesh(const string &in name, Object@ parent, uint meshHandle, const Vector3 &in translate, const Vector3 &in rotate,
        const Vector3 &in scale, const Vector4 &in color, bool castShadows) {
        Object@ obj = CreateNode(name, parent, translate, rotate, scale);

        MeshFilter@ filter;
        if (obj.AddComponent(@filter)) {
            filter.SetMeshHandle(meshHandle);
        }
        MeshRenderer@ renderer;
        if (obj.AddComponent(@renderer)) {
            renderer.SetMaterialName(materialName);
            renderer.SetInstanceColor(color);
            renderer.SetAllowInstancing(true);
            renderer.SetCastShadows(castShadows);
            if (renderTarget !is null) {
                renderer.SetTargetObject(renderTarget);
            }
        }
        return obj;
    }

    /// @brief ループ・自動再生のアニメーションを追加する
    void AddLoopAnimation(KeyFrameAnimator@ animator, const string &in name, const string &in fileName, float playbackSpeed) {
        animator.AddAnimation(name, AnimationPath(fileName));
        animator.SetLoop(name, true);
        animator.SetPlayOnStart(name, true);
        animator.SetPlaybackSpeed(name, playbackSpeed);
    }

    //==================================================
    // キーフレームjsonの書き出し
    //==================================================

    string AnimationPath(const string &in fileName) {
        string folder = animationFolder;
        if (folder.length() > 0) {
            string last = folder.substr(folder.length() - 1, 1);
            if (last != "/" && last != "\\") folder += "/";
        }
        return folder + fileName;
    }

    bool WriteAnimationFiles() {
        // 浮遊: 2秒周期で -1 → 1 → -1
        array<float> floatTimes = { 0.0f, 1.0f, 2.0f };
        array<float> floatValues = { -1.0f, 1.0f, -1.0f };
        array<string> floatEases = { "EaseInOutSine", "EaseInOutSine", "Linear" };

        // 回転: 1秒で 0 → 2π（ループ時に継ぎ目が出ないよう等速）
        array<float> spinTimes = { 0.0f, 1.0f };
        array<float> spinValues = { 0.0f, 2.0f * GetPI() };
        array<string> spinEases = { "Linear", "Linear" };

        // 点滅: 1.2秒周期で 0.2 → 1（素早く点灯）→ 保持 → 0.2（消灯）→ 保持
        array<float> blinkTimes = { 0.0f, 0.1f, 0.4f, 0.7f, 1.2f };
        array<float> blinkValues = { 0.2f, 1.0f, 1.0f, 0.2f, 0.2f };
        array<string> blinkEases = { "EaseOutQuad", "Linear", "EaseInQuad", "Linear", "Linear" };

        return WriteCurve(AnimationPath(kFloatFile), floatTimes, floatValues, floatEases)
            && WriteCurve(AnimationPath(kSpinFile), spinTimes, spinValues, spinEases)
            && WriteCurve(AnimationPath(kBlinkFile), blinkTimes, blinkValues, blinkEases);
    }

    /// @brief KeyframeAnimation::LoadFromJson 形式（Keyframe Animation Editor と同じ形式）で保存する
    bool WriteCurve(const string &in path, const array<float> &in times, const array<float> &in values, const array<string> &in eases) {
        Json@ json = Json();
        Json@ keysJson = Json();
        for (uint i = 0; i < times.length(); i++) {
            Json@ keyJson = Json();
            keyJson.SetFloat("time", times[i]);
            keyJson.SetFloat("value", values[i]);
            keyJson.SetString("ease", eases[i]);
            keysJson.PushJson(keyJson);
        }
        json.SetJson("keyframes", keysJson);
        return SaveJsonFile(path, json);
    }

    //==================================================
    // 設定の保存・読み込み
    //==================================================

    void SaveSettings() {
        Json@ json = Json();
        json.SetString("animationFolder", animationFolder);
        json.SetString("rootName", rootName);
        json.SetString("renderTargetName", renderTargetName);
        json.SetString("materialName", materialName);
        json.SetInt("seed", seed);
        json.SetFloat("rangeStartX", rangeStartX);
        json.SetFloat("rangeEndX", rangeEndX);

        Json@ fg = Json();
        fg.SetBool("enabled", fgEnabled);
        fg.SetFloat("marginX", fgMarginX);
        fg.SetFloat("zMin", fgZMin);
        fg.SetFloat("zMax", fgZMax);
        fg.SetFloat("yMin", fgYMin);
        fg.SetFloat("yMax", fgYMax);
        fg.SetBool("avoidBand", fgAvoidBand);
        fg.SetFloat("avoidYMin", fgAvoidYMin);
        fg.SetFloat("avoidYMax", fgAvoidYMax);
        fg.SetFloat("spacingMin", fgSpacingMin);
        fg.SetFloat("spacingMax", fgSpacingMax);
        fg.SetFloat("sizeMin", fgSizeMin);
        fg.SetFloat("sizeMax", fgSizeMax);
        fg.SetFloat("floatAmplitudeMin", fgFloatAmplitudeMin);
        fg.SetFloat("floatAmplitudeMax", fgFloatAmplitudeMax);
        fg.SetFloat("floatPeriodMin", fgFloatPeriodMin);
        fg.SetFloat("floatPeriodMax", fgFloatPeriodMax);
        fg.SetFloat("spinSpeedMin", fgSpinSpeedMin);
        fg.SetFloat("spinSpeedMax", fgSpinSpeedMax);
        fg.SetFloat("accentRatio", fgAccentRatio);
        fg.SetBool("castShadows", fgCastShadows);
        fg.SetVector4("color", fgColor);
        fg.SetVector4("accentColor", fgAccentColor);
        json.SetJson("foreground", fg);

        Json@ mid = Json();
        mid.SetBool("enabled", midEnabled);
        mid.SetFloat("marginX", midMarginX);
        mid.SetFloat("z", midZ);
        mid.SetFloat("groundY", midGroundY);
        mid.SetBool("groundStrip", midGroundStrip);
        mid.SetFloat("gapMin", midGapMin);
        mid.SetFloat("gapMax", midGapMax);
        mid.SetFloat("buildingWidthMin", midBuildingWidthMin);
        mid.SetFloat("buildingWidthMax", midBuildingWidthMax);
        mid.SetFloat("buildingHeightMin", midBuildingHeightMin);
        mid.SetFloat("buildingHeightMax", midBuildingHeightMax);
        mid.SetFloat("chimneyChance", midChimneyChance);
        mid.SetFloat("tankChance", midTankChance);
        mid.SetFloat("gearChance", midGearChance);
        mid.SetFloat("pipeChance", midPipeChance);
        mid.SetFloat("gearSpinSpeedMin", midGearSpinSpeedMin);
        mid.SetFloat("gearSpinSpeedMax", midGearSpinSpeedMax);
        mid.SetBool("castShadows", midCastShadows);
        mid.SetVector4("color", midColor);
        mid.SetVector4("detailColor", midDetailColor);
        mid.SetVector4("stripeColor", midStripeColor);
        mid.SetVector4("windowColor", midWindowColor);
        mid.SetVector4("lightColor", midLightColor);
        json.SetJson("middle", mid);

        Json@ far = Json();
        far.SetBool("enabled", farEnabled);
        far.SetFloat("marginX", farMarginX);
        far.SetFloat("z", farZ);
        far.SetFloat("groundY", farGroundY);
        far.SetFloat("gapMin", farGapMin);
        far.SetFloat("gapMax", farGapMax);
        far.SetFloat("widthMin", farWidthMin);
        far.SetFloat("widthMax", farWidthMax);
        far.SetFloat("heightMin", farHeightMin);
        far.SetFloat("heightMax", farHeightMax);
        far.SetFloat("chimneyChance", farChimneyChance);
        far.SetVector4("color", farColor);
        json.SetJson("far", far);

        if (SaveJsonFile(settingsPath, json)) {
            statusMessage = "設定を保存しました: " + settingsPath;
        } else {
            statusMessage = "設定の保存に失敗しました: " + settingsPath;
        }
    }

    /// @param reportMissing ファイルが無い場合にステータスへ表示するか（ウィンドウを開いたときの自動読み込みでは表示しない）
    void LoadSettings(bool reportMissing) {
        Json@ json = LoadJsonFile(settingsPath);
        if (json is null) {
            if (reportMissing) statusMessage = "設定の読み込みに失敗しました: " + settingsPath;
            return;
        }
        animationFolder = json.GetString("animationFolder", animationFolder);
        rootName = json.GetString("rootName", rootName);
        renderTargetName = json.GetString("renderTargetName", renderTargetName);
        materialName = json.GetString("materialName", materialName);
        seed = int(json.GetInt("seed", seed));
        rangeStartX = float(json.GetFloat("rangeStartX", rangeStartX));
        rangeEndX = float(json.GetFloat("rangeEndX", rangeEndX));

        Json@ fg = json.GetJson("foreground");
        if (fg !is null) {
            fgEnabled = fg.GetBool("enabled", fgEnabled);
            fgMarginX = float(fg.GetFloat("marginX", fgMarginX));
            fgZMin = float(fg.GetFloat("zMin", fgZMin));
            fgZMax = float(fg.GetFloat("zMax", fgZMax));
            fgYMin = float(fg.GetFloat("yMin", fgYMin));
            fgYMax = float(fg.GetFloat("yMax", fgYMax));
            fgAvoidBand = fg.GetBool("avoidBand", fgAvoidBand);
            fgAvoidYMin = float(fg.GetFloat("avoidYMin", fgAvoidYMin));
            fgAvoidYMax = float(fg.GetFloat("avoidYMax", fgAvoidYMax));
            fgSpacingMin = float(fg.GetFloat("spacingMin", fgSpacingMin));
            fgSpacingMax = float(fg.GetFloat("spacingMax", fgSpacingMax));
            fgSizeMin = float(fg.GetFloat("sizeMin", fgSizeMin));
            fgSizeMax = float(fg.GetFloat("sizeMax", fgSizeMax));
            fgFloatAmplitudeMin = float(fg.GetFloat("floatAmplitudeMin", fgFloatAmplitudeMin));
            fgFloatAmplitudeMax = float(fg.GetFloat("floatAmplitudeMax", fgFloatAmplitudeMax));
            fgFloatPeriodMin = float(fg.GetFloat("floatPeriodMin", fgFloatPeriodMin));
            fgFloatPeriodMax = float(fg.GetFloat("floatPeriodMax", fgFloatPeriodMax));
            fgSpinSpeedMin = float(fg.GetFloat("spinSpeedMin", fgSpinSpeedMin));
            fgSpinSpeedMax = float(fg.GetFloat("spinSpeedMax", fgSpinSpeedMax));
            fgAccentRatio = float(fg.GetFloat("accentRatio", fgAccentRatio));
            fgCastShadows = fg.GetBool("castShadows", fgCastShadows);
            fgColor = fg.GetVector4("color", fgColor);
            fgAccentColor = fg.GetVector4("accentColor", fgAccentColor);
        }

        Json@ mid = json.GetJson("middle");
        if (mid !is null) {
            midEnabled = mid.GetBool("enabled", midEnabled);
            midMarginX = float(mid.GetFloat("marginX", midMarginX));
            midZ = float(mid.GetFloat("z", midZ));
            midGroundY = float(mid.GetFloat("groundY", midGroundY));
            midGroundStrip = mid.GetBool("groundStrip", midGroundStrip);
            midGapMin = float(mid.GetFloat("gapMin", midGapMin));
            midGapMax = float(mid.GetFloat("gapMax", midGapMax));
            midBuildingWidthMin = float(mid.GetFloat("buildingWidthMin", midBuildingWidthMin));
            midBuildingWidthMax = float(mid.GetFloat("buildingWidthMax", midBuildingWidthMax));
            midBuildingHeightMin = float(mid.GetFloat("buildingHeightMin", midBuildingHeightMin));
            midBuildingHeightMax = float(mid.GetFloat("buildingHeightMax", midBuildingHeightMax));
            midChimneyChance = float(mid.GetFloat("chimneyChance", midChimneyChance));
            midTankChance = float(mid.GetFloat("tankChance", midTankChance));
            midGearChance = float(mid.GetFloat("gearChance", midGearChance));
            midPipeChance = float(mid.GetFloat("pipeChance", midPipeChance));
            midGearSpinSpeedMin = float(mid.GetFloat("gearSpinSpeedMin", midGearSpinSpeedMin));
            midGearSpinSpeedMax = float(mid.GetFloat("gearSpinSpeedMax", midGearSpinSpeedMax));
            midCastShadows = mid.GetBool("castShadows", midCastShadows);
            midColor = mid.GetVector4("color", midColor);
            midDetailColor = mid.GetVector4("detailColor", midDetailColor);
            midStripeColor = mid.GetVector4("stripeColor", midStripeColor);
            midWindowColor = mid.GetVector4("windowColor", midWindowColor);
            midLightColor = mid.GetVector4("lightColor", midLightColor);
        }

        Json@ far = json.GetJson("far");
        if (far !is null) {
            farEnabled = far.GetBool("enabled", farEnabled);
            farMarginX = float(far.GetFloat("marginX", farMarginX));
            farZ = float(far.GetFloat("z", farZ));
            farGroundY = float(far.GetFloat("groundY", farGroundY));
            farGapMin = float(far.GetFloat("gapMin", farGapMin));
            farGapMax = float(far.GetFloat("gapMax", farGapMax));
            farWidthMin = float(far.GetFloat("widthMin", farWidthMin));
            farWidthMax = float(far.GetFloat("widthMax", farWidthMax));
            farHeightMin = float(far.GetFloat("heightMin", farHeightMin));
            farHeightMax = float(far.GetFloat("heightMax", farHeightMax));
            farChimneyChance = float(far.GetFloat("chimneyChance", farChimneyChance));
            farColor = far.GetVector4("color", farColor);
        }
        statusMessage = "設定を読み込みました: " + settingsPath;
    }
}
