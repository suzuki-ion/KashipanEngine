// WaveFunctionCollapse用のタイル定義エディターツール。
//
// メニューバーの Tools > WFC Tile Editor からウィンドウを開き、
// タイルの追加・削除・リネーム・接続編集を行って、WaveFunctionCollapse::LoadFromJson()
// がそのまま読み込める形式のjsonとして任意のパスへ保存できる。
//
// 使い方:
//   1. "Add Tile" でタイルを追加（グリッド状に並ぶ）
//   2. タイルをクリックして選択し、下の編集パネルで名前の変更や方向ごとの接続先タイルの追加/削除を行う
//      （接続は片方向の宣言。双方向にしたい場合は逆側のタイルにも追加すること）
//   3. Save Path を指定して "Save" で保存、"Load" で読み込み
//
// スクリプト側での利用例:
//   WaveFunctionCollapse@ wfc = WaveFunctionCollapse();
//   Json@ json = LoadJsonFile("Assets/Application/WfcTiles.json");
//   if (json !is null) wfc.LoadFromJson(json);

/// @brief タイル1種類分の定義（名前と6方向の接続先タイル名リスト）
class WfcTileDef {
    string name;
    array<string> up;
    array<string> down;
    array<string> left;
    array<string> right;
    array<string> front;
    array<string> back;

    WfcTileDef(const string &in tileName) {
        name = tileName;
    }

    array<string>@ GetConnections(int dir) {
        if (dir == 0) return up;
        if (dir == 1) return down;
        if (dir == 2) return left;
        if (dir == 3) return right;
        if (dir == 4) return front;
        return back;
    }

    uint GetTotalConnectionCount() {
        return up.length() + down.length() + left.length() + right.length() + front.length() + back.length();
    }
}

[EditorWindow("WFC Tile Editor")]
[MenuItem("MenuBar/Tools/WFC Tile Editor", "open_wfc_tile_editor")]
class WfcTileEditor : EditorTool {
    array<WfcTileDef@> tiles;
    int selectedIndex = -1;

    string savePath = "Assets/Application/WfcTiles.json";
    string statusMessage = "";

    // 選択中タイルのリネーム編集用バッファ（Apply Renameで確定する）
    string renameBuffer = "";
    int renameBufferForIndex = -1;

    // WaveFunctionCollapse::LoadFromJsonが必須とするグリッド設定（保存時にjsonへ含める）
    int seed = 0;
    int gridWidth = 10;
    int gridHeight = 1;
    int gridDepth = 10;

    array<string> dirNames;
    // 方向ごとの「接続追加」コンボの選択インデックス
    array<int> pendingAdd;

    WfcTileEditor() {
        dirNames.insertLast("Up (+Y)");
        dirNames.insertLast("Down (-Y)");
        dirNames.insertLast("Left (-X)");
        dirNames.insertLast("Right (+X)");
        dirNames.insertLast("Front (+Z)");
        dirNames.insertLast("Back (-Z)");
        pendingAdd.resize(6);
    }

    void InitializeOnLoad() {
        Log("WfcTileEditor: 読み込まれました (Tools > WFC Tile Editor)");
    }

    void OnItemSelected(const string &in tag) {
        if (tag == "open_wfc_tile_editor") {
            OpenEditorWindow("WFC Tile Editor");
        }
    }

    void OnWindowEnable(const string &in windowName) {}
    void OnWindowDisable(const string &in windowName) {}

    void Update() {
        if (!IsEditorWindowOpen("WFC Tile Editor")) return;

        ShowSaveLoadSection();
        ImGui::Separator();
        ShowGridSettingsSection();
        ImGui::Separator();
        ShowTileGridSection();
        ImGui::Separator();
        ShowTileEditSection();
    }

    //==================================================
    // 保存・読み込み
    //==================================================

