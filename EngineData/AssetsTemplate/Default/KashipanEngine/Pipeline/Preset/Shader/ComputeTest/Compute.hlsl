// 読み込みとコンパイルのテスト用の最小構成のコンピュートシェーダー
// リソースを使わず処理も行わない。コンパイルに成功し、有効な PSO を作成できることを確認する。

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 何もしない
}
