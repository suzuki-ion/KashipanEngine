class TextUpdate : ScriptComponentBehavior {
    void Update() {
        TextRenderer@ textRenderer;
        if (!GetComponent(@textRenderer)) return;

        Object@ player = GetScene().GetObject("Player");
        if (player is null) return;

        Transform@ playerTransform;
        if (!player.GetComponent(@playerTransform)) return;

        textRenderer.SetText("プレイヤーの座標：" + playerTransform.GetWorldPosition().x + ", " + playerTransform.GetWorldPosition().y + ", " + playerTransform.GetWorldPosition().z);
    }
}