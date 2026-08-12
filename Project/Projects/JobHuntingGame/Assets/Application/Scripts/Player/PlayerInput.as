// プレイヤーの入力読み取りを担当する
class PlayerInput {
    float moveDirection = 0.0f;
    bool jumpTriggered = false;

    void Update() {
        // 左右移動入力
        moveDirection = 0.0f;
        if (IsCommandTriggered("PlayerMoveLeft")) {
            moveDirection = GetCommandValue("PlayerMoveLeft");
        }
        if (IsCommandTriggered("PlayerMoveRight")) {
            moveDirection = GetCommandValue("PlayerMoveRight");
        }
        // ジャンプ入力
        jumpTriggered = IsCommandTriggered("PlayerJump");
    }
}
