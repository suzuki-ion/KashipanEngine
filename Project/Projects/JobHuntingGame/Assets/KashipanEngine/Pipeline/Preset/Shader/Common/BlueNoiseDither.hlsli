#pragma once
#include "Time.hlsli"

// ブルーノイズによるディザ閾値テーブル（BlueNoiseGeneratorがコンピュートシェーダーで
// 起動時に1回だけ生成し、全描画先・全パイプラインで共通のバッファとしてバインドされる）
StructuredBuffer<float> gBlueNoiseDither : register(t30);

float HashToUnit(float x) {
	return frac(sin(x * 12.9898f) * 43758.5453f);
}

// ブルーノイズテーブルの1辺のサイズ（BlueNoiseGenerator::kSizeと必ず一致させること）
static const uint kBlueNoiseSize = 64;

// Bayer行列（16x16、B_2=[0,2;3,1]を起点に4倍ずつ再帰的に拡張して生成）を(値+0.5)/256で
// 正規化した閾値テーブル。マテリアルのuseBayerDitherがtrueの場合、gBlueNoiseDitherの代わりに
// こちらを参照する。ブルーノイズと違い軸に沿った規則的な模様（斜めのクロスハッチ）が見えるが、
// テクスチャ/構造化バッファのサンプルが不要でわずかに軽い。周期は16ピクセルなので、
// 大きな面ではタイルの繰り返しが視認できる点に注意
static const uint kBayerSize = 16;
static const float kBayerMatrix16x16[256] = {
	0.001953f, 0.501953f, 0.126953f, 0.626953f, 0.033203f, 0.533203f, 0.158203f, 0.658203f,
	0.009766f, 0.509766f, 0.134766f, 0.634766f, 0.041016f, 0.541016f, 0.166016f, 0.666016f,
	0.751953f, 0.251953f, 0.876953f, 0.376953f, 0.783203f, 0.283203f, 0.908203f, 0.408203f,
	0.759766f, 0.259766f, 0.884766f, 0.384766f, 0.791016f, 0.291016f, 0.916016f, 0.416016f,
	0.189453f, 0.689453f, 0.064453f, 0.564453f, 0.220703f, 0.720703f, 0.095703f, 0.595703f,
	0.197266f, 0.697266f, 0.072266f, 0.572266f, 0.228516f, 0.728516f, 0.103516f, 0.603516f,
	0.939453f, 0.439453f, 0.814453f, 0.314453f, 0.970703f, 0.470703f, 0.845703f, 0.345703f,
	0.947266f, 0.447266f, 0.822266f, 0.322266f, 0.978516f, 0.478516f, 0.853516f, 0.353516f,
	0.048828f, 0.548828f, 0.173828f, 0.673828f, 0.017578f, 0.517578f, 0.142578f, 0.642578f,
	0.056641f, 0.556641f, 0.181641f, 0.681641f, 0.025391f, 0.525391f, 0.150391f, 0.650391f,
	0.798828f, 0.298828f, 0.923828f, 0.423828f, 0.767578f, 0.267578f, 0.892578f, 0.392578f,
	0.806641f, 0.306641f, 0.931641f, 0.431641f, 0.775391f, 0.275391f, 0.900391f, 0.400391f,
	0.236328f, 0.736328f, 0.111328f, 0.611328f, 0.205078f, 0.705078f, 0.080078f, 0.580078f,
	0.244141f, 0.744141f, 0.119141f, 0.619141f, 0.212891f, 0.712891f, 0.087891f, 0.587891f,
	0.986328f, 0.486328f, 0.861328f, 0.361328f, 0.955078f, 0.455078f, 0.830078f, 0.330078f,
	0.994141f, 0.494141f, 0.869141f, 0.369141f, 0.962891f, 0.462891f, 0.837891f, 0.337891f,
	0.013672f, 0.513672f, 0.138672f, 0.638672f, 0.044922f, 0.544922f, 0.169922f, 0.669922f,
	0.005859f, 0.505859f, 0.130859f, 0.630859f, 0.037109f, 0.537109f, 0.162109f, 0.662109f,
	0.763672f, 0.263672f, 0.888672f, 0.388672f, 0.794922f, 0.294922f, 0.919922f, 0.419922f,
	0.755859f, 0.255859f, 0.880859f, 0.380859f, 0.787109f, 0.287109f, 0.912109f, 0.412109f,
	0.201172f, 0.701172f, 0.076172f, 0.576172f, 0.232422f, 0.732422f, 0.107422f, 0.607422f,
	0.193359f, 0.693359f, 0.068359f, 0.568359f, 0.224609f, 0.724609f, 0.099609f, 0.599609f,
	0.951172f, 0.451172f, 0.826172f, 0.326172f, 0.982422f, 0.482422f, 0.857422f, 0.357422f,
	0.943359f, 0.443359f, 0.818359f, 0.318359f, 0.974609f, 0.474609f, 0.849609f, 0.349609f,
	0.060547f, 0.560547f, 0.185547f, 0.685547f, 0.029297f, 0.529297f, 0.154297f, 0.654297f,
	0.052734f, 0.552734f, 0.177734f, 0.677734f, 0.021484f, 0.521484f, 0.146484f, 0.646484f,
	0.810547f, 0.310547f, 0.935547f, 0.435547f, 0.779297f, 0.279297f, 0.904297f, 0.404297f,
	0.802734f, 0.302734f, 0.927734f, 0.427734f, 0.771484f, 0.271484f, 0.896484f, 0.396484f,
	0.248047f, 0.748047f, 0.123047f, 0.623047f, 0.216797f, 0.716797f, 0.091797f, 0.591797f,
	0.240234f, 0.740234f, 0.115234f, 0.615234f, 0.208984f, 0.708984f, 0.083984f, 0.583984f,
	0.998047f, 0.498047f, 0.873047f, 0.373047f, 0.966797f, 0.466797f, 0.841797f, 0.341797f,
	0.990234f, 0.490234f, 0.865234f, 0.365234f, 0.958984f, 0.458984f, 0.833984f, 0.333984f,
};

