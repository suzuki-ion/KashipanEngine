#include "TownPresentation.as"
#include "TownLanguage.as"

// クリア時は街の更新・入力を停止し、選択するまで中央に結果を表示する。
class TownClearMenu {
    TownUI@ ui = TownUI();
    TextRenderer@ notice;
    bool visible = false;
    void Initialize(Scene@ scene) {
        ui.Initialize(scene); ui.layer = 210;
        ui.Rect(640, 360, 1280, 720, Vector4(0, .015f, .025f, .72f));
        ui.layer = 212;
        ui.Rect(640, 360, 780, 350, Vector4(.035f, .13f, .16f, .98f));
        TextRenderer@ heading = ui.Text("CLEAR!", 640, 218, 64);
        heading.SetHorizontalAlign(TextHorizontalAlign::Center);
        heading.SetColor(Vector4(1, .88f, .35f, 1));
        TextRenderer@ message = ui.Text(TownTr("光の塔が完成しました！", "The Tower of Light is complete!"), 640, 310, 29);
        message.SetHorizontalAlign(TextHorizontalAlign::Center);
        @notice = ui.Text(TownTr("このまま街づくりを続けられます。", "You can keep building your town."), 640, 365, 20);
        notice.SetHorizontalAlign(TextHorizontalAlign::Center);
        ui.Button(TownTr("ゲームを続ける", "Keep playing"), 425, 445);
        ui.Button(TownTr("保存して終了", "Save and quit"), 735, 445);
        for(uint i=0;i<ui.buttonTexts.length();++i)ui.buttonTexts[i].SetFontSize(26);
        ui.Show(false);
    }
    void Show(bool show) { visible = show; ui.Show(show); }
    int Clicked() { return ui.Clicked(); }
    void SaveFailed() { notice.SetText(TownTr("保存できませんでした。もう一度選択してください。", "Save failed. Please choose again.")); }
}
