// ディゾルブ（ノイズに基づく消失＋エッジカラー）。dissolveThreshold=0（既定）で無効
float dissolveThreshold; // @Range(0, 1, 0.01)
float4 dissolveEdgeColor; // @Color
float dissolveEdgeWidth; // @Range(0, 0.5, 0.01)
float dissolveNoiseScale; // @Range(0.1, 50, 0.1)
