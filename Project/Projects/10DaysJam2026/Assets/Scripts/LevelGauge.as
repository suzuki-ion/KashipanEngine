class LevelGauge : ScriptComponentBehavior {
    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    [SerializeField, Tooltip("ゲージ満タン時のスケールX")]
    float maxScaleX = 26.0f;

    Transform@ tf;

    void Start() {
        @tf = GetTransform();
    }

    void Update() {
        if (player is null || tf is null) return;

        float ratio = 0.0f;

        ScriptComponent@ playerSc;
        if (player.GetComponent(@playerSc)) {
            int currentWeaponType = -1;

            // プレイヤーから現在の武器タイプを取得
            if (playerSc.GetVariable("currentWeaponType", currentWeaponType)) {

                // -1以上なら武器を装備している状態
                if (currentWeaponType >= 0) {
                    array<Object@>@ weapons;

                    // プレイヤーが保持している武器の配列を取得
                    if (playerSc.GetVariable("weapons", @weapons)) {
                        if (weapons !is null && uint(currentWeaponType) < weapons.length()) {
                            Object@ currentWeapon = weapons[currentWeaponType];

                            if (currentWeapon !is null) {
                                ScriptComponent@ weaponSc;

                                // 現在装備中の武器オブジェクトから経験値を取得
                                if (currentWeapon.GetComponent(@weaponSc)) {
                                    float exp = 0.0f;
                                    float nextExp = 1.0f;
                                    if (weaponSc.GetVariable("exp", exp) && weaponSc.GetVariable("nextExp", nextExp)) {
                                        if (nextExp > 0.0f) {
                                            ratio = exp / nextExp;
                                            ratio = Clamp(ratio, 0.0f, 1.0f);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 現在のスケールを保持しつつXだけを経験値に応じて更新
        Vector3 scale = tf.GetScale();
        scale.x = maxScaleX * ratio;
        tf.SetScale(scale);
    }

    void End() {
    }
}
