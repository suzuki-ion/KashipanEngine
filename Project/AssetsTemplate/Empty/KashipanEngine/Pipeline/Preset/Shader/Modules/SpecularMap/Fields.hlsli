// スペキュラー/グロスマップ。specularMapIntensity=0（既定）で無効
float specularMapIntensity; // @Range(0, 1, 0.01)
// バインドレステクスチャ配列（gTextures[]）内でのスペキュラー/グロスマップのインデックス。
// extraParametersで"specularTextureIndex"というTextureRef型パラメータとして設定する
uint specularTextureIndex;