    void ShowSaveLoadSection() {
        ImGui::InputText("Save Path", savePath);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("保存/読み込みするjsonのパス（実行ディレクトリ基準の相対パスまたは絶対パス）");
        }
        if (ImGui::Button("Save")) {
            SaveToFile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            LoadFromFile();
        }
        if (statusMessage != "") {
            ImGui::TextDisabled(statusMessage);
        }
    }

    void SaveToFile() {
        Json@ json = Json();
        json.SetInt("seed", seed);
        json.SetInt("gridWidth", gridWidth);
        json.SetInt("gridHeight", gridHeight);
        json.SetInt("gridDepth", gridDepth);

        Json@ tilesJson = Json();
        for (uint i = 0; i < tiles.length(); i++) {
            WfcTileDef@ tile = tiles[i];
            Json@ tileJson = Json();
            tileJson.SetString("name", tile.name);
            Json@ connectionsJson = Json();
            connectionsJson.Set("up", tile.up);
            connectionsJson.Set("down", tile.down);
            connectionsJson.Set("left", tile.left);
            connectionsJson.Set("right", tile.right);
            connectionsJson.Set("front", tile.front);
            connectionsJson.Set("back", tile.back);
            tileJson.SetJson("connections", connectionsJson);
            tilesJson.PushJson(tileJson);
        }
        json.SetJson("tiles", tilesJson);

        if (SaveJsonFile(savePath, json)) {
            statusMessage = "保存しました: " + savePath;
        } else {
            statusMessage = "保存に失敗しました: " + savePath;
        }
    }

    void LoadFromFile() {
        Json@ json = LoadJsonFile(savePath);
        if (json is null) {
            statusMessage = "読み込みに失敗しました: " + savePath;
            return;
        }

        seed = int(json.GetInt("seed", 0));
        gridWidth = int(json.GetInt("gridWidth", 10));
        gridHeight = int(json.GetInt("gridHeight", 1));
        gridDepth = int(json.GetInt("gridDepth", 10));

        tiles.resize(0);
        selectedIndex = -1;
        renameBufferForIndex = -1;

        Json@ tilesJson = json.GetJson("tiles");
        if (tilesJson !is null) {
            for (uint i = 0; i < tilesJson.Size(); i++) {
                Json@ tileJson = tilesJson.At(i);
                if (tileJson is null) continue;
                WfcTileDef@ tile = WfcTileDef(tileJson.GetString("name", ""));
                Json@ connectionsJson = tileJson.GetJson("connections");
                if (connectionsJson !is null) {
                    connectionsJson.Get("up", tile.up);
                    connectionsJson.Get("down", tile.down);
                    connectionsJson.Get("left", tile.left);
                    connectionsJson.Get("right", tile.right);
                    connectionsJson.Get("front", tile.front);
                    connectionsJson.Get("back", tile.back);
                }
                tiles.insertLast(tile);
            }
        }
        statusMessage = "読み込みました: " + savePath + " (タイル数: " + tiles.length() + ")";
    }

    //==================================================
    // WFCグリッド設定
    //==================================================

    void ShowGridSettingsSection() {
        ImGui::Text("WFC Settings");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("WaveFunctionCollapse::LoadFromJsonの読み込みに必要なシード値とグリッドサイズ。\n読み込み後にスクリプト側でSetSeed/SetGridSizeで上書きもできる");
        }
        ImGui::SetNextItemWidth(120);
        ImGui::DragInt("Seed", seed, 1, 0, 2147483647);
        ImGui::SetNextItemWidth(80);
        ImGui::DragInt("Width", gridWidth, 1, 1, 100000);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::DragInt("Height", gridHeight, 1, 1, 100000);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::DragInt("Depth", gridDepth, 1, 1, 100000);
    }

    //==================================================
    // タイル一覧（縦横グリッド表示）
    //==================================================

    void ShowTileGridSection() {
        ImGui::Text("Tiles (" + tiles.length() + ")");
        ImGui::SameLine();
        if (ImGui::SmallButton("Add Tile")) {
            AddTile();
        }

        // ウィンドウ幅から1行あたりの列数を計算して、タイルを縦横のグリッド状に並べる
        float buttonSize = 72.0f;
        float spacing = 6.0f;
        int columns = int(ImGui::GetContentRegionAvail().x / (buttonSize + spacing));
        if (columns < 1) columns = 1;

        for (uint i = 0; i < tiles.length(); i++) {
            if (int(i) % columns != 0) ImGui::SameLine();
            ImGui::PushID(int(i));

            // 選択中のタイルはラベルに印を付ける
            string label = (int(i) == selectedIndex ? "*" : "") + tiles[i].name + "\n" + tiles[i].GetTotalConnectionCount() + " conn";
            if (ImGui::Button(label, buttonSize, buttonSize)) {
                selectedIndex = int(i);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(tiles[i].name + "\nクリックで選択して下のパネルで編集");
            }
            ImGui::PopID();
        }
    }

    void AddTile() {
        // "Tile0", "Tile1", ... の形式で未使用の名前を割り当てる
        int number = 0;
        string newName = "Tile0";
        while (FindTileIndex(newName) >= 0) {
            number++;
            newName = "Tile" + number;
        }
        tiles.insertLast(WfcTileDef(newName));
        selectedIndex = int(tiles.length()) - 1;
    }

    int FindTileIndex(const string &in name) {
        for (uint i = 0; i < tiles.length(); i++) {
            if (tiles[i].name == name) return int(i);
        }
        return -1;
    }

    void DeleteSelectedTile() {
        if (selectedIndex < 0 || selectedIndex >= int(tiles.length())) return;
        string removedName = tiles[selectedIndex].name;

        // 他のタイルの接続リストから削除対象の名前を取り除く
        RemoveNameFromAllConnections(removedName);

        tiles.removeAt(selectedIndex);
        selectedIndex = -1;
        renameBufferForIndex = -1;
    }

    void RemoveNameFromAllConnections(const string &in name) {
        for (uint i = 0; i < tiles.length(); i++) {
            for (int dir = 0; dir < 6; dir++) {
                array<string>@ conns = tiles[i].GetConnections(dir);
                int found = conns.find(name);
                while (found >= 0) {
                    conns.removeAt(found);
                    found = conns.find(name);
                }
            }
        }
    }

    void RenameTile(int tileIndex, const string &in newName) {
        if (tileIndex < 0 || tileIndex >= int(tiles.length())) return;
        if (newName == "") {
            statusMessage = "リネーム失敗: 名前が空です";
            return;
        }
        if (FindTileIndex(newName) >= 0) {
            statusMessage = "リネーム失敗: \"" + newName + "\" は既に使われています";
            return;
        }

        string oldName = tiles[tileIndex].name;
        tiles[tileIndex].name = newName;

        // 他のタイルの接続リストに含まれる旧名を新名へ置き換える
        for (uint i = 0; i < tiles.length(); i++) {
            for (int dir = 0; dir < 6; dir++) {
                array<string>@ conns = tiles[i].GetConnections(dir);
                for (uint j = 0; j < conns.length(); j++) {
                    if (conns[j] == oldName) conns[j] = newName;
                }
            }
        }
        statusMessage = "リネームしました: " + oldName + " -> " + newName;
    }

    //==================================================
    // 選択中タイルの編集パネル
    //==================================================

    void ShowTileEditSection() {
        if (selectedIndex < 0 || selectedIndex >= int(tiles.length())) {
            ImGui::TextDisabled("タイルをクリックすると、ここで編集できます");
            return;
        }

        WfcTileDef@ tile = tiles[selectedIndex];
        ImGui::Text("Edit Tile: " + tile.name);
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete Tile")) {
            DeleteSelectedTile();
            return;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("このタイルを削除する（他タイルの接続リストからも取り除かれる）");
        }

        // リネーム（Apply Renameで確定。他タイルの接続リスト内の参照も追従する）
        if (renameBufferForIndex != selectedIndex) {
            renameBuffer = tile.name;
            renameBufferForIndex = selectedIndex;
        }
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##rename", renameBuffer);
        ImGui::SameLine();
        if (ImGui::SmallButton("Apply Rename")) {
            RenameTile(selectedIndex, renameBuffer);
            renameBufferForIndex = -1;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("入力した名前へ変更する（他タイルからの接続参照も自動で追従する）");
        }

        // 接続先の選択肢（全タイルの名前）を作る
        array<string> tileLabels;
        for (uint i = 0; i < tiles.length(); i++) {
            tileLabels.insertLast(tiles[i].name);
        }

        for (int dir = 0; dir < 6; dir++) {
            ImGui::PushID(dir);
            ImGui::Text(dirNames[dir]);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("この方向に隣接して置けるタイルの一覧。\n空の場合はこの方向に何も置けない(グリッド境界のみ)という意味になる");
            }

            // 既存の接続をボタンとして並べ、クリックで削除する
            array<string>@ conns = tile.GetConnections(dir);
            int removeIndex = -1;
            for (uint j = 0; j < conns.length(); j++) {
                ImGui::SameLine();
                ImGui::PushID(int(j));
                if (ImGui::SmallButton(conns[j] + " x")) {
                    removeIndex = int(j);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("クリックでこの接続を削除");
                }
                ImGui::PopID();
            }
            if (removeIndex >= 0) {
                conns.removeAt(removeIndex);
            }

            // 接続の追加（コンボで対象タイルを選んで+ボタン）
            if (tileLabels.length() > 0) {
                ImGui::SameLine();
                if (pendingAdd[dir] >= int(tileLabels.length())) pendingAdd[dir] = 0;
                ImGui::SetNextItemWidth(140);
                ImGui::Combo("##addTarget", pendingAdd[dir], tileLabels);
                ImGui::SameLine();
                if (ImGui::SmallButton("+")) {
                    string addName = tiles[pendingAdd[dir]].name;
                    if (conns.find(addName) < 0) {
                        conns.insertLast(addName);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("選択したタイルをこの方向の接続先として追加");
                }
            }
            ImGui::PopID();
        }
    }
}
