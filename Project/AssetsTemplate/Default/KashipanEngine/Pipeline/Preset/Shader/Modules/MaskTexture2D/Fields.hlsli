// マスクテクスチャ（グレースケール想定の2枚目のテクスチャでアルファを間引く）。maskIntensity=0（既定）で無効
float maskIntensity; // @Range(0, 1, 0.01)
// バインドレステクスチャ配列（gTextures[]）内でのマスクテクスチャのインデックス。
// extraParametersで"maskTextureIndex"というTextureRef型パラメータとして設定する
uint maskTextureIndex;
