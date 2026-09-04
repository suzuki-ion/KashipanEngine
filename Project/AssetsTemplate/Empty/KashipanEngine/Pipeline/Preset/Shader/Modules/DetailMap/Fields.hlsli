// ディテールアルベドマップ（別UVスケールで重ね貼りする2枚目のテクスチャ）。detailBlend=0（既定）で無効
float detailBlend; // @Range(0, 1, 0.01)
float detailTiling; // @Range(1, 32, 0.5)
// バインドレステクスチャ配列（gTextures[]）内でのディテールアルベドマップのインデックス。
// extraParametersで"detailAlbedoTextureIndex"というTextureRef型パラメータとして設定する
uint detailAlbedoTextureIndex;
