// グラデーションカラー（陰影度をU座標としてgTextures[mat.gradationTextureIndex]をサンプルし、乗算で合成する）。
// gradationBlend=0（既定）で無効。gradationTextureIndexはextraParametersでTextureRef型として設定する
float gradationBlend; // @Range(0, 1, 0.01)
// バインドレステクスチャ配列（gTextures[]）内でのグラデーションテクスチャのインデックス
uint gradationTextureIndex;
