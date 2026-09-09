// 起動時に保存済みのブラウン管風演出設定(video.crtEffect)を、自身と同じオブジェクトが持つ
// BarrelDistortionEffect/ScanlineEffectへ適用するスクリプト。
// マスター/BGM/SE音量やフルスクリーン/解像度はTitleScene起動時(TitleMenuController)に
// プロセス全体へ適用されたまま維持されるが、ポストエフェクトのisActiveはScreenBufferの
// インスタンスごと(=シーンごと)に保持されるため、ゲームシーン側でも改めて適用する必要がある。
//
// 前提（エディター側で設定が必要）:
//   - このスクリプトは、BarrelDistortionEffect/ScanlineEffectを持つ最終合成用ScreenBufferオブジェクト
//     (FinalScreen)自身に付与しておくこと

class ApplyVideoSettings : ScriptComponentBehavior {
    void Start() {
        bool crtEffect = GetSettingBool("video.crtEffect", false);

        BarrelDistortionEffect@ barrel;
        if (GetOwnerObject().GetComponent(@barrel)) barrel.SetActive(crtEffect);

        ScanlineEffect@ scanline;
        if (GetOwnerObject().GetComponent(@scanline)) scanline.SetActive(crtEffect);
    }
}
