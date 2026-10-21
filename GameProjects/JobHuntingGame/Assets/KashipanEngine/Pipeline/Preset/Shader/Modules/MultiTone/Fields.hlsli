// 多段影（ObjectToonパイプラインでのみ有効）。toneCount<2.5で2階調、それ以外で3階調
float toneCount; // @Range(2, 3, 1)
float shadowThreshold; // @Range(0, 1, 0.01)
float midThreshold; // @Range(0, 1, 0.01)
float bandSoftness; // @Range(0, 0.2, 0.01)
