// ノーマルマップ使用フラグ（0=未使用、既定）。gNormalMapはRGBが接空間の法線[0,1]エンコード。
float useNormalMap;
// バインドレステクスチャ配列（gTextures[]）内でのノーマルマップのインデックス。
// extraParametersで"normalMapTextureIndex"というTextureRef型パラメータとして設定する
uint normalMapTextureIndex;
