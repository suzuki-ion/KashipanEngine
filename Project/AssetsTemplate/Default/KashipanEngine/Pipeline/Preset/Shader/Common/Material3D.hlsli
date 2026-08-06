// struct Materialの基本フィールド一覧（structの中身のみ、struct宣言自体はObjectPS.hlsl側で行う）。
// カスタムシェーダー向けの追加パラメータは、末尾のADDITIONAL_MATERIAL_FIELDSマーカー位置に
// シェーダーモジュール合成システム（Graphics/Pipeline/System/ShaderModuleComposer）経由で追加される。
// フィールド名が MaterialManager::Material::extraParameters（.matの"parameters"）のキーと一致していれば、
// C++側の対応コード変更なしに値が自動的にここへ渡る（対応する型は float/float2/float3/float4/float4x4/int/uint/bool）。
float enableLighting;
float enableEnvironmentMapping;
float enableShadowMapProjection;
float useTexture;
float4 color;
float4x4 uvTransform;
float shininess;
float4 specularColor;
float environmentCoefficient;
// リムライト（ライト方向を考慮した逆光縁取り）。rimIntensity=0（既定）の場合は無効
float4 rimColor;
float rimPower;
float rimIntensity;
// 法線マップ使用フラグ（0=未使用）。gNormalMapはRGBが接空間の法線[0,1]エンコード
float useNormalMap;
// オブジェクト単位の色（MeshRendererのInstance Color）。マテリアル本体（共有アセット）とは別に、
// インスタンス（描画エントリ）ごとに異なる値を持つ
float4 instanceColor;
// instanceColorの適用方法。0=Override（置き換え）, 1=Multiply（乗算）, 2=Add（加算）, 3=Subtract（減算）
float instanceColorBlendMode;
