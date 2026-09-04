// マットキャップ・2ndマットキャップ（ビュー空間法線からサンプルする後乗せレイヤー）。
// matcap(2nd)Intensity=0（既定）で無効。matcap(2nd)TextureIndexはextraParametersでTextureRef型として設定する。
// blendModeは0=Add/1=Multiply/2=Screen/3=Replace
float matcapIntensity; // @Range(0, 2, 0.01)
float matcapBlendMode; // @Range(0, 3, 1)
float matcap2ndIntensity; // @Range(0, 2, 0.01)
float matcap2ndBlendMode; // @Range(0, 3, 1)
// バインドレステクスチャ配列（gTextures[]）内でのマットキャップ・2ndマットキャップのインデックス
uint matcapTextureIndex;
uint matcap2ndTextureIndex;
