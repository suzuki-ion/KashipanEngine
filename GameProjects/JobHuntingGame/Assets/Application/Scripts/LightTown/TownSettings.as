#include "TownLanguage.as"
#include "TownPresentation.as"

void TownApplySettings(Scene@ scene) {
    SetCategoryVolume("SE", GetSettingFloat("town.seVolume", 0.8f));
    SetCategoryVolume("BGM", GetSettingFloat("town.bgmVolume", 0.6f));
    array<string> names = {"TownAmbient", "TownLight", "SceneLight", "Light Object", "Directional Light",
        "PrefabEnemyPart1", "PrefabCheckpointPart2", "PrefabTeleportPart2", "PrefabTeleportPart4",
        "HistoricalStreetSpot", "HistoricalStreetSphere", "HistoricalStreetFill", "HistoricalFactoryShadow", "HistoricalFactoryFill", "TownFacilityLight", "TownFacilityFill", "TownStreetlight", "TownFirefly"};
    for (uint n = 0; n < names.length(); ++n) {
        array<Object@>@ objects = scene.GetObjects(names[n]);
        for (uint i = 0; i < objects.length(); ++i) {
            Light@ light;
            if (!objects[i].GetComponent(@light)) continue;
            // 旧Stage1の背面平行光源と街灯の補助光は、影なしのフィルライト。
            bool shadowCapable = names[n] != "SceneLight" && names[n] != "TownAmbient" && names[n] != "HistoricalStreetFill" && names[n] != "HistoricalFactoryFill" && names[n] != "TownFirefly" && names[n] != "TownFacilityFill";
            light.SetCastShadows(shadowCapable && GetSettingBool("town.shadows", true));
            light.SetShadowMapResolution(uint(GetSettingInt("town.shadowResolution", 1024)));
        }
    }
    Object@ screen = scene.GetObject("TownScreen");
    if (screen !is null) {
        // GameDefaultObjects/MainScreenの有効状態を基準に一括切替する。
        // SSAOはGTAOとの二重適用を避け、プレハブ既定の無効状態を維持する。
        bool enabled = GetSettingBool("town.postEffects", true);
        GTAOEffect@ gtao; SSAOEffect@ ssao; FXAAEffect@ fxaa;
        MotionBlurEffect@ motion; DepthOfFieldEffect@ dof;
        ChromaticAberrationEffect@ chromatic; VignetteEffect@ vignette;
        if (screen.GetComponent(@gtao)) gtao.SetActive(enabled);
        if (screen.GetComponent(@ssao)) ssao.SetActive(false);
        if (screen.GetComponent(@fxaa)) fxaa.SetActive(enabled);
        if (screen.GetComponent(@motion)) motion.SetActive(enabled);
        if (screen.GetComponent(@dof)) dof.SetActive(enabled);
        if (screen.GetComponent(@chromatic)) chromatic.SetActive(enabled);
        if (screen.GetComponent(@vignette)) vignette.SetActive(enabled);
        BloomEffect@ bloom;
        if (screen.GetComponent(@bloom)) bloom.SetActive(enabled);
        ColorAdjustEffect@ adjust;
        if (screen.GetComponent(@adjust)) {
            // 明るさはポストエフェクト切替と独立した画面設定。
            adjust.SetActive(true); adjust.SetBrightness(GetSettingFloat("town.brightness", 0.1f));
        }
    }
}

class TownSlider {
    UIButton@ input;
    Object@ thumb;
    TextRenderer@ valueLabel;
    float value = 0;
    float y = 0;
    bool changed = false;
    void Create(TownUI@ ui, float rowY, float initial) {
        y = rowY; value = initial;
        Object@ track = ui.Rect(760, y, 360, 26, Vector4(0.2f, 0.3f, 0.4f, 1));
        track.AddComponent(@input); input.SetDisplayCameraObject(ui.camera);
        @thumb = ui.Rect(580 + value * 360, y, 12, 34, Vector4(0.4f, 0.9f, 1, 1));
        @valueLabel = ui.Text("", 980, y - 12, 22);
        Refresh();
    }
    void Refresh() {
        thumb.GetTransform().SetTranslate(Vector3(580 + value * 360, 720 - y, 0));
        valueLabel.SetText("" + int(value * 100 + 0.5f) + "%");
    }
    void Update() {
        changed = false;
        if (!input.IsPressed() && !input.IsClicked()) return;
        Vector2 uv;
        if (!input.TryGetLocalHoverPosition(uv)) return;
        float next = uv.x < 0 ? 0 : (uv.x > 1 ? 1 : uv.x);
        if (next != value) { value = next; changed = true; Refresh(); }
    }
}

