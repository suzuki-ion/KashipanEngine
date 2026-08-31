// バックライト（光源が背後にあるときに縁を光らせる）。backlightIntensity=0（既定）で無効
float4 backlightColor; // @Color
float backlightPower; // @Range(1, 20, 0.5)
float backlightIntensity; // @Range(0, 5, 0.01)
