// プレイヤーの移動方向の先へ追従オブジェクト（カメラの狙い位置など）を配置する
class PlayerMovementTarget {
    Player@ owner;

    PlayerMovementTarget(Player@ inOwner) {
        @owner = inOwner;
    }

    // 指定オブジェクトを、プレイヤーの現在位置を基準に水平移動速度の分だけ進行方向へ配置する。
    // 速度が大きいほど movementTargetDistanceMultiplier に比例して距離が広がる
    void Update(PlayerMovement@ movement) {
        if (owner.movementTargetObject is null) return;

        Transform@ playerTransform = GetTransform();
        Transform@ targetTransform;
        if (playerTransform is null || !owner.movementTargetObject.GetComponent(@targetTransform)) return;

        Vector3 horizontalVelocity = movement.GetHorizontalVelocity();

        const Vector3 desiredWorldPosition =
            playerTransform.GetWorldPosition()
            + owner.movementTargetOffset
            + horizontalVelocity * owner.movementTargetDistanceMultiplier;
        const Vector3 targetWorldDelta = desiredWorldPosition - targetTransform.GetWorldPosition();
        targetTransform.SetTranslate(targetTransform.GetTranslate() + targetWorldDelta);
    }
}
