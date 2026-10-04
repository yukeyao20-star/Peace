// 地形雪盖着色器（积雪视觉，参照 MC 雪层只盖顶面的观感）
//
// 顶点色仍作底色（COLOR），按 snow_amount 在合适的位置混入积雪：
//   * 坡度门控：只有法线朝上的缓坡积雪，陡崖/岩壁保持裸露（MC 雪不在侧壁）；
//   * 高度门控：水线以下的湖床不积雪（雪落进水里就化了）；
//   * 覆盖噪声：低频决定成片推进、高频加碎雪颗粒，雪线随雪量自然生长；
//   * 粗糙度随积雪降低 —— 雪面更亮更"脆"，日光下有一层冷高光。
// snow_amount 由 TerrainBuilder 每帧从 SkyCycle 的雪强因子写入（无雪时为 0，
// 着色器完全回到原顶点色外观，与旧 StandardMaterial3D 观感一致）。

shader_type spatial;

uniform float snow_amount : hint_range(0.0, 1.0) = 0.0;
uniform vec3 snow_color : source_color = vec3(0.93, 0.95, 0.99);
uniform float snow_water_line = -0.55; // 水面海拔（TerrainBuilder 生成时写入）

varying vec3 w_normal; // 世界空间法线
varying vec3 w_pos;    // 世界空间坐标

float vhash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float vnoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(vhash(i), vhash(i + vec2(1.0, 0.0)), u.x),
			mix(vhash(i + vec2(0.0, 1.0)), vhash(i + vec2(1.0, 1.0)), u.x), u.y);
}

void vertex() {
	w_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
	w_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec3 base = COLOR.rgb;
	// 草丝微纹理：双频噪点斑驳 + 双向细条纹（交叉草纤维——草叶间的"地"也是草质感，
	// 与真草叶叠出羊毛连续感）；只染草地区域，随相机距离淡出（>80m 全淡）防闪烁
	float green_m = smoothstep(0.05, 0.18, COLOR.g - max(COLOR.r, COLOR.b));
	if (green_m > 0.001) {
		float n1 = vnoise(w_pos.xz * 9.0);
		float n2 = vnoise(w_pos.xz * 26.0 + 13.1);
		float warp = vnoise(w_pos.xz * 3.5) * 6.0;
		float blade_a = sin(w_pos.x * 34.0 + w_pos.z * 27.0 + warp) * 0.5 + 0.5;
		float blade_b = sin(w_pos.x * -29.0 + w_pos.z * 36.0 + warp * 1.3) * 0.5 + 0.5;
		float dist_f = clamp(1.0 - length(CAMERA_POSITION_WORLD - w_pos) / 80.0, 0.0, 1.0);
		float tex = ((n1 - 0.5) * 0.14 + (n2 - 0.5) * 0.12
				+ (blade_a - 0.5) * 0.09 + (blade_b - 0.5) * 0.08) * dist_f;
		base *= 1.0 + tex * green_m;
	}
	float up = clamp(normalize(w_normal).y, 0.0, 1.0);
	float slope_m = smoothstep(0.60, 0.80, up); // 缓坡才积雪
	// 水线上方 0.06~0.38m 渐入积雪带（湿沙圈不积雪，与湖岸湿沙带衔接）
	float height_m = smoothstep(snow_water_line + 0.06, snow_water_line + 0.38, w_pos.y);
	// 覆盖噪声：低频成片 + 高频颗粒；阈值 = 1-雪量，雪量越大覆盖越广
	float cov_n = vnoise(w_pos.xz * 0.45 + vec2(7.3, 2.9)) * 0.72
			+ vnoise(w_pos.xz * 2.2) * 0.28;
	float cov = smoothstep(1.0 - snow_amount - 0.10, 1.0 - snow_amount + 0.10, cov_n)
			* slope_m * height_m * snow_amount;

	// 高山常年积雪（山顶雪盖）：海拔高于 13.5m 的高山地带自然凝结常年白雪，
	// 随海拔升高覆盖越厚，在山峰与山脊处形成巍峨壮丽的雪山胜景（陡峭崖壁不易挂雪，平缓山脊与山顶纯白如玉）
	float mountain_snow_edge = 14.5 + (cov_n - 0.5) * 4.5; // 随噪声自然参差生长的雪线
	float peak_snow = smoothstep(mountain_snow_edge - 1.5, mountain_snow_edge + 4.5, w_pos.y);
	float peak_slope = smoothstep(0.42, 0.72, up); // 稍微陡一点的山肩也能附着轻微冰雪
	float mountain_cov = peak_snow * peak_slope;
	cov = max(cov, mountain_cov);

	// 细颗粒亮度扰动，防大面积纯色死白
	float grain = vnoise(w_pos.xz * 5.0) - 0.5;
	vec3 snow = snow_color * (1.0 + grain * 0.05);
	ALBEDO = mix(base, snow, cov);
	ROUGHNESS = mix(1.0, 0.45, cov);
	METALLIC = 0.0;
}
