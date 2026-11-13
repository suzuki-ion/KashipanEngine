#include "TownTitleBackdrop.as"
#include "TownTransition.as"
#include "TownState.as"
#include "TownSettings.as"

class TitleController : ScriptComponentBehavior {
    TownTransition@ transition=TownTransition();
    TownTitleBackdrop@ backdrop=TownTitleBackdrop();
    TownUI@ ui = TownUI();
    TownSettingsMenu@ settings = TownSettingsMenu();
    TextRenderer@ title; TextRenderer@ subtitle; TextRenderer@ notice;
    bool canContinue = false;
    bool leaving = false;
    float overwriteConfirm = 0;
    void Start() {
        transition.Initialize(GetScene());
        SetCurrentLanguage(GetSettingString("language", "ja-JP"));
        TownApplySettings(GetScene());backdrop.Initialize(GetScene());
        ui.Initialize(GetScene());
        const float menuX = 88;
        @title = ui.Text("LIGHT TOWN", menuX, 100, 58);
        @subtitle = ui.Text("", menuX, 188, 22);
        ui.Button("", menuX, 290); ui.Button("", menuX, 363);
        ui.Button("", menuX, 436); ui.Button("", menuX, 509);
        for (uint i = 0; i < ui.buttonTexts.length(); ++i) ui.buttonTexts[i].SetFontSize(28);
        @notice = ui.Text("", menuX, 610, 20);
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
        if(transition.Update(GetDeltaTime()))return;if(transition.failed){leaving=false;transition.failed=false;}
        backdrop.Update(GetDeltaTime());
        if (leaving) return;
        if (settings.visible) { settings.Update();backdrop.ConfigureView(); if (settings.languageChanged) Refresh();if(!settings.visible&&settings.needsSceneRefresh){backdrop.world.Rebuild();settings.needsSceneRefresh=false;}return; }
        overwriteConfirm -= GetDeltaTime();
        int click = ui.Clicked();
        if (!canContinue) ui.buttonTexts[1].SetColor(Vector4(0.35f, 0.4f, 0.45f, 1));
        if (click == 2) { settings.Open(); return; }
        if (click == 3) { RequestExitGameLoop(); return; }
        if (click == 0) {
            if (canContinue && overwriteConfirm <= 0) {
                overwriteConfirm = 4;
                notice.SetText(TownTr("もう一度「はじめから」を選ぶと、\nセーブデータを上書きします。", "Select New Game again\nto overwrite the save.")); return;
            }
            TownState@ state = TownState();
            if (!state.Initialize(GetScene())) { notice.SetText(TownTr("ゲーム設定を読み込めません。", "Unable to load game rules.")); return; }
            state.NewGame();
            if (!state.Save()) { notice.SetText(TownTr("セーブできません。\n保存先を確認してください。", "Unable to save.\nCheck the save location.")); return; }
            Go();
        }
        if (click == 1 && canContinue) {
            Json@ saved = LoadJsonFile("SaveData/LightTown.json");
            if (saved is null || saved.GetInt("version") != 1 || saved.GetJson("buildings") is null) {
                canContinue = false; ui.buttons[1].SetActive(false); Refresh(); return;
            }
            GetScene().SetGlobalVariable("LightTown.State", saved.ToString());
            GetScene().SetGlobalVariable("LightTown.LastRealTime", GetMonotonicSeconds());
            GetScene().SetGlobalVariable("LightTown.TransformationUnlocked", saved.GetBool("transformationUnlocked"));
            Go();
        }
    }
    void Go() {
        GetScene().SetNextSceneName("CityScene"); leaving = transition.Begin("CityScene");
        if (!leaving) notice.SetText(TownTr("街へ移動できませんでした。", "Unable to open the town."));
    }
}
