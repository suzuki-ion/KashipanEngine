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

float DitherBlueNoise(float2 screenPos, float objectSeed, bool enableTemporalDither, float distanceFromCamera, float depthBucketSize) {
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

	int2 ipos = (int2(screenPos) + int2(seedShiftX + frameShiftX + depthShiftX, seedShiftY + frameShiftY + depthShiftY)) & int(kBlueNoiseSize - 1);
	uint idx = uint(ipos.x) + uint(ipos.y) * kBlueNoiseSize;
	return gBlueNoiseDither[idx];
}
