// 発光（ライティング非依存で常時加算）。emissionIntensity=0（既定）で無効。
// emissionTextureIndexはextraParametersでTextureRef型として設定する
float4 emissionColor; // @Color
float emissionIntensity; // @Range(0, 5, 0.01)
// バインドレステクスチャ配列（gTextures[]）内での発光テクスチャのインデックス
uint emissionTextureIndex;
