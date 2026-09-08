// ゲームプレイ上の判定基準にするカメラの位置・表示サイズを、シーン変数として毎フレーム公開する。
// 敵の非表示処理等、複数のスクリプトから同じカメラ情報を参照したい場合に、
// Object@ camera のようなシリアライズフィールドで個別に受け取る代わりにこちらを読む。
// (Prefab化されたスクリプトがシーン固有のカメラオブジェクトを直接参照すると、
//  他シーンに配置した際にUUIDが一致せず参照が解決できなくなるため)
//
// 使い方: Camera2Dを持つオブジェクト(例: GameScreenCamera)にこのスクリプトをアタッチする。
// 参照側は GetScene().GetVariable("gameplayCameraPos", camPos) /
// GetScene().GetVariable("gameplayCameraSize", camSize) で読む。
class GameplayCameraPublisher : ScriptComponentBehavior {
    void Update() {
        Transform@ tf = GetTransform();
        Camera2D@ cam2d;
        if (tf is null || !GetComponent(@cam2d)) return;

        GetScene().SetVariable("gameplayCameraPos", tf.GetTranslate());
        GetScene().SetVariable("gameplayCameraSize", Vector2(cam2d.GetWidth(), cam2d.GetHeight()));
    }
}
