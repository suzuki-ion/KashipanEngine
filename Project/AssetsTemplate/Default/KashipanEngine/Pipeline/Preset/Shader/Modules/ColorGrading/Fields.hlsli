// 色調整（最終色にHSV/ガンマ補正を適用）。saturation/brightness/gammaは1.0を基準とした差分値（既定0で無変化）
float hueShift; // @Range(-180, 180, 1)
float saturation; // @Range(-1, 1, 0.01)
float brightness; // @Range(-1, 1, 0.01)
float gamma; // @Range(-0.9, 2, 0.01)