// 半透明ディザの閾値を計算する。マテリアルのuseBayerDitherに応じて、ブルーノイズテーブル
// （gBlueNoiseDither）とBayer行列（kBayerMatrix16x16）のどちらを参照するかを切り替える
float ComputeDitherThreshold(float2 screenPos, float objectSeed, bool enableTemporalDither, float distanceFromCamera, float depthBucketSize, bool useBayerDither) {
	// オブジェクト固有のシード値（EmptyObject::GetObjectID()由来、C++側でハッシュ済み。
	// ObjectVS.hlsl参照）をハッシュ化した値でサンプリング位置をずらす。シーン内で実質的に
	// 一意な値のため、同一アルファのオブジェクト同士が重なった際に閾値が一致し奥が完全に
	// 消えてしまう問題を緩和できる。オブジェクト単位で固定された値（画素ごとには変化しない）
	// なので、同一オブジェクトの表面内でディザパターンがばらついてノイズっぽく見えることもない。
	// x/yのシフトを同じハッシュ値から導出すると位相の組み合わせがkBlueNoiseSize通りしか
	// 作れないため、異なる種で2回ハッシュして独立させ、テーブルが許す最大kBlueNoiseSize^2通り
	// （64x64なら4096通り、約0.024%）まで位相の衝突確率を下げている。
	// 参照先はBlueNoiseGeneratorが起動時にコンピュートシェーダーで生成したブルーノイズテーブル
	// （gBlueNoiseDither）で、8x8 Bayer行列のような軸に沿った規則的な模様を持たない。
	// enableTemporalDither（マテリアルのbool値）がfalseの場合はここで終わりで、
	// 位相はオブジェクトのシードのみに基づく、時間に依存しない固定値になる
	int seedShiftX = int(HashToUnit(objectSeed) * float(kBlueNoiseSize));
	int seedShiftY = int(HashToUnit(objectSeed * 7.1907f) * float(kBlueNoiseSize));

	int frameShiftX = 0;
	int frameShiftY = 0;
	if (enableTemporalDither) {
		// フレームごとに位相を追加でシフトする。以前はフレーム番号をそのまま（1ずつ線形に）
		// 使っていたが、ブルーノイズテーブル上で隣接フレームが常に隣り合う位相を参照するため、
		// 模様が一定方向に「滑って見える」問題があった。gTimeから求めた擬似フレーム番号を
		// HashToUnitでハッシュして無相関に飛び散らせることで、隣接フレーム間の相関を断ち、
		// TVノイズ/フィルムグレインのような単なるちらつきに見せている。
		// 常時ちらつく値になるため、後段のTemporalBlendEffectで時間的に平均化することを前提とし、
		// マテリアル側でこのフラグがtrueの場合のみ有効にする
		uint frameCounter = uint(gTime * 60.0f) % 4096u;
		frameShiftX = int(HashToUnit(float(frameCounter) + 0.5f) * float(kBlueNoiseSize));
		frameShiftY = int(HashToUnit(float(frameCounter) * 3.371f + 0.5f) * float(kBlueNoiseSize));
	}

	int depthShiftX = 0;
	int depthShiftY = 0;
	if (depthBucketSize > 0.0f) {
		// 同一メッシュ内で奥行き方向に重なる層（フード付き服のフードと胴体など）を分離するための
		// 項。カメラ（またはシャドウマップの場合はライト）からのワールド距離（線形。NDC空間のZは
		// 近距離ほど広がる非線形カーブのため使わない）を、depthBucketSize刻みの粗いバケットに
		// 量子化してからハッシュする。量子化しているため、1つの連続した面の中ではバケットが
		// 変わらず位相は一定に保たれる（面内が荒れることはない）が、実際に奥行きの離れた層は
		// 別バケット＝別位相になり、同一idSeedのオブジェクトでも独立してディザされるようになる
		float depthBucket = floor(distanceFromCamera / depthBucketSize);
		depthShiftX = int(HashToUnit(depthBucket) * float(kBlueNoiseSize));
		depthShiftY = int(HashToUnit(depthBucket * 7.1907f) * float(kBlueNoiseSize));
	}

	int totalShiftX = seedShiftX + frameShiftX + depthShiftX;
	int totalShiftY = seedShiftY + frameShiftY + depthShiftY;

	if (useBayerDither) {
		// シフト量はブルーノイズテーブル基準（0〜63）で計算しているが、Bayer行列は周期16なので
		// 下位ビットだけを使う（周期が違うだけで、シフトによる位相分離＝オブジェクト/フレーム/
		// 奥行きごとに参照位置をずらして重なりの衝突を避けるという考え方自体は共通）
		int2 ipos = (int2(screenPos) + int2(totalShiftX, totalShiftY)) & int(kBayerSize - 1);
		uint idx = uint(ipos.x) + uint(ipos.y) * kBayerSize;
		return kBayerMatrix16x16[idx];
	}

	int2 ipos = (int2(screenPos) + int2(totalShiftX, totalShiftY)) & int(kBlueNoiseSize - 1);
	uint idx = uint(ipos.x) + uint(ipos.y) * kBlueNoiseSize;
	return gBlueNoiseDither[idx];
}
