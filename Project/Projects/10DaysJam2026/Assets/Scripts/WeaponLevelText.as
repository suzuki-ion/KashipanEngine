class WeaponLevelText : ScriptComponentBehavior {
    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    BitmapTextRenderer@ text;

    void Start() {
        GetComponent(@text);
    }

    void Update() {
        if (player is null || text is null) return;

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
                                
                                // 現在装備中の武器オブジェクトからレベルを取得
                                if (currentWeapon.GetComponent(@weaponSc)) {
                                    int level = 1;
                                    if (weaponSc.GetVariable("level", level)) {
                                        text.SetText("Lv." + level);
                                        return; // 成功したら処理を終了
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 武器を未所持の場合は空白
        text.SetText("");
    }

    void End() {
    }
}