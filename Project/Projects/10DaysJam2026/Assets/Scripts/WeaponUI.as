#include "WeaponList.as"

class WeaponUI : ScriptComponentBehavior {
    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    // [SerializeField, Tooltip("武器テクスチャ名前リスト")]
    // array<string@>@ textureNames;

    // 現在のテクスチャ
    TextureSource@ currentTexture;

    WeaponList currentWeaponType;

    void Start() {
        GetComponent(@currentTexture);
    }

    void Update() {
        //ScriptComponent@ sc;
        //if(player.GetComponent(@sc)){
        //    if(sc.GetVariable("currentWeaponType", currentWeaponType)){
        //        @currentTexture.SetTextureAssetPath(textureNames[currentWeaponType]);
        //    }
        //}
    }

    void End() {
    }
}
