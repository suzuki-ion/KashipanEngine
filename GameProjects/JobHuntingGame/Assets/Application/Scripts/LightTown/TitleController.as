#include "TownState.as"
#include "TownSettings.as"

class TitleController : ScriptComponentBehavior {
    TownUI@ ui = TownUI();
    TownSettingsMenu@ settings = TownSettingsMenu();
    TextRenderer@ title; TextRenderer@ subtitle; TextRenderer@ notice;
    bool canContinue = false;
    bool leaving = false;
    float overwriteConfirm = 0;
    void Start() {
        SetCurrentLanguage(GetSettingString("language", "ja-JP"));
        TownApplySettings(GetScene());
        ui.Initialize(GetScene());
        @title = ui.Text("LIGHT TOWN", 480, 100, 58);
        @subtitle = ui.Text("", 485, 188, 22);
        ui.Button("", 520, 290); ui.Button("", 520, 363);
        ui.Button("", 520, 436); ui.Button("", 520, 509);
        for (uint i = 0; i < ui.buttonTexts.length(); ++i) ui.buttonTexts[i].SetFontSize(28);
        @notice = ui.Text("", 370, 610, 20);
        Json@ saved = LoadJsonFile("SaveData/LightTown.json");
        canContinue = saved !is null && saved.GetInt("version") == 1 && saved.GetJson("buildings") !is null && saved.GetJson("buildings").IsArray();
        ui.buttons[1].SetActive(canContinue);
        settings.Initialize(GetScene()); Refresh();
    }
    void Refresh() {
        subtitle.SetText(TownTr("光を集め、小さな街を育てよう", "Gather light. Build a town."));
        ui.buttonTexts[0].SetText(TownTr("はじめから", "New Game"));
        ui.buttonTexts[1].SetText(TownTr("つづきから", "Continue") + (canContinue ? "" : TownTr("（セーブデータなし）", " (No save data)")));
        ui.buttonTexts[2].SetText(TownTr("設定", "Settings"));
        ui.buttonTexts[3].SetText(TownTr("終了", "Quit"));
    }
    void Update() {
        if (leaving) return;
        if (settings.visible) { settings.Update(); if (settings.languageChanged) Refresh(); return; }
        overwriteConfirm -= GetDeltaTime();
        int click = ui.Clicked();
        if (!canContinue) ui.buttonTexts[1].SetColor(Vector4(0.35f, 0.4f, 0.45f, 1));
        if (click == 2) { settings.Open(); return; }
        if (click == 3) { RequestExitGameLoop(); return; }
        if (click == 0) {
            if (canContinue && overwriteConfirm <= 0) {
                overwriteConfirm = 4;
                notice.SetText(TownTr("もう一度「はじめから」でセーブを上書きします。", "Select New Game again to overwrite the save.")); return;
            }
            TownState@ state = TownState();
            if (!state.Initialize(GetScene())) { notice.SetText(TownTr("ゲーム設定を読み込めません。", "Unable to load game rules.")); return; }
            state.NewGame();
            if (!state.Save()) { notice.SetText(TownTr("セーブできません。保存先を確認してください。", "Unable to save. Check the save location.")); return; }
            Go();
        }
        if (click == 1 && canContinue) {
            Json@ saved = LoadJsonFile("SaveData/LightTown.json");
            if (saved is null || saved.GetInt("version") != 1 || saved.GetJson("buildings") is null) {
                canContinue = false; ui.buttons[1].SetActive(false); Refresh(); return;
            }
            GetScene().SetGlobalVariable("LightTown.State", saved.ToString());
            GetScene().SetGlobalVariable("LightTown.TransformationUnlocked", saved.GetBool("transformationUnlocked"));
            Go();
        }
    }
    void Go() {
        GetScene().SetNextSceneName("CityScene"); leaving = GetScene().ChangeToNextScene();
        if (!leaving) notice.SetText(TownTr("街へ移動できませんでした。", "Unable to open the town."));
    }
}