class TownSettingsMenu {
    TownUI@ ui = TownUI();
    TownSlider@ se = TownSlider();
    TownSlider@ bgm = TownSlider();
    TownSlider@ brightness = TownSlider();
    TextRenderer@ heading; TextRenderer@ seLabel; TextRenderer@ bgmLabel; TextRenderer@ brightnessLabel;
    TextRenderer@ help;
    Scene@ scene;
    bool visible = false;
    bool languageChanged = false;
    bool needsSceneRefresh = false;
    bool dirty = false;
    float flushClock = 0;
    void Initialize(Scene@ owner) {
        @scene = owner; ui.Initialize(owner); ui.layer = 110;
        ui.Rect(640, 360, 1280, 720, Vector4(0.025f, 0.05f, 0.09f, 0.68f));
        @heading = ui.Text("", 110, 42, 36);
        ui.Button("", 120, 132); // 言語
        @seLabel = ui.Text("", 120, 211, 24); se.Create(ui, 228, GetSettingFloat("town.seVolume", 0.8f));
        @bgmLabel = ui.Text("", 120, 291, 24); bgm.Create(ui, 308, GetSettingFloat("town.bgmVolume", 0.6f));
        ui.Button("", 120, 380); // 影
        ui.Button("", 580, 380); // 解像度
        ui.Button("", 120, 460); // ポストエフェクト
        @brightnessLabel = ui.Text("", 120, 535, 24);
        brightness.Create(ui, 552, (GetSettingFloat("town.brightness", 0.1f) + 0.4f) / 0.8f);
        ui.Button("", 120, 638); // 戻る
        @help = ui.Text("", 580, 640, 18);
        Refresh(); ui.Show(false);
    }
    void Refresh() {
        heading.SetText(TownTr("設定", "Settings"));
        ui.buttonTexts[0].SetText(TownTr("表示言語：日本語  [切替]", "Language: English  [Change]"));
        seLabel.SetText(TownTr("効果音の音量", "Sound effects volume"));
        bgmLabel.SetText(TownTr("BGMの音量", "Music volume"));
        brightnessLabel.SetText(TownTr("画面の明るさ", "Screen brightness"));
        ui.buttonTexts[1].SetText(TownTr("ライトの影：", "Light shadows: ") + (GetSettingBool("town.shadows", true) ? "ON" : "OFF"));
        ui.buttonTexts[2].SetText(TownTr("影の解像度：", "Shadow resolution: ") + GetSettingInt("town.shadowResolution", 1024) + "  [切替 / Change]");
        ui.buttonTexts[3].SetText(TownTr("ポストエフェクト：", "Post effects: ") + (GetSettingBool("town.postEffects", true) ? "ON" : "OFF"));
        ui.buttonTexts[4].SetText(TownTr("戻る [Esc]", "Back [Esc]"));
        help.SetText(TownTr("スライダーをドラッグして調整 / 自動保存", "Drag sliders to adjust / Saved automatically"));
    }
    void Open() { visible = true; languageChanged = false; needsSceneRefresh = false; ui.Show(true); Refresh(); }
    void Flush() {
        if (!dirty) return;
        SetSettingFloat("town.seVolume", se.value); SetSettingFloat("town.bgmVolume", bgm.value);
        SetSettingFloat("town.brightness", brightness.value * 0.8f - 0.4f);
        dirty = false;
    }
    void Update() {
        if (!visible) return;
        languageChanged = false;
        int click = ui.Clicked();
        if (click == 0) { SetCurrentLanguage(GetCurrentLanguage() == "ja-JP" ? "en-US" : "ja-JP"); languageChanged = true; needsSceneRefresh = true; }
        if (click == 1) SetSettingBool("town.shadows", !GetSettingBool("town.shadows", true));
        if (click == 2) {
            int resolution = GetSettingInt("town.shadowResolution", 1024);
            SetSettingInt("town.shadowResolution", resolution >= 2048 ? 512 : resolution * 2);
        }
        if (click == 3) SetSettingBool("town.postEffects", !GetSettingBool("town.postEffects", true));
        se.Update(); bgm.Update(); brightness.Update();
        if (se.changed || bgm.changed || brightness.changed) {
            dirty = true;
            SetCategoryVolume("SE", se.value); SetCategoryVolume("BGM", bgm.value);
            ColorAdjustEffect@ adjust; scene.GetObject("TownScreen").GetComponent(@adjust);
            if (adjust !is null) adjust.SetBrightness(brightness.value * 0.8f - 0.4f);
        }
        flushClock += GetDeltaTime();
        if (flushClock >= 0.3f) { Flush(); flushClock = 0; }
        if (click >= 0 && click < 4) { TownApplySettings(scene); Refresh(); }
        if (click == 4 || IsCommandTriggered("Cancel")) { Flush(); visible = false; ui.Show(false); }
    }
}
