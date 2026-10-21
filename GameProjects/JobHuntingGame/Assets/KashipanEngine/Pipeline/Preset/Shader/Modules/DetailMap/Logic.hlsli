// ディテールマップ: UVをdetailTiling倍して重ね貼りする（グレー=無変化のディテール乗算）。
// mat.detailTilingが0以下なら既定値4.0を使う（COMPOSITE_HOOKSの中でAOより先、優先度2で呼ぶこと）。
// テクスチャ本体はgTextures[]（バインドレステクスチャ配列）からmat.detailAlbedoTextureIndexで参照する
void ApplyDetailMap(inout float4 outputColor, Material mat, float2 uv) {
    if (mat.detailBlend <= 0.0001f) return;
    float tiling = (mat.detailTiling > 0.0001f) ? mat.detailTiling : 4.0f;
    float3 detail = gTextures[mat.detailAlbedoTextureIndex].Sample(gSamplers[mat.samplerIndex], uv * tiling).rgb;
    outputColor.rgb = lerp(outputColor.rgb, outputColor.rgb * detail * 2.0f, mat.detailBlend);
}
