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
               // -1は未所持(武器なし)を表すのでテクスチャ更新をスキップ
               if (currentWeaponType >= 0 && textureNames !is null && uint(currentWeaponType) < textureNames.length()) {
                   string textureName = "App/Sprite/UI/" + textureNames[currentWeaponType] + ".png";
                   currentTexture.SetTextureAssetPath(textureName);
               } else {
                   currentTexture.SetTextureAssetPath("");
               }
           }
        }
    }

    void End() {
    }
}
