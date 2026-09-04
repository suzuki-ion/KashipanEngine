#include "WeaponList.as"

class WeaponUI : ScriptComponentBehavior {
    [SerializeField, Tooltip("プレイヤー")]
    Object@ player;

    [SerializeField, Tooltip("武器テクスチャ名前リスト")]
    array<string>@ textureNames;

    // 現在のテクスチャ
    TextureSource@ currentTexture;

    int currentWeaponType;

    void Start() {
        GetComponent(@currentTexture);
    }

    void Update() {
        ScriptComponent@ sc;
        if(player.GetComponent(@sc)){
           if(sc.GetVariable("currentWeaponType", currentWeaponType)){
               string textureName = "App/Sprite/UI/" + textureNames[currentWeaponType] + ".png";
               currentTexture.SetTextureAssetPath(textureName);
           }
        }
    }

    void End() {
    }
}
